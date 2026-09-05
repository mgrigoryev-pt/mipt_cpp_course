// Тесты движка правил и полиморфных условий. Занятие 2.3.
//
// Тесты занятия 2.1 остаются подключёнными: событие, разбор, окно, обёртка
// над границей и четыре условия не изменились по поведению — условия стали
// наследниками интерфейса, но operator() у них остался.
//
// Тесты занятия 2.2 сняты: их предмет — Condition с переключателем и MatchRule
// с невиртуальным Check — это ровно то, что здесь заменено.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "conditions.h"
#include "doctest.h"
#include "event.h"
#include "event_source.h"
#include "match_rule.h"
#include "rule.h"
#include "rule_engine.h"
#include "rules.h"
#include "sequence_rule.h"

using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::EventType;
using nano_edr::AllOf;
using nano_edr::AnyOf;
using nano_edr::EventTypeIs;
using nano_edr::Field;
using nano_edr::FieldEndsWith;
using nano_edr::FieldEquals;
using nano_edr::ICondition;
using nano_edr::IRule;
using nano_edr::MatchRule;
using nano_edr::RuleEngine;
using nano_edr::SequenceRule;
using nano_edr::Severity;

namespace {

Event MakeEvent(const std::string& type, const std::string& pid,
                std::uint64_t ts_ms) {
    EventParts parts;
    parts.ts = std::to_string(ts_ms);
    parts.type = type;
    parts.pid = pid;
    parts.fields.push_back(Field{"image", "C:\\Windows\\System32\\wscript.exe"});
    return Event(parts);
}

}  // namespace

// ---------------------------------------------------------------------------
// EventType
// ---------------------------------------------------------------------------

TEST_CASE("тип события разбирается в перечисление") {
    CHECK(MakeEvent("process_start", "1", 100).event_type() ==
          EventType::kProcessStart);
    CHECK(MakeEvent("file_write", "1", 100).event_type() ==
          EventType::kFileWrite);
    CHECK(MakeEvent("net_connect", "1", 100).event_type() ==
          EventType::kNetConnect);
}

TEST_CASE("неизвестный тип — kOther, а не отказ") {
    // Так устроена граница: неизвестный тип доставляется как есть. Агент,
    // падающий на типе, которого не знал его автор, бесполезен — телеметрию
    // расширяют без него.
    const Event event = MakeEvent("quantum_teleport", "1", 100);

    CHECK(event.event_type() == EventType::kOther);
    // Строка при этом сохраняется: по kOther не поймёшь, что пришло.
    CHECK(event.type() == "quantum_teleport");
}

TEST_CASE("условие по типу события") {
    const Event start = MakeEvent("process_start", "1", 100);
    const Event write = MakeEvent("file_write", "1", 100);

    const EventTypeIs is_process({EventType::kProcessStart});
    CHECK(is_process.Matches(start));
    CHECK_FALSE(is_process.Matches(write));

    const EventTypeIs is_file({EventType::kFileWrite, EventType::kFileCreate});
    CHECK(is_file.Matches(write));
    CHECK_FALSE(is_file.Matches(start));
}

// ---------------------------------------------------------------------------
// Полиморфные условия
// ---------------------------------------------------------------------------

TEST_CASE("условия разных типов лежат в одном векторе") {
    // То, чего нельзя было сделать на занятии 2.2: вектор хранит указатели,
    // а указатель одного размера всегда.
    std::vector<std::unique_ptr<ICondition>> conditions;
    conditions.push_back(std::make_unique<FieldEquals>("type", "process_start"));
    conditions.push_back(std::make_unique<FieldEndsWith>("image", "wscript.exe"));
    conditions.push_back(
        std::make_unique<EventTypeIs>(std::vector<EventType>{
            EventType::kProcessStart}));

    const Event event = MakeEvent("process_start", "1", 100);
    std::size_t matched = 0;
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        if (conditions[i]->Matches(event)) {
            ++matched;
        }
    }

    CHECK(matched == 3);
}

TEST_CASE("условие остаётся функтором") {
    // operator() оставлен поверх Matches, чтобы код занятия 2.1 продолжал
    // компилироваться.
    const Event event = MakeEvent("process_start", "1", 100);
    const FieldEquals condition("type", "process_start");

    CHECK(condition(event));
    CHECK(condition.Matches(event));
}

TEST_CASE("у интерфейса условия виртуальный деструктор") {
    // Без него удаление через unique_ptr<ICondition> не вызвало бы деструктор
    // наследника, и строки внутри условия утекли бы. Проверка компиляторная;
    // саму утечку показывает санитайзер.
    CHECK(std::has_virtual_destructor_v<ICondition>);
}


// ---------------------------------------------------------------------------
// Условия из условий
// ---------------------------------------------------------------------------

TEST_CASE("AllOf требует всех, AnyOf — хотя бы одного") {
    const Event event = MakeEvent("process_start", "1", 100);

    auto all = std::make_unique<AllOf>();
    all->Add(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    all->Add(std::make_unique<FieldEndsWith>("image", "wscript.exe"));
    CHECK(all->Matches(event));

    auto all_broken = std::make_unique<AllOf>();
    all_broken->Add(std::make_unique<FieldEndsWith>("image", "wscript.exe"));
    all_broken->Add(std::make_unique<FieldEquals>("image", "cmd.exe"));
    CHECK_FALSE(all_broken->Matches(event));

    auto any = std::make_unique<AnyOf>();
    any->Add(std::make_unique<FieldEquals>("image", "cmd.exe"));
    any->Add(std::make_unique<FieldEndsWith>("image", "wscript.exe"));
    CHECK(any->Matches(event));
}

TEST_CASE("пустые наборы: AllOf истина, AnyOf ложь") {
    // Асимметрия намеренная и не произвольная. «Все из ничего выполнены» —
    // корректное утверждение, «хотя бы одно из ничего» — нет.
    const Event event = MakeEvent("process_start", "1", 100);

    const AllOf all;
    const AnyOf any;

    CHECK(all.Matches(event));
    CHECK_FALSE(any.Matches(event));
}

TEST_CASE("составные условия вкладываются друг в друга") {
    // То, чего переключатель не выражал вовсе: условие содержит условия,
    // ничего не зная об их видах — включая самих себя.
    const Event event = MakeEvent("process_start", "1", 100);

    auto hosts = std::make_unique<AnyOf>();
    hosts->Add(std::make_unique<FieldEndsWith>("image", "wscript.exe"));
    hosts->Add(std::make_unique<FieldEndsWith>("image", "cscript.exe"));

    auto rule_condition = std::make_unique<AllOf>();
    rule_condition->Add(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule_condition->Add(std::move(hosts));

    CHECK(rule_condition->Matches(event));
    CHECK_FALSE(rule_condition->Matches(MakeEvent("file_write", "1", 100)));
}

// ---------------------------------------------------------------------------
// MatchRule через интерфейс
// ---------------------------------------------------------------------------

TEST_CASE("правило работает через указатель на интерфейс") {
    auto rule = std::make_unique<MatchRule>("script_host", Severity::kHigh);
    rule->AddCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule->AddCondition(std::make_unique<FieldEndsWith>("image", "wscript.exe"));

    IRule* as_interface = rule.get();

    CHECK(as_interface->id() == "script_host");
    CHECK(as_interface->severity() == Severity::kHigh);
    CHECK(as_interface->Check(MakeEvent("process_start", "1", 100)));
    CHECK_FALSE(as_interface->Check(MakeEvent("file_write", "1", 100)));
    CHECK(as_interface->hits() == 1);
}

TEST_CASE("у интерфейса правила виртуальный деструктор") {
    CHECK(std::has_virtual_destructor_v<IRule>);
}

// ---------------------------------------------------------------------------
// SequenceRule: первое правило с состоянием
// ---------------------------------------------------------------------------

TEST_CASE("последовательность срабатывает на втором шаге") {
    SequenceRule rule("two_step", Severity::kHigh, 5000);
    rule.SetFirst(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule.SetSecond(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kNetConnect}));

    // Первый шаг сам по себе не детект.
    CHECK_FALSE(rule.Check(MakeEvent("process_start", "42", 1000)));
    CHECK(rule.pending() == 1);

    // Второй шаг того же процесса в пределах окна — детект.
    CHECK(rule.Check(MakeEvent("net_connect", "42", 2000)));
    CHECK(rule.hits() == 1);
}

TEST_CASE("шаги разных процессов не связываются") {
    // Без связки по процессу правило соединяло бы случайные события,
    // и ложные срабатывания были бы неизбежны.
    SequenceRule rule("two_step", Severity::kHigh, 5000);
    rule.SetFirst(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule.SetSecond(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kNetConnect}));

    rule.Check(MakeEvent("process_start", "42", 1000));
    CHECK_FALSE(rule.Check(MakeEvent("net_connect", "77", 2000)));
    CHECK(rule.hits() == 0);
}

TEST_CASE("за пределами окна связка не работает") {
    SequenceRule rule("two_step", Severity::kHigh, 1000);
    rule.SetFirst(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule.SetSecond(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kNetConnect}));

    rule.Check(MakeEvent("process_start", "42", 1000));
    CHECK_FALSE(rule.Check(MakeEvent("net_connect", "42", 5000)));
    CHECK(rule.hits() == 0);
}

TEST_CASE("незавершённые совпадения не накапливаются бесконечно") {
    // У правила с состоянием память растёт, и увидеть, что она не растёт
    // без предела, стоит своими глазами. Устаревшие совпадения обязаны
    // выбрасываться по времени, а не по счёту.
    SequenceRule rule("two_step", Severity::kHigh, 1000);
    rule.SetFirst(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule.SetSecond(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kNetConnect}));

    for (int i = 0; i < 100; ++i) {
        rule.Check(MakeEvent("process_start", std::to_string(i),
                             1000 + static_cast<std::uint64_t>(i) * 100));
    }

    // Окно 1000 мс, события шли с шагом 100 мс на протяжении 9900 мс.
    // В памяти обязаны остаться только последние.
    CHECK(rule.pending() < 20);
}

TEST_CASE("связку по процессу можно отключить") {
    SequenceRule rule("two_step", Severity::kLow, 5000);
    rule.SetFirst(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule.SetSecond(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kNetConnect}));
    rule.set_same_pid(false);

    rule.Check(MakeEvent("process_start", "42", 1000));
    CHECK(rule.Check(MakeEvent("net_connect", "77", 2000)));
}

// ---------------------------------------------------------------------------
// RuleEngine
// ---------------------------------------------------------------------------

TEST_CASE("движок гоняет событие по всем правилам") {
    RuleEngine engine;

    auto process = std::make_unique<MatchRule>("process", Severity::kLow);
    process->AddCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));

    auto script = std::make_unique<MatchRule>("script", Severity::kHigh);
    script->AddCondition(std::make_unique<FieldEndsWith>("image", "wscript.exe"));

    auto never = std::make_unique<MatchRule>("never", Severity::kLow);
    never->AddCondition(std::make_unique<FieldEquals>("image", "cmd.exe"));

    engine.AddRule(std::move(process));
    engine.AddRule(std::move(script));
    engine.AddRule(std::move(never));

    REQUIRE(engine.size() == 3);

    std::vector<const IRule*> fired;
    const std::size_t hits =
        engine.ProcessEvent(MakeEvent("process_start", "1", 100), &fired);

    CHECK(hits == 2);
    REQUIRE(fired.size() == 2);
    CHECK(fired[0]->id() == "process");
    CHECK(fired[1]->id() == "script");
}

TEST_CASE("движок дописывает в список, а не заменяет его") {
    // Очищать чужой контейнер — не дело функции, которая его не создавала.
    RuleEngine engine;
    auto rule = std::make_unique<MatchRule>("all", Severity::kLow);
    engine.AddRule(std::move(rule));

    std::vector<const IRule*> fired;
    engine.ProcessEvent(MakeEvent("process_start", "1", 100), &fired);
    engine.ProcessEvent(MakeEvent("file_write", "1", 200), &fired);

    CHECK(fired.size() == 2);
}

TEST_CASE("движок работает и без списка сработавших") {
    RuleEngine engine;
    auto rule = std::make_unique<MatchRule>("all", Severity::kLow);
    engine.AddRule(std::move(rule));

    CHECK(engine.ProcessEvent(MakeEvent("process_start", "1", 100), nullptr) == 1);
}

TEST_CASE("движок хранит правила разных типов") {
    // Главное, ради чего занятие: MatchRule и SequenceRule — разные классы,
    // и лежат в одном векторе. На 2.2 это было невозможно.
    RuleEngine engine;

    auto match = std::make_unique<MatchRule>("match", Severity::kLow);
    match->AddCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));

    auto sequence = std::make_unique<SequenceRule>("seq", Severity::kHigh, 5000);
    sequence->SetFirst(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    sequence->SetSecond(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kNetConnect}));

    engine.AddRule(std::move(match));
    engine.AddRule(std::move(sequence));

    std::vector<const IRule*> fired;
    engine.ProcessEvent(MakeEvent("process_start", "42", 1000), &fired);
    CHECK(fired.size() == 1);

    fired.clear();
    engine.ProcessEvent(MakeEvent("net_connect", "42", 2000), &fired);
    REQUIRE(fired.size() == 1);
    CHECK(fired[0]->id() == "seq");
}

TEST_CASE("движок нельзя копировать") {
    CHECK_FALSE(std::is_copy_constructible_v<RuleEngine>);
    CHECK_FALSE(std::is_copy_assignable_v<RuleEngine>);
}

// ---------------------------------------------------------------------------
// Интерфейсы источника и получателя
// ---------------------------------------------------------------------------

TEST_CASE("у интерфейсов источника и получателя виртуальные деструкторы") {
    // Источники хранятся указателями на интерфейс — иначе выбор источника
    // одной строкой не получится. Значит деструктор обязан быть виртуальным,
    // иначе при удалении файлового источника не закроется файл.
    CHECK(std::has_virtual_destructor_v<nano_edr::IEventSource>);
    CHECK(std::has_virtual_destructor_v<nano_edr::IEventHandler>);
}

TEST_CASE("интерфейсы абстрактны") {
    // Ни один из четырёх интерфейсов нельзя создать сам по себе, и это
    // не придирка: класс, который можно создать, однажды создадут.
    CHECK(std::is_abstract_v<nano_edr::IEventSource>);
    CHECK(std::is_abstract_v<nano_edr::IEventHandler>);
    CHECK(std::is_abstract_v<nano_edr::IRule>);
    CHECK(std::is_abstract_v<nano_edr::ICondition>);
}
