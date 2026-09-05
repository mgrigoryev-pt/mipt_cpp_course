// Тесты общего владения и слабых ссылок. Занятие 4.3.
//
// Три темы, и все три про одно: кто чем владеет.
//
//   счётчик копий      атомарный, потому что его увеличивают из двух потоков;
//   родитель процесса  weak_ptr, потому что владеет записями словарь;
//   копия сработки     без улик, потому что улики никуда не уезжают.
//
// Ни один из тестов не запускает симулятор: проверяются структуры данных
// и то, что из них следует.

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "doctest.h"
#include "copy_stats.h"
#include "detection.h"
#include "entity_model.h"
#include "event.h"
#include "rules.h"

using nano_edr::Detection;
using nano_edr::EntityModel;
using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::EvictionLimits;
using nano_edr::Field;
using nano_edr::ProcessRecord;
using nano_edr::Severity;

namespace {

Event Make(const std::string& ts, const std::string& type,
           const std::string& pid, std::vector<Field> fields = {}) {
    EventParts parts;
    parts.ts = ts;
    parts.type = type;
    parts.pid = pid;
    parts.fields = std::move(fields);
    return Event(std::move(parts));
}

}  // namespace

// ---------------------------------------------------------------------------
// Счётчик копий
// ---------------------------------------------------------------------------

TEST_CASE("счётчик копий не теряет инкрементов при работе из двух потоков") {
    // До 4.3 счётчик был обычным uint64_t, и вот этот тест на нём падал бы —
    // не всегда, а примерно всегда, и на разное число.
    //
    // Ловится это не глазами: `++x` для неатомарного числа — три операции
    // (прочитать, прибавить, записать), и два потока успевают прочитать
    // одно и то же значение.
    nano_edr::ResetCopyStats();

    constexpr int kThreads = 4;
    constexpr int kPerThread = 10000;
    {
        std::vector<std::jthread> workers;
        for (int t = 0; t < kThreads; ++t) {
            workers.emplace_back([] {
                for (int i = 0; i < kPerThread; ++i) {
                    ++nano_edr::copy_stats().window_copied;
                }
            });
        }
    }

    CHECK(nano_edr::copy_stats().window_copied.get() ==
          static_cast<uint64_t>(kThreads) * kPerThread);
    nano_edr::ResetCopyStats();
}

TEST_CASE("счётчик сравнивается с числом и обнуляется") {
    // Выданные тесты занятия 3.3 пишут `== 0`, и обязаны продолжать
    // собираться: они проверяют move-семантику, а она никуда не делась.
    nano_edr::ResetCopyStats();
    CHECK(nano_edr::copy_stats().window_copied == 0);
    ++nano_edr::copy_stats().window_copied;
    CHECK(nano_edr::copy_stats().window_copied == 1);
    nano_edr::ResetCopyStats();
    CHECK(nano_edr::copy_stats().window_copied == 0);
}

// ---------------------------------------------------------------------------
// Слабая ссылка на родителя
// ---------------------------------------------------------------------------

TEST_CASE("цепочка предков строится, когда родитель известен") {
    EntityModel model;
    model.Observe(Make("1000", "process_start", "880",
                       {{"ppid", "4"}, {"image", "explorer.exe"}}));
    model.Observe(Make("2000", "process_start", "1042",
                       {{"ppid", "880"}, {"image", "cmd.exe"}}));

    const std::vector<std::string> chain = model.GetAncestorChain("1042");
    REQUIRE(chain.size() >= 2);
    CHECK(chain[0] == "1042");
    CHECK(chain[1] == "880");
}

TEST_CASE("родитель, узнанный позже ребёнка, всё равно находится") {
    // Ключевой случай, из-за которого поле parent нельзя заполнять
    // на событии старта: в phishing_macro cmd.exe стартует раньше, чем
    // модель впервые слышит про winword.
    EntityModel model;
    model.Observe(Make("2000", "process_start", "1042",
                       {{"ppid", "880"}, {"image", "cmd.exe"}}));
    // Про 880 модель узнаёт только теперь, и узнаёт настоящее время старта.
    model.Observe(Make("3000", "process_start", "880",
                       {{"ppid", "4"}, {"image", "winword.exe"}}));

    const std::vector<std::string> chain = model.GetAncestorChain("1042");
    REQUIRE(chain.size() >= 1);
    CHECK(chain[0] == "1042");
    // Родителя здесь нет и быть не должно: 880 стартовал ПОЗЖЕ ребёнка,
    // значит это другое воплощение номера, а не тот родитель.
    if (chain.size() > 1) {
        CHECK(chain[1] != "880");
    }
}

TEST_CASE("заглушка с нулевым стартом не запоминается как родитель") {
    // Запись про 880 заводится по упоминанию в чужом событии — с нулевым
    // временем старта. Если запомнить её слабой ссылкой, настоящий
    // process_start того же номера уже ничего не изменит, и цепочка
    // навсегда останется неправильной.
    EntityModel model;

    // 880 упомянут как родитель, своего события у него ещё не было.
    model.Observe(Make("2000", "process_start", "1042",
                       {{"ppid", "880"}, {"image", "cmd.exe"}}));
    // Спрашиваем цепочку — здесь и происходит разрешение родителя.
    const std::vector<std::string> before = model.GetAncestorChain("1042");
    REQUIRE_FALSE(before.empty());

    // Настоящее событие про 880, и стартовал он РАНЬШЕ ребёнка.
    model.Observe(Make("1000", "process_start", "880",
                       {{"ppid", "4"}, {"image", "winword.exe"}}));

    const std::vector<std::string> after = model.GetAncestorChain("1042");
    REQUIRE(after.size() >= 2);
    CHECK(after[0] == "1042");
    CHECK(after[1] == "880");
}

TEST_CASE("вытесненный родитель обрывает цепочку, а не портит её") {
    // Ради этого weak_ptr здесь и стоит. Сильная ссылка удержала бы
    // вытесненную запись в памяти — предел памяти превратился бы
    // в пожелание; голый указатель начал бы врать.
    EntityModel model;
    EvictionLimits limits;
    limits.max_processes = 2;
    limits.max_idle_ms = 0;  // только по объёму, чтобы случай был точным
    model.SetLimits(limits);

    model.Observe(Make("1000", "process_start", "880",
                       {{"ppid", "4"}, {"image", "explorer.exe"}}));
    model.Observe(Make("2000", "process_start", "1042",
                       {{"ppid", "880"}, {"image", "cmd.exe"}}));
    REQUIRE(model.GetAncestorChain("1042").size() >= 2);

    // Ещё процессы, чтобы предел сработал и старые записи ушли.
    for (int i = 0; i < 8; ++i) {
        model.Observe(Make(std::to_string(4000 + i * 100), "process_start",
                           std::to_string(2000 + i),
                           {{"ppid", "4"}, {"image", "svchost.exe"}}));
    }
    model.Evict();

    // Что именно осталось — дело вытеснения; важно, что ответ остался
    // осмысленным, а не превратился в чужой процесс.
    const std::vector<std::string> chain = model.GetAncestorChain("1042");
    for (const std::string& pid : chain) {
        CHECK(pid != "880");
    }
}

// ---------------------------------------------------------------------------
// Копия сработки без улик
// ---------------------------------------------------------------------------

TEST_CASE("WithoutEvidence копирует сообщение и оставляет улики") {
    Detection original;
    original.rule = "script_host_from_temp";
    original.severity = Severity::kCritical;
    original.ts = nano_edr::Timestamp{1730000000000};
    original.pid = "1101";
    original.chain = "880>1042";
    original.actions = {"kill_process", "quarantine_source"};
    original.evidence.push_back(
        std::make_unique<Event>(Make("1000", "process_start", "1101")));

    const Detection copy = nano_edr::WithoutEvidence(original);

    CHECK(copy.rule == original.rule);
    CHECK(copy.severity == original.severity);
    CHECK(copy.ts.ms == original.ts.ms);
    CHECK(copy.pid == original.pid);
    CHECK(copy.chain == original.chain);
    CHECK(copy.actions == original.actions);

    // Улики не уезжают наверх и остаются у оригинала.
    CHECK(copy.evidence.empty());
    CHECK(original.evidence.size() == 1);
}
