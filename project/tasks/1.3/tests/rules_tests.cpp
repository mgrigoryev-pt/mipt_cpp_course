// Тесты движка правил. Занятие 1.3.
//
// Проверяется механика таблицы, а не сами правила: какие правила заведены —
// дело агента, и тесты про них у вас свои. Здесь — что CheckRules считает
// детекты, обходит всю таблицу и не спотыкается на пустой.
//
// CheckRules печатает детекты, поэтому в выводе прогона будут строки
// [DETECT] от тестовых правил. Так и должно быть: печать внутри — временная
// уступка, разводить её от подсчёта нечем до занятия 3.2.

#include <string>

#include "doctest.h"
#include "event.h"
#include "rules.h"

using nano_edr::CheckRules;
using nano_edr::Event;
using nano_edr::Rule;
using nano_edr::Severity;
using nano_edr::SeverityName;

namespace {

Event MakeEvent(const std::string& type) {
    Event event;
    event.ts = "1730000001000";
    event.type = type;
    event.pid = "1042";
    return event;
}

bool AlwaysTrue(const Event&) {
    return true;
}

bool AlwaysFalse(const Event&) {
    return false;
}

bool IsFileWriteOnly(const Event& event) {
    return event.type == "file_write";
}

}  // namespace

TEST_CASE("имена важности") {
    CHECK(std::string(SeverityName(Severity::kLow)) == "low");
    CHECK(std::string(SeverityName(Severity::kMedium)) == "medium");
    CHECK(std::string(SeverityName(Severity::kHigh)) == "high");
    CHECK(std::string(SeverityName(Severity::kCritical)) == "critical");
}

TEST_CASE("значение вне перечисления не роняет программу") {
    // Приведение здесь законно и определено: у enum class есть фиксированный
    // базовый тип (по умолчанию int), поэтому любое int-значение представимо.
    // У обычного enum без указанного базового типа так бы не вышло — там
    // диапазон ограничен числом бит под перечислители.
    const Severity broken = static_cast<Severity>(99);

    CHECK(std::string(SeverityName(broken)) == "?");
}

TEST_CASE("пустая таблица — ноль детектов") {
    const Event event = MakeEvent("file_write");

    CHECK(CheckRules(event, nullptr, 0) == 0);
}

TEST_CASE("считаются все сработавшие правила, а не первое") {
    const Event event = MakeEvent("file_write");
    const Rule rules[] = {
        {"always", AlwaysTrue, Severity::kLow},
        {"never", AlwaysFalse, Severity::kLow},
        {"file_write", IsFileWriteOnly, Severity::kMedium},
    };

    CHECK(CheckRules(event, rules, 3) == 2);
}

TEST_CASE("ни одно правило не сработало") {
    const Event event = MakeEvent("process_start");
    const Rule rules[] = {
        {"never", AlwaysFalse, Severity::kLow},
        {"file_write", IsFileWriteOnly, Severity::kMedium},
    };

    CHECK(CheckRules(event, rules, 2) == 0);
}

TEST_CASE("длина таблицы важнее её содержимого") {
    // Передана длина 1 — второе правило не должно проверяться, даже если
    // в массиве оно есть. Это ровно та ошибка, из-за которой на границе с C
    // читают за конец массива.
    const Event event = MakeEvent("file_write");
    const Rule rules[] = {
        {"never", AlwaysFalse, Severity::kLow},
        {"always", AlwaysTrue, Severity::kLow},
    };

    CHECK(CheckRules(event, rules, 1) == 0);
}
