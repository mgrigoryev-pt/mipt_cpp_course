// Тесты разбора строки журнала. Занятие 1.2.
//
// Половина случаев здесь — вырожденные входы: незакрытая кавычка, пустой ключ,
// пара без '=', дубликаты ключей. Это не придирки. Журнал пишет чужой код,
// и одна испорченная строка не должна ни ронять агента, ни превращаться
// в событие-обрубок, по которому потом сработает правило.
//
// Проверяется одна функция — ParseEventLine. Как она устроена внутри, тесты
// не спрашивают: разбор пар может лежать в ней самой, может в отдельной
// функции рядом. Поэтому шапка в каждом случае на месте — без ts и type
// строки журнала не бывает, даже когда проверяется поведение на кавычках.

#include <string>

#include "doctest.h"
#include "event.h"
#include "parse.h"

using nano_edr::Event;
using nano_edr::IsBlankOrComment;
using nano_edr::ParseEventLine;

// Поля проверяются по индексу, а не поиском по ключу. Так короче — и так
// заодно проверяется, что порядок полей сохранён: контракт это обещает,
// потому что повторяющиеся ключи в журнале законны.

TEST_CASE("строка журнала: шапка отдельно, остальное в fields") {
    const std::string line =
        "ts=1730000001000 type=file_write pid=1042 "
        "path=\"C:\\Users\\max\\a.js\" size=812";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    CHECK(event.ts == "1730000001000");
    CHECK(event.type == "file_write");
    CHECK(event.pid == "1042");
    REQUIRE(event.fields.size() == 2);
    CHECK(event.fields[0].key == "path");
    CHECK(event.fields[0].value == "C:\\Users\\max\\a.js");
    CHECK(event.fields[1].key == "size");
    CHECK(event.fields[1].value == "812");
}

TEST_CASE("порядок полей в строке не обязан быть таким, как в шапке") {
    const std::string line = "pid=7 path=x type=file_delete ts=100";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    CHECK(event.ts == "100");
    CHECK(event.type == "file_delete");
    CHECK(event.pid == "7");
    REQUIRE(event.fields.size() == 1);
    CHECK(event.fields[0].key == "path");
}

TEST_CASE("поля идут в том же порядке, что и в строке") {
    const std::string line = "ts=100 type=boot a=1 b=2 c=3";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    REQUIRE(event.fields.size() == 3);
    CHECK(event.fields[0].key == "a");
    CHECK(event.fields[0].value == "1");
    CHECK(event.fields[1].key == "b");
    CHECK(event.fields[2].key == "c");
}

TEST_CASE("событие без pid допустимо") {
    const std::string line = "ts=100 type=boot";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    CHECK(event.pid.empty());
    CHECK(event.fields.empty());
}

TEST_CASE("значение в кавычках сохраняет пробелы, кавычки снимаются") {
    const std::string line =
        "ts=100 type=process_start "
        "path=\"C:\\Program Files\\app.exe\" size=10";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    REQUIRE(event.fields.size() == 2);
    CHECK(event.fields[0].value == "C:\\Program Files\\app.exe");
    CHECK(event.fields[1].value == "10");
}

TEST_CASE("обратный слеш внутри кавычек — обычный символ") {
    // Экранирования в формате нет намеренно: в путях Windows обратный слеш
    // на каждом шагу, и трактовать его как escape значило бы ломать каждый
    // второй путь.
    const std::string line =
        "ts=100 type=file_write path=\"C:\\temp\\new\\test.js\"";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    REQUIRE(event.fields.size() == 1);
    CHECK(event.fields[0].value == "C:\\temp\\new\\test.js");
}

TEST_CASE("знак равенства внутри значения не делит пару второй раз") {
    const std::string line =
        "ts=100 type=process_start cmdline=\"app.exe --key=value\" k=a=b";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    REQUIRE(event.fields.size() == 2);
    CHECK(event.fields[0].value == "app.exe --key=value");
    CHECK(event.fields[1].key == "k");
    CHECK(event.fields[1].value == "a=b");
}

TEST_CASE("пустое значение — не ошибка") {
    const std::string line = "ts=100 type=boot k=";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    REQUIRE(event.fields.size() == 1);
    CHECK(event.fields[0].key == "k");
    CHECK(event.fields[0].value.empty());
}

TEST_CASE("повторяющийся ключ сохраняется") {
    // Повторы в журнале законны, и склеивать или выбрасывать их значит терять
    // то, что прислала система. Первое вхождение занимает своё место,
    // второе остаётся полем.
    const std::string line = "ts=100 type=boot tag=one tag=two ts=200";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    CHECK(event.ts == "100");
    REQUIRE(event.fields.size() == 3);
    CHECK(event.fields[0].value == "one");
    CHECK(event.fields[1].value == "two");
    CHECK(event.fields[2].key == "ts");
    CHECK(event.fields[2].value == "200");
}

TEST_CASE("лишние пробелы между парами не мешают") {
    const std::string line = "  ts=100   type=boot  a=1  ";
    Event event;

    REQUIRE(ParseEventLine(&line, &event));
    CHECK(event.type == "boot");
    CHECK(event.fields.size() == 1);
}

TEST_CASE("незакрытая кавычка — отказ") {
    const std::string line = "ts=100 type=file_write path=\"C:\\a b.js size=10";
    Event event;

    CHECK_FALSE(ParseEventLine(&line, &event));
}

TEST_CASE("пустой ключ — отказ") {
    const std::string line = "ts=100 type=boot =value";
    Event event;

    CHECK_FALSE(ParseEventLine(&line, &event));
}

TEST_CASE("слово без знака равенства — отказ") {
    const std::string line = "ts=100 type=boot broken a=1";
    Event event;

    CHECK_FALSE(ParseEventLine(&line, &event));
}

TEST_CASE("мусор сразу за закрывающей кавычкой — отказ") {
    const std::string line = "ts=100 type=boot k=\"a\"b";
    Event event;

    CHECK_FALSE(ParseEventLine(&line, &event));
}

TEST_CASE("событие без ts или без type — отказ") {
    Event event;

    const std::string no_ts = "type=file_write pid=1";
    CHECK_FALSE(ParseEventLine(&no_ts, &event));

    const std::string no_type = "ts=100 pid=1";
    CHECK_FALSE(ParseEventLine(&no_type, &event));
}

TEST_CASE("пустые строки и комментарии распознаются отдельно") {
    const std::string empty = "";
    const std::string spaces = "   \t ";
    const std::string hash = "# комментарий";
    const std::string semicolon = "  ; тоже комментарий";
    const std::string event_line = "ts=100 type=boot";

    CHECK(IsBlankOrComment(&empty));
    CHECK(IsBlankOrComment(&spaces));
    CHECK(IsBlankOrComment(&hash));
    CHECK(IsBlankOrComment(&semicolon));
    CHECK_FALSE(IsBlankOrComment(&event_line));
}

TEST_CASE("комментарий — не событие и не ошибка") {
    // Отличить пропускаемую строку от испорченной можно только через
    // IsBlankOrComment: ParseEventLine на обеих вернёт false.
    const std::string line = "# ts=100 type=file_write";
    Event event;

    CHECK_FALSE(ParseEventLine(&line, &event));
    CHECK(IsBlankOrComment(&line));
    CHECK(event.ts.empty());
    CHECK(event.fields.empty());
}
