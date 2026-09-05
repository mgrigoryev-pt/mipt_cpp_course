// Тесты условия-в-векторе и классов правил. Занятие 2.2.
//
// Тесты занятия 2.1 остаются подключёнными и проходят: событие, разбор, окно,
// обёртка над границей и четыре условия-функтора не изменились. Здесь только
// то, что добавилось.

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#include "conditions.h"
#include "doctest.h"
#include "event.h"
#include "match_rule.h"
#include "rule_base.h"
#include "rules.h"

using nano_edr::Condition;
using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::Field;
using nano_edr::MatchRule;
using nano_edr::RuleBase;
using nano_edr::Severity;

namespace {

Event MakeProcessStart() {
    EventParts parts;
    parts.ts = "1730000001000";
    parts.type = "process_start";
    parts.pid = "1042";
    parts.fields.push_back(Field{"image", "C:\\Windows\\System32\\WScript.exe"});
    parts.fields.push_back(
        Field{"cmdline", "wscript.exe C:\\Users\\max\\AppData\\Local\\Temp\\A.JS"});
    return Event(parts);
}

Event MakeFileWrite() {
    EventParts parts;
    parts.ts = "1730000002000";
    parts.type = "file_write";
    parts.pid = "1042";
    parts.fields.push_back(Field{"path", "C:\\work\\report.docx"});
    return Event(parts);
}

}  // namespace

// ---------------------------------------------------------------------------
// Condition
// ---------------------------------------------------------------------------

TEST_CASE("Condition::Equals — любое из значений") {
    const Event event = MakeProcessStart();

    CHECK(Condition::Equals("type", {"process_start"})(event));
    CHECK(Condition::Equals("type", {"file_write", "process_start"})(event));
    CHECK_FALSE(Condition::Equals("type", {"file_write"})(event));
}

TEST_CASE("Condition не смотрит на регистр") {
    const Event event = MakeProcessStart();

    CHECK(Condition::Equals("type", {"PROCESS_START"})(event));
    CHECK(Condition::Contains("cmdline", {"A.JS"})(event));
    CHECK(Condition::EndsWith("image", {"WSCRIPT.EXE"})(event));
}

TEST_CASE("Condition видит шапку события") {
    const Event event = MakeProcessStart();

    CHECK(Condition::Equals("pid", {"1042"})(event));
    CHECK(Condition::Equals("ts", {"1730000001000"})(event));
}

TEST_CASE("отсутствующее поле — условие не выполнено") {
    const Event event = MakeFileWrite();

    CHECK_FALSE(Condition::Equals("image", {"cmd.exe"})(event));
    CHECK_FALSE(Condition::Contains("cmdline", {"wscript"})(event));
    CHECK_FALSE(Condition::EndsWith("cmdline", {".js"})(event));
}

TEST_CASE("пустой список значений не выполняется никогда") {
    // «Любое из ничего» — это ложь, а не истина. Иначе условие без значений
    // срабатывало бы на всём, и опечатка в конфигурации правил превращалась бы
    // в правило-ловушку.
    const Event event = MakeProcessStart();

    CHECK_FALSE(Condition::Equals("type", {})(event));
    CHECK_FALSE(Condition::Contains("cmdline", {})(event));
    CHECK_FALSE(Condition::EndsWith("image", {})(event));
}

TEST_CASE("Condition можно положить в вектор") {
    // Ровно то, ради чего он существует: четыре функтора занятия 2.1 —
    // разные типы, и вектора из них не собрать.
    std::vector<Condition> conditions;
    conditions.push_back(Condition::Equals("type", {"process_start"}));
    conditions.push_back(Condition::EndsWith("image", {"wscript.exe"}));

    const Event event = MakeProcessStart();
    std::size_t matched = 0;
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        if (conditions[i](event)) {
            ++matched;
        }
    }

    CHECK(matched == 2);
}

// ---------------------------------------------------------------------------
// RuleBase и MatchRule
// ---------------------------------------------------------------------------

TEST_CASE("общая часть правила доступна через базу") {
    MatchRule rule("script_host", Severity::kHigh);

    CHECK(rule.id() == "script_host");
    CHECK(rule.severity() == Severity::kHigh);
    CHECK(rule.hits() == 0);
    CHECK(rule.actions().empty());
}

TEST_CASE("действия накапливаются в порядке добавления") {
    MatchRule rule("script_host", Severity::kHigh);
    rule.AddAction("kill_process");
    rule.AddAction("quarantine_file");

    REQUIRE(rule.actions().size() == 2);
    CHECK(rule.actions()[0] == "kill_process");
    CHECK(rule.actions()[1] == "quarantine_file");
}

TEST_CASE("все условия должны выполниться") {
    MatchRule rule("script_host_from_temp", Severity::kHigh);
    rule.AddCondition(Condition::Equals("type", {"process_start"}));
    rule.AddCondition(Condition::EndsWith("image", {"wscript.exe"}));
    rule.AddCondition(Condition::Contains("cmdline", {"\\Temp\\"}));

    CHECK(rule.Check(MakeProcessStart()));
    CHECK_FALSE(rule.Check(MakeFileWrite()));
}

TEST_CASE("одно невыполненное условие отменяет детект") {
    MatchRule rule("почти", Severity::kLow);
    rule.AddCondition(Condition::Equals("type", {"process_start"}));
    rule.AddCondition(Condition::EndsWith("image", {"cmd.exe"}));

    CHECK_FALSE(rule.Check(MakeProcessStart()));
}

TEST_CASE("правило без условий срабатывает на всём") {
    // Не защищено намеренно: правило без условий — ошибка автора правила,
    // а не случай, который надо обрабатывать. Но знать об этом стоит.
    MatchRule rule("пустое", Severity::kLow);

    CHECK(rule.Check(MakeProcessStart()));
    CHECK(rule.Check(MakeFileWrite()));
}

TEST_CASE("счётчик считает только детекты") {
    MatchRule rule("script_host", Severity::kHigh);
    rule.AddCondition(Condition::Equals("type", {"process_start"}));

    CHECK(rule.hits() == 0);
    rule.Check(MakeProcessStart());
    CHECK(rule.hits() == 1);
    rule.Check(MakeFileWrite());
    CHECK(rule.hits() == 1);
    rule.Check(MakeProcessStart());
    CHECK(rule.hits() == 2);
}

TEST_CASE("наследник — это база") {
    // Проверка на этапе компиляции. Обратное неверно, и следующий случай
    // про то, чем это оборачивается.
    CHECK(std::is_base_of_v<RuleBase, MatchRule>);
    CHECK(std::is_convertible_v<MatchRule*, RuleBase*>);
    CHECK_FALSE(std::is_convertible_v<RuleBase*, MatchRule*>);
}

TEST_CASE("объект наследника больше объекта базы") {
    // Главный вопрос занятия: почему MatchRule нельзя хранить
    // в std::vector<RuleBase>.
    //
    // Вектор хранит объекты по значению. При копировании MatchRule в ячейку
    // размера RuleBase от него осталась бы только базовая часть — вектор
    // условий исчез бы. Это называется срезкой, и компилятор про неё молчит:
    // копирование наследника в базу совершенно законно.
    //
    // Пока Check невиртуальный, обойти это нечем: хранить приходится вектором
    // конкретного типа. На занятии 2.3 появится virtual, и правила будут
    // храниться указателями на базу — тогда срезки не будет, потому что
    // указатель одного размера всегда.
    CHECK(sizeof(MatchRule) > sizeof(RuleBase));
}
