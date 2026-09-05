// Тесты разбора без владения. Занятие 4.2.
//
// Поведение разбора не изменилось ни в одном случае — изменился тип
// результата. Поэтому здесь проверяется два разных утверждения:
//
//   1. смотрящая форма разбирает ровно то же, что владеющая;
//   2. то, что она вернула, действительно смотрит внутрь исходного текста,
//      а не копирует его.
//
// Второе проверяется сравнением адресов: view на подстроку обязан указывать
// внутрь того же буфера. Это единственный способ проверить отсутствие
// аллокации, не заглядывая в аллокатор.

#include <string>
#include <string_view>
#include <vector>

#include "doctest.h"
#include "event.h"
#include "parse.h"

using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::EventPartsView;
using nano_edr::Field;
using nano_edr::FieldView;
using nano_edr::IsBlankOrComment;
using nano_edr::ParseEventParts;
using nano_edr::ParseFields;

namespace {

// Лежит ли view внутри буфера строки.
bool PointsInto(std::string_view view, const std::string& buffer) {
    return view.data() >= buffer.data() &&
           view.data() + view.size() <= buffer.data() + buffer.size();
}

}  // namespace

TEST_CASE("смотрящий разбор даёт пары в порядке появления") {
    std::vector<FieldView> fields;

    REQUIRE(ParseFields("a=1 b=2 c=3", &fields));
    REQUIRE(fields.size() == 3);
    CHECK(fields[0].key == "a");
    CHECK(fields[0].value == "1");
    CHECK(fields[2].key == "c");
}

TEST_CASE("значения смотрят внутрь исходной строки") {
    // Главное утверждение занятия: разбор не копирует.
    const std::string line = "ts=1730000001000 type=process_start pid=1042";
    std::vector<FieldView> fields;

    REQUIRE(ParseFields(line, &fields));
    REQUIRE(fields.size() == 3);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        CHECK(PointsInto(fields[i].key, line));
        CHECK(PointsInto(fields[i].value, line));
    }
}

TEST_CASE("кавычки снимаются, пробелы внутри сохраняются") {
    std::vector<FieldView> fields;

    REQUIRE(ParseFields("cmdline=\"cmd /c wscript a.js\" pid=7", &fields));
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].value == "cmd /c wscript a.js");
    CHECK(fields[1].key == "pid");
}

TEST_CASE("незакрытая кавычка — отказ") {
    std::vector<FieldView> fields;
    CHECK_FALSE(ParseFields("path=\"C:/a.js", &fields));
}

TEST_CASE("шапка вынимается, остальное остаётся полями") {
    const std::string line =
        "ts=1730000001000 type=file_write pid=1042 path=a.js size=812";
    EventPartsView parts;

    REQUIRE(ParseEventParts(line, &parts));
    CHECK(parts.ts == "1730000001000");
    CHECK(parts.type == "file_write");
    CHECK(parts.pid == "1042");
    REQUIRE(parts.fields.size() == 2);
    CHECK(parts.fields[0].key == "path");
    CHECK(parts.fields[1].key == "size");
}

TEST_CASE("повторный ключ шапки остаётся полем") {
    // Формат разрешает повторы, и терять их нельзя: угадывать, какое из двух
    // значений ts «настоящее», нечем.
    const std::string line = "ts=1 type=x ts=2";
    EventPartsView parts;

    REQUIRE(ParseEventParts(line, &parts));
    CHECK(parts.ts == "1");
    REQUIRE(parts.fields.size() == 1);
    CHECK(parts.fields[0].key == "ts");
    CHECK(parts.fields[0].value == "2");
}

TEST_CASE("переиспользование не оставляет следов прошлой строки") {
    // Так его и надо звать: один объект на весь цикл. Проверяется, что
    // очистка полная — иначе поля предыдущего события утекли бы в следующее.
    EventPartsView parts;

    REQUIRE(ParseEventParts("ts=1 type=a path=x size=1", &parts));
    REQUIRE(parts.fields.size() == 2);

    REQUIRE(ParseEventParts("ts=2 type=b", &parts));
    CHECK(parts.ts == "2");
    CHECK(parts.type == "b");
    CHECK(parts.pid.empty());
    CHECK(parts.fields.empty());
}

TEST_CASE("смотрящая и владеющая формы разбирают одинаково") {
    // Две реализации одного формата разошлись бы молча. Здесь проверяется,
    // что их не две: владеющая обязана быть переходником.
    const std::string line =
        "ts=1730000001000 type=net_connect pid=1101 raddr=185.12.3.4 rport=443";

    EventPartsView view;
    EventParts owned;
    REQUIRE(ParseEventParts(line, &view));
    REQUIRE(ParseEventParts(line, &owned));

    CHECK(view.ts == owned.ts);
    CHECK(view.type == owned.type);
    CHECK(view.pid == owned.pid);
    REQUIRE(view.fields.size() == owned.fields.size());
    for (std::size_t i = 0; i < view.fields.size(); ++i) {
        CHECK(view.fields[i].key == owned.fields[i].key);
        CHECK(view.fields[i].value == owned.fields[i].value);
    }
}

TEST_CASE("событие из view владеет своими строками") {
    // Событие обязано пережить буфер, из которого разобрано, — иначе оно
    // бесполезно: буфер это одна строка журнала.
    std::string line = "ts=1730000001000 type=process_start pid=7";
    EventPartsView parts;
    REQUIRE(ParseEventParts(line, &parts));

    const Event event(parts);

    // Буфер затирается. Если бы Event держал views, здесь бы всё поехало —
    // и поехало бы молча, потому что память ещё жива.
    line.assign(500, 'x');

    CHECK(event.raw_ts() == "1730000001000");
    CHECK(event.type() == "process_start");
    CHECK(event.pid() == "7");
}

TEST_CASE("битая шапка — отказ, комментарий — не событие") {
    EventPartsView parts;
    CHECK_FALSE(ParseEventParts("type=x pid=1", &parts));   // нет ts
    CHECK_FALSE(ParseEventParts("ts=1 pid=1", &parts));     // нет type
    CHECK_FALSE(ParseEventParts("# комментарий", &parts));
    CHECK(IsBlankOrComment("   "));
    CHECK(IsBlankOrComment("; тоже комментарий"));
    CHECK_FALSE(IsBlankOrComment("ts=1 type=x"));
}
