// Тесты разбора строки журнала. Занятие 2.1.
//
// Поведение разбора не изменилось с занятия 1.2 — изменился результат.
// Раньше разбор отдавал Event, теперь EventParts: у Event появился инвариант,
// и собрать его из обрывка нельзя. Проверять инвариант здесь нечего, это дело
// конструктора Event; здесь проверяется только разбор.

#include <string>
#include <vector>

#include "doctest.h"
#include "event.h"
#include "parse.h"

using nano_edr::EventParts;
using nano_edr::Field;
using nano_edr::IsBlankOrComment;
using nano_edr::ParseEventParts;
using nano_edr::ParseFields;

TEST_CASE("пары разбираются в порядке появления") {
    std::vector<Field> fields;

    REQUIRE(ParseFields("a=1 b=2 c=3", &fields));
    REQUIRE(fields.size() == 3);
    CHECK(fields[0].key == "a");
    CHECK(fields[0].value == "1");
    CHECK(fields[2].key == "c");
}

TEST_CASE("пустой текст — успех и ноль пар") {
    std::vector<Field> fields;

    CHECK(ParseFields("", &fields));
    CHECK(fields.empty());
}

TEST_CASE("значение в кавычках сохраняет пробелы") {
    std::vector<Field> fields;

    REQUIRE(ParseFields("path=\"C:\\Program Files\\app.exe\" size=10", &fields));
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].value == "C:\\Program Files\\app.exe");
    CHECK(fields[1].value == "10");
}

TEST_CASE("обратный слеш внутри кавычек — обычный символ") {
    // Экранирования в формате нет намеренно: в путях Windows обратный слеш
    // на каждом шагу.
    std::vector<Field> fields;

    REQUIRE(ParseFields("path=\"C:\\temp\\new\\test.js\"", &fields));
    REQUIRE(fields.size() == 1);
    CHECK(fields[0].value == "C:\\temp\\new\\test.js");
}

TEST_CASE("знак равенства внутри значения не делит пару второй раз") {
    std::vector<Field> fields;

    REQUIRE(ParseFields("cmdline=\"app.exe --key=value\" k=a=b", &fields));
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].value == "app.exe --key=value");
    CHECK(fields[1].value == "a=b");
}

TEST_CASE("повторяющийся ключ сохраняется дважды") {
    std::vector<Field> fields;

    REQUIRE(ParseFields("tag=one tag=two", &fields));
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].value == "one");
    CHECK(fields[1].value == "two");
}

TEST_CASE("вырожденные входы — отказ") {
    std::vector<Field> fields;

    CHECK_FALSE(ParseFields("path=\"C:\\a b.js size=10", &fields));  // кавычка
    CHECK_FALSE(ParseFields("=value", &fields));                     // пустой ключ
    CHECK_FALSE(ParseFields("a=1 broken b=2", &fields));             // без '='
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

TEST_CASE("событие без pid допустимо, без ts или type — нет") {
    EventParts parts;

    CHECK(ParseEventParts("ts=100 type=boot", &parts));
    CHECK(parts.pid.empty());

    CHECK_FALSE(ParseEventParts("type=file_write pid=1", &parts));
    CHECK_FALSE(ParseEventParts("ts=100 pid=1", &parts));
}

TEST_CASE("разбор не проверяет, что ts — число") {
    // Это инвариант Event, и проверяется он там. Один вопрос — одно место:
    // две проверки одного и того же однажды разойдутся.
    EventParts parts;

    CHECK(ParseEventParts("ts=вчера type=file_write", &parts));
    CHECK(parts.ts == "вчера");
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
