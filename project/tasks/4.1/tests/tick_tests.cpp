// Тесты уборки на тике. Занятие 4.1.
//
// Правило с состоянием — это контейнер, который живёт столько же, сколько
// процесс, и наполняется из потока. Лекция 13 называет такой контейнер утечкой
// с полезной нагрузкой: формально всё достижимо и корректно освободится при
// выходе, фактически процесс растёт, пока его не убьёт OOM-killer.
//
// Проверяется здесь именно это — что память правила ограничена, а не то,
// что правило срабатывает. Второе проверяют тесты 3.1, и они продолжают
// проходить: вытеснение не должно менять решения правила о срабатывании.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "conditions.h"
#include "doctest.h"
#include "event.h"
#include "rule.h"
#include "rule_engine.h"
#include "rules.h"

using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::EventType;
using nano_edr::EventTypeIs;
using nano_edr::Field;
using nano_edr::IRule;
using nano_edr::RuleEngine;
using nano_edr::SequenceRule;
using nano_edr::Severity;
using nano_edr::ThresholdRule;
using nano_edr::Timestamp;

namespace {

// CTAD (лекция 8): `std::vector{EventType::kFileWrite}` вместо
// `std::vector<EventType>{...}`. Тип выводится из содержимого, и в выданных
// тестах занятий 3.1 и 3.2 этого нет — там та же конструкция написана
// с явным аргументом. Разница в диффе видна, и она про читаемость: аргумент
// шаблона здесь ничего не сообщал, кроме того, что и так стоит рядом.

Event Make(const std::string& type, const std::string& pid,
           std::uint64_t ts_ms, const std::vector<Field>& fields) {
    EventParts parts;
    parts.ts = std::to_string(ts_ms);
    parts.type = type;
    parts.pid = pid;
    parts.fields = fields;
    return Event(parts);
}

Event Write(const std::string& pid, std::uint64_t ts_ms) {
    return Make("file_write", pid, ts_ms,
                {Field{"path", "C:\\Temp\\x.dat"}, Field{"size", "1024"}});
}

}  // namespace

// ---------------------------------------------------------------------------
// Пороговое правило: счётчики по ключам
// ---------------------------------------------------------------------------

TEST_CASE("счётчик заводится на каждый ключ") {
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 20, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector{EventType::kFileWrite}));

    for (std::uint64_t i = 0; i < 50; ++i) {
        rule.Check(Write(std::to_string(1000 + i), 1000 + i));
    }

    // Пятьдесят разных номеров — пятьдесят счётчиков. Это не изъян правила:
    // порог считается на процесс, иначе десять безобидных процессов сложились
    // бы в одну сработку. Изъян в том, что до 4.1 эти счётчики никто не убирал.
    CHECK(rule.bucket_count() == 50);
}

TEST_CASE("тик убирает счётчики, к которым давно не обращались") {
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 20, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector{EventType::kFileWrite}));
    rule.set_idle_ms(5000);

    for (std::uint64_t i = 0; i < 50; ++i) {
        rule.Check(Write(std::to_string(1000 + i), 1000 + i));
    }
    // Один ключ упомянут заново — он и должен остаться.
    rule.Check(Write("1007", 100000));
    REQUIRE(rule.bucket_count() == 50);

    rule.OnTick(Timestamp{100000});

    CHECK(rule.bucket_count() == 1);
}

TEST_CASE("нулевой простой отключает вытеснение счётчиков") {
    // Тестам занятия 3.1 важен сам порог, а не память, и они не звали
    // set_idle_ms. Умолчание обязано ничего не менять — иначе на 4.1 сломались
    // бы чужие тесты, и сломались бы тихо.
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 20, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector{EventType::kFileWrite}));

    rule.Check(Write("1042", 1000));
    rule.OnTick(Timestamp{9999999});

    CHECK(rule.bucket_count() == 1);
}

TEST_CASE("вытеснение счётчика не меняет решения правила") {
    // Порог остаётся порогом: девятнадцать не срабатывает, двадцатое
    // срабатывает — и после уборки чужих счётчиков тоже.
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 20, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector{EventType::kFileWrite}));
    rule.set_idle_ms(5000);

    for (std::uint64_t i = 0; i < 19; ++i) {
        CHECK_FALSE(rule.Check(Write("1042", 1000 + i)));
    }
    rule.OnTick(Timestamp{1019});
    CHECK(rule.Check(Write("1042", 1020)));
}

// ---------------------------------------------------------------------------
// Правило-последовательность: незакрытые совпадения
// ---------------------------------------------------------------------------

TEST_CASE("тик снимает просроченные первые шаги") {
    SequenceRule rule("two_steps", Severity::kMedium, 5000);
    rule.SetFirst(std::make_unique<EventTypeIs>(
        std::vector{EventType::kFileCreate}));
    rule.SetSecond(std::make_unique<EventTypeIs>(
        std::vector{EventType::kNetConnect}));

    for (std::uint64_t i = 0; i < 10; ++i) {
        rule.Check(Make("file_create", std::to_string(1000 + i), 1000 + i,
                        {Field{"path", "C:\\Temp\\a.js"}}));
    }
    REQUIRE(rule.pending() == 10);

    // До 4.1 эти десять снимались только при следующем вызове Check —
    // то есть никогда, если подходящих событий больше не будет.
    rule.OnTick(Timestamp{100000});

    CHECK(rule.pending() == 0);
}

TEST_CASE("тик не снимает то, у чего окно не истекло") {
    SequenceRule rule("two_steps", Severity::kMedium, 5000);
    rule.SetFirst(std::make_unique<EventTypeIs>(
        std::vector{EventType::kFileCreate}));
    rule.SetSecond(std::make_unique<EventTypeIs>(
        std::vector{EventType::kNetConnect}));

    rule.Check(Make("file_create", "1042", 1000,
                    {Field{"path", "C:\\Temp\\a.js"}}));
    rule.OnTick(Timestamp{3000});

    REQUIRE(rule.pending() == 1);

    // И совпадение всё ещё закрывается — тик не сломал правило.
    CHECK(rule.Check(Make("net_connect", "1042", 4000,
                          {Field{"raddr", "1.2.3.4"}})));
}

// ---------------------------------------------------------------------------
// Движок раздаёт тик
// ---------------------------------------------------------------------------

TEST_CASE("движок доносит тик до правил") {
    RuleEngine engine;

    auto owned = std::make_unique<ThresholdRule<64>>(
        "mass_write", Severity::kHigh, std::size_t(20),
        std::uint64_t(10000), std::string("pid"));
    owned->SetCondition(std::make_unique<EventTypeIs>(
        std::vector{EventType::kFileWrite}));
    owned->set_idle_ms(1000);
    ThresholdRule<64>* rule = owned.get();
    engine.AddRule(std::move(owned));

    engine.ProcessEvent(Write("1042", 1000), nullptr);
    REQUIRE(rule->bucket_count() == 1);

    engine.OnTick(Timestamp{50000});

    CHECK(rule->bucket_count() == 0);
}

TEST_CASE("правило без состояния тик переживает") {
    // У IRule::OnTick есть тело по умолчанию, и это единственный метод
    // интерфейса, у которого оно есть. Правилу без состояния убирать нечего,
    // и заставлять его писать пустую функцию значило бы платить за чужую
    // проблему.
    RuleEngine engine;

    auto rule = std::make_unique<nano_edr::MatchRule>("plain", Severity::kLow);
    rule->AddCondition(std::make_unique<EventTypeIs>(
        std::vector{EventType::kFileWrite}));
    engine.AddRule(std::move(rule));

    engine.OnTick(Timestamp{50000});

    std::vector<const IRule*> fired;
    CHECK(engine.ProcessEvent(Write("1042", 1000), &fired) == 1);
}
