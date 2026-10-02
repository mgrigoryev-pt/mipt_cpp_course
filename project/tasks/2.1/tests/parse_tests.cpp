// Тесты разбора строки журнала. Занятие 2.1.
//
// Разбор пар не изменился с занятия 1.2 — изменился результат. Раньше разбор
// отдавал Event, теперь EventParts: у Event появился инвариант, и собрать его
// из обрывка нельзя. Проверять инвариант здесь нечего, это дело конструктора
// Event; здесь проверяется только разбор.

#include <stdexcept>
#include <string>

#include "doctest.h"
#include "event.h"
#include "parse.h"

using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::IsBlankOrComment;
using nano_edr::ParseEventParts;

// Разбор пар проверяется на строке с минимальной шапкой ts и type: всё
// остальное попадает в fields в порядке появления.

TEST_CASE("пары разбираются в порядке появления") {
    EventParts parts;

    REQUIRE(ParseEventParts("ts=100 type=boot a=1 b=2 c=3", &parts));
    REQUIRE(parts.fields.size() == 3);
    CHECK(parts.fields[0].key == "a");
    CHECK(parts.fields[0].value == "1");
    CHECK(parts.fields[2].key == "c");
}

TEST_CASE("строка из одной шапки — успех и ноль полей") {
    EventParts parts;

    CHECK(ParseEventParts("ts=100 type=boot", &parts));
    CHECK(parts.fields.empty());
}

TEST_CASE("значение в кавычках сохраняет пробелы") {
    EventParts parts;

    REQUIRE(ParseEventParts(
        "ts=100 type=file_write path=\"C:\\Program Files\\app.exe\" size=10",
        &parts));
    REQUIRE(parts.fields.size() == 2);
    CHECK(parts.fields[0].value == "C:\\Program Files\\app.exe");
    CHECK(parts.fields[1].value == "10");
}

TEST_CASE("обратный слеш внутри кавычек — обычный символ") {
    // Экранирования в формате нет намеренно: в путях Windows обратный слеш
    // на каждом шагу.
    EventParts parts;

    REQUIRE(ParseEventParts(
        "ts=100 type=file_write path=\"C:\\temp\\new\\test.js\"", &parts));
    REQUIRE(parts.fields.size() == 1);
    CHECK(parts.fields[0].value == "C:\\temp\\new\\test.js");
}

TEST_CASE("знак равенства внутри значения не делит пару второй раз") {
    EventParts parts;

    REQUIRE(ParseEventParts(
        "ts=100 type=boot cmdline=\"app.exe --key=value\" k=a=b", &parts));
    REQUIRE(parts.fields.size() == 2);
    CHECK(parts.fields[0].value == "app.exe --key=value");
    CHECK(parts.fields[1].value == "a=b");
}

TEST_CASE("повторяющийся ключ сохраняется дважды") {
    EventParts parts;

    REQUIRE(ParseEventParts("ts=100 type=boot tag=one tag=two", &parts));
    REQUIRE(parts.fields.size() == 2);
    CHECK(parts.fields[0].value == "one");
    CHECK(parts.fields[1].value == "two");
}

TEST_CASE("вырожденные входы — отказ") {
    EventParts parts;

    // Незакрытая кавычка, пустой ключ, слово без '='.
    CHECK_FALSE(
        ParseEventParts("ts=100 type=boot path=\"C:\\a b.js size=10", &parts));
    CHECK_FALSE(ParseEventParts("ts=100 type=boot =value", &parts));
    CHECK_FALSE(ParseEventParts("ts=100 type=boot a=1 broken b=2", &parts));
}

TEST_CASE("строка журнала: шапка отдельно, остальное в fields") {
    EventParts parts;

    REQUIRE(ParseEventParts(
        "ts=1730000001000 type=file_write pid=1042 "
        "path=\"C:\\Users\\max\\a.js\" size=812",
        &parts));

    CHECK(parts.ts == "1730000001000");
    CHECK(parts.type == "file_write");
    CHECK(parts.pid == "1042");
    REQUIRE(parts.fields.size() == 2);
    CHECK(parts.fields[0].key == "path");
    CHECK(parts.fields[1].key == "size");
}

TEST_CASE("порядок полей в строке произволен") {
    EventParts parts;

    REQUIRE(ParseEventParts("pid=7 path=x type=file_delete ts=100", &parts));
    CHECK(parts.ts == "100");
    CHECK(parts.type == "file_delete");
    CHECK(parts.pid == "7");
    REQUIRE(parts.fields.size() == 1);
}

TEST_CASE("повтор ключа шапки остаётся в fields") {
    // Первое вхождение занимает шапку, второе — обычное поле.
    EventParts parts;

    REQUIRE(ParseEventParts("ts=100 type=boot tag=x ts=200", &parts));
    CHECK(parts.ts == "100");
    REQUIRE(parts.fields.size() == 2);
    CHECK(parts.fields[0].key == "tag");
    CHECK(parts.fields[1].key == "ts");
    CHECK(parts.fields[1].value == "200");
}

TEST_CASE("событие без pid допустимо") {
    EventParts parts;

    CHECK(ParseEventParts("ts=100 type=boot", &parts));
    CHECK(parts.pid.empty());
}

TEST_CASE("разбор не проверяет, что ts — число") {
    // Это инвариант Event, и проверяется он там. Один вопрос — одно место:
    // две проверки одного и того же однажды разойдутся.
    EventParts parts;

    CHECK(ParseEventParts("ts=вчера type=file_write", &parts));
    CHECK(parts.ts == "вчера");
}

TEST_CASE("строку без ts или type отвергает конструктор Event") {
    // Тест не требует, чтобы разбор принял такую строку: важно только, что
    // события из неё не получится.
    for (const char* line : {"type=file_write pid=1", "ts=100 pid=1"}) {
        EventParts parts;
        if (ParseEventParts(line, &parts)) {
            CHECK_THROWS_AS(Event{parts}, std::invalid_argument);
        }
    }
}

TEST_CASE("пустые строки и комментарии распознаются отдельно") {
    CHECK(IsBlankOrComment(""));
    CHECK(IsBlankOrComment("   \t "));
    CHECK(IsBlankOrComment("# комментарий"));
    CHECK(IsBlankOrComment("  ; тоже комментарий"));
    CHECK_FALSE(IsBlankOrComment("ts=100 type=boot"));
}

TEST_CASE("комментарий — не событие и не ошибка") {
    // Отличить пропускаемую строку от испорченной можно только через
    // IsBlankOrComment: ParseEventParts на обеих вернёт false. Считать их
    // одним счётчиком значит прятать проблему журнала за комментарием.
    const std::string line = "# ts=100 type=file_write";
    EventParts parts;

    CHECK_FALSE(ParseEventParts(line, &parts));
    CHECK(IsBlankOrComment(line));
}
