// Тесты получателей детектов. Занятие 3.2.
//
// Тесты занятий 2.1, 2.3 и 3.1 остаются подключёнными: событие, разбор,
// обёртка, условия, движок, три вида правил, кольцевой буфер и комбинаторы
// не изменились. Всё, что добавило это занятие, добавлено рядом.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "alert_sink.h"
#include "dedup_sink.h"
#include "detection.h"
#include "doctest.h"
#include "event.h"
#include "rule.h"
#include "rule_engine.h"
#include "rules.h"

using nano_edr::AlertCounter;
using nano_edr::AlertSink;
using nano_edr::CountingSink;
using nano_edr::DedupSink;
using nano_edr::Detection;
using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::EventType;
using nano_edr::EventTypeIs;
using nano_edr::FanOutSink;
using nano_edr::Field;
using nano_edr::LambdaRule;
using nano_edr::MatchRule;
using nano_edr::RuleEngine;
using nano_edr::Severity;
using nano_edr::Timestamp;

namespace {

Event MakeEvent(const std::string& type, const std::string& pid,
                std::uint64_t ts_ms) {
    EventParts parts;
    parts.ts = std::to_string(ts_ms);
    parts.type = type;
    parts.pid = pid;
    parts.fields.push_back(Field{"path", "C:\\tmp\\a.txt"});
    return Event(parts);
}

Detection MakeDetection(const std::string& rule, const std::string& pid,
                        std::uint64_t ts_ms) {
    Detection detection;
    detection.rule = rule;
    detection.severity = Severity::kHigh;
    detection.ts = Timestamp{ts_ms};
    detection.pid = pid;
    return detection;
}

}  // namespace

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

TEST_CASE("строка детекта без цепочки") {
    const Detection detection = MakeDetection("test_rule", "1042", 1000);

    const std::string text = nano_edr::ToString(detection);

    CHECK(text.find("[DETECT]") != std::string::npos);
    CHECK(text.find("test_rule") != std::string::npos);
    CHECK(text.find("pid=1042") != std::string::npos);
    // Цепочки нет — и слова chain нет тоже. Пустое `chain=` соврало бы,
    // что цепочка известна и пуста.
    CHECK(text.find("chain=") == std::string::npos);
}

TEST_CASE("строка детекта с цепочкой") {
    Detection detection = MakeDetection("test_rule", "1042", 1000);
    detection.chain = "880>1042(cmd.exe)";

    const std::string text = nano_edr::ToString(detection);

    CHECK(text.find("chain=880>1042(cmd.exe)") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Получатели
// ---------------------------------------------------------------------------

TEST_CASE("счётчик считает и запоминает") {
    AlertCounter counter;
    const AlertSink sink = CountingSink(&counter);

    sink(MakeDetection("a", "1", 100));
    sink(MakeDetection("b", "2", 200));

    REQUIRE(counter.total == 2);
    REQUIRE(counter.seen.size() == 2);
    CHECK(counter.seen[0].rule == "a");
    CHECK(counter.seen[1].pid == "2");
}

TEST_CASE("счётчик пишет наружу, а не в свою копию") {
    // Получатель хранится внутри AlertSink по значению, то есть отданная копия
    // тесту больше не видна. Именно поэтому наблюдаемое состояние живёт
    // снаружи, а получатель только указывает на него.
    AlertCounter counter;
    FanOutSink fan;
    fan.Add(CountingSink(&counter));

    const AlertSink sink = std::move(fan);
    sink(MakeDetection("a", "1", 100));

    CHECK(counter.total == 1);
}

TEST_CASE("раздача нескольким получателям") {
    AlertCounter first;
    AlertCounter second;
    FanOutSink fan;
    fan.Add(CountingSink(&first));
    fan.Add(CountingSink(&second));

    REQUIRE(fan.size() == 2);
    fan(MakeDetection("a", "1", 100));

    CHECK(first.total == 1);
    CHECK(second.total == 1);
}

TEST_CASE("получатель из получателя") {
    // Получатель — значение, поэтому его можно обернуть. С интерфейсом
    // и наследниками так тоже можно, но обёртку пришлось бы проектировать
    // заранее; здесь — не пришлось.
    AlertCounter counter;
    FanOutSink inner;
    inner.Add(CountingSink(&counter));

    FanOutSink outer;
    outer.Add(std::move(inner));
    outer(MakeDetection("a", "1", 100));

    CHECK(counter.total == 1);
}

// ---------------------------------------------------------------------------
// Подавление дубликатов
// ---------------------------------------------------------------------------

TEST_CASE("повтор до истечения срока подавлен") {
    AlertCounter counter;
    std::size_t suppressed = 0;
    DedupSink dedup(CountingSink(&counter), 10000, &suppressed);

    dedup(MakeDetection("mass_write", "42", 1000));
    dedup(MakeDetection("mass_write", "42", 2000));
    dedup(MakeDetection("mass_write", "42", 9000));

    CHECK(counter.total == 1);
    CHECK(suppressed == 2);
}

TEST_CASE("после срока детект проходит снова") {
    // Срок отсчитывается от ПЕРВОГО детекта. Иначе непрерывный поток одинаковых
    // детектов молчал бы вечно, и про длящуюся атаку агент перестал бы
    // докладывать.
    AlertCounter counter;
    std::size_t suppressed = 0;
    DedupSink dedup(CountingSink(&counter), 10000, &suppressed);

    dedup(MakeDetection("mass_write", "42", 1000));
    dedup(MakeDetection("mass_write", "42", 5000));
    dedup(MakeDetection("mass_write", "42", 11001));

    CHECK(counter.total == 2);
    CHECK(suppressed == 1);
}

TEST_CASE("на границе срока повтор подавлен, после срока отсчёт заново") {
    AlertCounter counter;
    std::size_t suppressed = 0;
    DedupSink dedup(CountingSink(&counter), 10000, &suppressed);

    dedup(MakeDetection("mass_write", "42", 1000));
    dedup(MakeDetection("mass_write", "42", 11000));
    dedup(MakeDetection("mass_write", "42", 11001));
    dedup(MakeDetection("mass_write", "42", 21001));
    dedup(MakeDetection("mass_write", "42", 21002));

    REQUIRE(counter.total == 3);
    CHECK(suppressed == 2);
    CHECK(counter.seen[1].ts.ms == 11001);
    CHECK(counter.seen[2].ts.ms == 21002);
}

TEST_CASE("разные процессы не подавляют друг друга") {
    AlertCounter counter;
    std::size_t suppressed = 0;
    DedupSink dedup(CountingSink(&counter), 10000, &suppressed);

    dedup(MakeDetection("mass_write", "42", 1000));
    dedup(MakeDetection("mass_write", "43", 1100));

    CHECK(counter.total == 2);
    CHECK(suppressed == 0);
}

TEST_CASE("разные правила не подавляют друг друга") {
    AlertCounter counter;
    std::size_t suppressed = 0;
    DedupSink dedup(CountingSink(&counter), 10000, &suppressed);

    dedup(MakeDetection("mass_write", "42", 1000));
    dedup(MakeDetection("ransom_extension", "42", 1100));

    CHECK(counter.total == 2);
    CHECK(suppressed == 0);
}

TEST_CASE("нулевой срок выключает подавление") {
    AlertCounter counter;
    std::size_t suppressed = 0;
    DedupSink dedup(CountingSink(&counter), 0, &suppressed);

    dedup(MakeDetection("mass_write", "42", 1000));
    dedup(MakeDetection("mass_write", "42", 1001));

    CHECK(counter.total == 2);
    CHECK(suppressed == 0);
}

TEST_CASE("счётчик подавленных может быть nullptr") {
    AlertCounter counter;
    DedupSink dedup(CountingSink(&counter), 10000, nullptr);

    dedup(MakeDetection("mass_write", "42", 1000));
    dedup(MakeDetection("mass_write", "42", 2000));

    CHECK(counter.total == 1);
}

// ---------------------------------------------------------------------------
// Движок и получатели
// ---------------------------------------------------------------------------

TEST_CASE("движок без получателей работает") {
    // Критерий занятия с другой стороны: движок ничего не знает о получателях,
    // в том числе и того, есть ли они. Тесты занятия 2.3, не знающие про них
    // вовсе, обязаны продолжать проходить.
    RuleEngine engine;
    auto rule = std::make_unique<MatchRule>("any_start", Severity::kLow);
    rule->AddCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    engine.AddRule(std::move(rule));

    CHECK(engine.sink_count() == 0);
    CHECK(engine.ProcessEvent(MakeEvent("process_start", "1", 100), nullptr) ==
          1);
}

TEST_CASE("детект уходит получателю") {
    AlertCounter counter;
    RuleEngine engine;
    auto rule = std::make_unique<MatchRule>("any_start", Severity::kHigh);
    rule->AddCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule->AddAction("kill_process");
    engine.AddRule(std::move(rule));
    engine.Subscribe(CountingSink(&counter));

    engine.ProcessEvent(MakeEvent("process_start", "1042", 1000), nullptr);
    engine.ProcessEvent(MakeEvent("file_write", "1042", 1100), nullptr);

    REQUIRE(counter.total == 1);
    CHECK(counter.seen[0].rule == "any_start");
    CHECK(counter.seen[0].pid == "1042");
    CHECK(counter.seen[0].ts.ms == 1000);
    CHECK(counter.seen[0].severity == Severity::kHigh);
    REQUIRE(counter.seen[0].actions.size() == 1);
    CHECK(counter.seen[0].actions[0] == "kill_process");
}

TEST_CASE("получателей может быть несколько") {
    AlertCounter first;
    AlertCounter second;
    RuleEngine engine;
    auto rule = std::make_unique<MatchRule>("any_start", Severity::kLow);
    rule->AddCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    engine.AddRule(std::move(rule));
    engine.Subscribe(CountingSink(&first));
    engine.Subscribe(CountingSink(&second));

    REQUIRE(engine.sink_count() == 2);
    engine.ProcessEvent(MakeEvent("process_start", "1", 100), nullptr);

    CHECK(first.total == 1);
    CHECK(second.total == 1);
}

// ---------------------------------------------------------------------------
// LambdaRule
// ---------------------------------------------------------------------------

TEST_CASE("правило из предиката") {
    LambdaRule rule("even_pid", Severity::kLow, [](const Event& event) {
        return !event.pid().empty() && (event.pid().back() - '0') % 2 == 0;
    });

    CHECK(rule.Check(MakeEvent("process_start", "1042", 100)));
    CHECK_FALSE(rule.Check(MakeEvent("process_start", "1043", 200)));
    CHECK(rule.hits() == 1);
    CHECK(rule.id() == "even_pid");
}

TEST_CASE("правило с захватом внешнего состояния") {
    // То, ради чего LambdaRule и нужно: предикат смотрит не только
    // на событие. На занятии это модель сущностей; здесь — счётчик,
    // чтобы тест остался про правило, а не про модель.
    std::size_t asked = 0;
    LambdaRule rule("counts_calls", Severity::kLow,
                    [&asked](const Event&) {
                        ++asked;
                        return false;
                    });

    rule.Check(MakeEvent("process_start", "1", 100));
    rule.Check(MakeEvent("file_write", "1", 200));

    CHECK(asked == 2);
    CHECK(rule.hits() == 0);
}

TEST_CASE("правило без предиката молчит") {
    LambdaRule rule("empty", Severity::kLow, LambdaRule::Predicate());

    CHECK_FALSE(rule.Check(MakeEvent("process_start", "1", 100)));
    CHECK(rule.hits() == 0);
}
