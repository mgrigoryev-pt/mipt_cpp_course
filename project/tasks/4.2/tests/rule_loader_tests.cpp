// Тесты загрузчика правил. Занятие 4.2.
//
// Проверяется не «разбирается ли правильный файл» — это самая лёгкая часть.
// Проверяется то, ради чего загрузчик пишется придирчивым: что на битой
// конфигурации он отказывает, и отказывает С НОМЕРОМ СТРОКИ.
//
// Номер строки здесь не удобство, а требование занятия. Конфигурация,
// диагностика которой не называет строку, отлаживается перебором: человек
// правит наугад и перезапускает.
//
// Тесты работают через RuleLoader::Parse, а не Load: файл на диске тут
// не нужен, а строка в тесте видна целиком вместе с ожидаемым номером.
//
// Два случая из пятнадцати относятся к продвинутой части — к видам правил
// sequence и threshold. Они закрыты NANO_EDR_RULE_KINDS и по умолчанию
// не собираются: базовая часть занятия читает из файла только match,
// и тест, требующий большего, сообщал бы о несделанном как о сломанном.

#include <expected>
#include <string>
#include <string_view>

#include "doctest.h"
#include "rule_engine.h"
#include "rule_loader.h"

using nano_edr::ConfigError;
using nano_edr::LoadOptions;
using nano_edr::RuleEngine;
using nano_edr::RuleLoader;
using nano_edr::RuleSet;

namespace {

// Минимальное правило, к которому тесты добавляют по одной битой строке.
constexpr std::string_view kGood =
    "[rule demo]\n"
    "kind     = match\n"
    "severity = high\n"
    "match    = type is process_start\n"
    "action   = kill_process\n";

std::expected<RuleSet, ConfigError> ParseText(std::string_view text) {
    return RuleLoader::Parse(text, "rules.conf", LoadOptions{});
}

}  // namespace

TEST_CASE("исправная конфигурация даёт правила") {
    auto rules = ParseText(kGood);
    REQUIRE(rules.has_value());
    CHECK(rules->size() == 1);
}

TEST_CASE("правила уезжают в движок, набор становится пустым") {
    auto rules = ParseText(kGood);
    REQUIRE(rules.has_value());

    RuleEngine engine;
    rules->MoveInto(&engine);

    CHECK(engine.size() == 1);
    // Владение переехало. Набор, продолжающий делать вид, что правила
    // при нём, — это набор пустых указателей.
    CHECK(rules->empty());
}

#ifdef NANO_EDR_RULE_KINDS
TEST_CASE("три вида правил разбираются") {
    auto rules = ParseText(
        "[rule a]\n"
        "kind = match\n"
        "severity = low\n"
        "match = type is process_start\n"
        "\n"
        "[rule b]\n"
        "kind = sequence\n"
        "severity = high\n"
        "window_ms = 10000\n"
        "first = type is process_start\n"
        "second = type is net_connect\n"
        "\n"
        "[rule c]\n"
        "kind = threshold\n"
        "severity = critical\n"
        "count = 20\n"
        "window_ms = 10000\n"
        "key = pid\n"
        "match = type is file_write\n");
    REQUIRE(rules.has_value());
    CHECK(rules->size() == 3);
}
#endif  // NANO_EDR_RULE_KINDS

TEST_CASE("неизвестный ключ — отказ с номером строки") {
    // Не «пропустим»: неизвестный ключ почти всегда опечатка, а опечатка
    // в конфигурации детектов — это правило, которого нет.
    auto rules = ParseText(
        "[rule demo]\n"
        "kind = match\n"
        "severiti = high\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 3);
    CHECK(rules.error().Message().find("rules.conf:3") != std::string::npos);
}

TEST_CASE("неизвестный оператор — отказ с номером строки") {
    auto rules = ParseText(
        "[rule demo]\n"
        "kind = match\n"
        "severity = high\n"
        "match = image startswith cmd.exe\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 4);
}

TEST_CASE("неизвестная важность — отказ") {
    auto rules = ParseText(
        "[rule demo]\n"
        "kind = match\n"
        "severity = очень\n"
        "match = type is process_start\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 3);
}

TEST_CASE("неизвестный тип события — отказ") {
    // Опечатка в имени типа была бы правилом, которое не срабатывает никогда.
    auto rules = ParseText(
        "[rule demo]\n"
        "kind = match\n"
        "severity = high\n"
        "match = type is proces_start\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 4);
}

TEST_CASE("строка без равенства — отказ") {
    auto rules = ParseText(
        "[rule demo]\n"
        "kind match\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 2);
}

TEST_CASE("строка вне секции — отказ") {
    auto rules = ParseText("kind = match\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 1);
}

TEST_CASE("правило без условий — отказ, и он про заголовок правила") {
    // Номер указывает на строку `[rule demo]`, а не на то место, где правило
    // кончилось: человек читает про правило, а не про его границу.
    auto rules = ParseText(
        "[rule demo]\n"
        "kind = match\n"
        "severity = high\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 1);
}

TEST_CASE("правило без kind — отказ") {
    auto rules = ParseText(
        "[rule demo]\n"
        "severity = high\n"
        "match = type is process_start\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 1);
}

TEST_CASE("пустой файл — отказ без номера строки") {
    // Ноль означает «к строке не привязано»: называть строку тут нечего,
    // и выдумывать её было бы враньём.
    auto rules = ParseText("# только комментарий\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 0);
    // И в сообщении номера нет — двоеточие с числом не появляется.
    CHECK(rules.error().Message().find("rules.conf:0") == std::string::npos);
}

#ifdef NANO_EDR_RULE_KINDS
TEST_CASE("порог больше вместимости буфера — отказ") {
    // Правило с count больше буфера отметок не сработает никогда. Молча
    // принять его значит отдать человеку правило, которое выглядит рабочим.
    auto rules = ParseText(
        "[rule demo]\n"
        "kind = threshold\n"
        "severity = high\n"
        "count = 100000\n"
        "window_ms = 1000\n"
        "key = pid\n"
        "match = type is file_write\n");
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 1);
}
#endif  // NANO_EDR_RULE_KINDS

TEST_CASE("слово or внутри кавычек не режет условие") {
    // Разбор, не знающий про кавычки, сломался бы здесь на два бессмысленных
    // куска — и сломался бы молча, приняв битое правило за исправное.
    auto rules = ParseText(
        "[rule demo]\n"
        "kind = match\n"
        "severity = high\n"
        "match = cmdline contains \"a or b\"\n");
    REQUIRE(rules.has_value());
    CHECK(rules->size() == 1);
}

TEST_CASE("файла нет — отказ без номера строки") {
    auto rules = RuleLoader::Load("нет-такого-файла.conf", LoadOptions{});
    REQUIRE_FALSE(rules.has_value());
    CHECK(rules.error().line == 0);
    CHECK(rules.error().Message().find("нет-такого-файла") !=
          std::string::npos);
}
