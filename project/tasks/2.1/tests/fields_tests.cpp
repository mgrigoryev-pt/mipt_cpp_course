// Тесты функций доступа к полям. Занятие 2.1.
//
// Проверяется только то, что изменилось с занятия 1.3: FindField
// и GetRequiredField теперь видят шапку события. Остальное поведение этих
// функций прежнее, и его сверяет вывод агента на сценариях.

#include <stdexcept>
#include <string>

#include "doctest.h"
#include "event.h"
#include "fields.h"

using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::Field;
using nano_edr::FindField;
using nano_edr::GetRequiredField;

namespace {

Event MakeFileWrite() {
    EventParts parts;
    parts.ts = "1730000002000";
    parts.type = "file_write";
    parts.pid = "1042";
    parts.fields.push_back(Field{"path", "C:\\Users\\max\\a.js"});
    parts.fields.push_back(Field{"size", "812"});
    return Event(parts);
}

}  // namespace

TEST_CASE("FindField видит шапку события") {
    // На 1.3 было наоборот. Условиям нужно спрашивать про тип события так же,
    // как про любое другое поле.
    const Event event = MakeFileWrite();

    const std::string* ts = FindField(event, "ts");
    const std::string* type = FindField(event, "type");
    const std::string* pid = FindField(event, "pid");

    REQUIRE(ts != nullptr);
    REQUIRE(type != nullptr);
    REQUIRE(pid != nullptr);
    CHECK(*ts == "1730000002000");
    CHECK(*type == "file_write");
    CHECK(*pid == "1042");
}

TEST_CASE("ключ шапки берётся из шапки, даже если повторяется в fields") {
    // Разбор кладёт второй ts в fields. Значение события — первое.
    EventParts parts;
    parts.ts = "100";
    parts.type = "boot";
    parts.fields.push_back(Field{"ts", "200"});
    const Event event(parts);

    const std::string* ts = FindField(event, "ts");
    REQUIRE(ts != nullptr);
    CHECK(*ts == "100");
}

TEST_CASE("FindField по-прежнему находит обычные поля") {
    const Event event = MakeFileWrite();

    const std::string* size = FindField(event, "size");
    REQUIRE(size != nullptr);
    CHECK(*size == "812");
    CHECK(FindField(event, "domain") == nullptr);
}

TEST_CASE("GetRequiredField ищет там же, где FindField") {
    // Разница между двумя функциями одна — что делать, когда поля нет.
    const Event event = MakeFileWrite();

    CHECK(GetRequiredField(event, "ts") == "1730000002000");
    CHECK(GetRequiredField(event, "type") == "file_write");
    CHECK(GetRequiredField(event, "path") == "C:\\Users\\max\\a.js");
}

TEST_CASE("GetRequiredField на отсутствующем поле бросает и называет поле") {
    const Event event = MakeFileWrite();

    CHECK_THROWS_AS(GetRequiredField(event, "image"), std::invalid_argument);

    try {
        GetRequiredField(event, "image");
        FAIL("исключения не было");
    } catch (const std::invalid_argument& e) {
        const std::string message = e.what();
        CHECK(message.find("image") != std::string::npos);
    }
}
