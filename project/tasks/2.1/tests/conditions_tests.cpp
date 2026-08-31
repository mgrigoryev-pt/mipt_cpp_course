// Тесты условий-функторов. Занятие 2.1.
//
// Условие — объект с operator(). Проверяется три вещи: что оно настраивается
// (то, чего не умеет функция), что оно не смотрит на регистр, и что оно видит
// шапку события так же, как остальные поля.

#include <string>
#include <vector>

#include "conditions.h"
#include "doctest.h"
#include "event.h"

using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::Field;
using nano_edr::FieldContains;
using nano_edr::FieldEndsWith;
using nano_edr::FieldEquals;
using nano_edr::FieldInList;

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

}  // namespace

TEST_CASE("FieldEquals сравнивает целиком и без регистра") {
    const Event event = MakeProcessStart();

    CHECK(FieldEquals("type", "process_start")(event));
    CHECK(FieldEquals("type", "PROCESS_START")(event));
    CHECK_FALSE(FieldEquals("type", "process")(event));
    CHECK_FALSE(FieldEquals("type", "file_write")(event));
}

TEST_CASE("условие видит шапку события") {
    // Без этого про тип события условие написать было бы нечем, а именно
    // оно нужно чаще всего.
    const Event event = MakeProcessStart();

    CHECK(FieldEquals("ts", "1730000001000")(event));
    CHECK(FieldEquals("pid", "1042")(event));
    CHECK(FieldEquals("type", "process_start")(event));
}

TEST_CASE("отсутствующее поле — условие не выполнено, а не ошибка") {
    // Спрашивать про поле, которого у этого типа события не бывает,
    // совершенно законно: у process_start нет domain.
    const Event event = MakeProcessStart();

    CHECK_FALSE(FieldEquals("domain", "evil.example")(event));
    CHECK_FALSE(FieldContains("path", "temp")(event));
    CHECK_FALSE(FieldEndsWith("path", ".js")(event));
    CHECK_FALSE(FieldInList("domain", {"a", "b"})(event));
}

TEST_CASE("FieldContains ищет подстроку без регистра") {
    const Event event = MakeProcessStart();

    CHECK(FieldContains("cmdline", "a.js")(event));
    CHECK(FieldContains("cmdline", "A.JS")(event));
    CHECK(FieldContains("cmdline", "\\temp\\")(event));
    CHECK_FALSE(FieldContains("cmdline", "powershell")(event));
}

TEST_CASE("FieldEndsWith смотрит на конец без регистра") {
    const Event event = MakeProcessStart();

    CHECK(FieldEndsWith("image", "wscript.exe")(event));
    CHECK(FieldEndsWith("image", "WSCRIPT.EXE")(event));
    CHECK_FALSE(FieldEndsWith("image", "cscript.exe")(event));
    // Именно конец, а не «где-то внутри».
    CHECK_FALSE(FieldEndsWith("image", "system32")(event));
}

TEST_CASE("FieldInList проверяет вхождение в набор") {
    // «Скриптовый хост» — это wscript.exe или cscript.exe. Через FieldEquals
    // это выражается только дублированием правила.
    const Event event = MakeProcessStart();

    CHECK(FieldEquals("type", "process_start")(event));
    CHECK(FieldInList("type", {"process_start", "process_end"})(event));
    CHECK(FieldInList("type", {"PROCESS_START"})(event));
    CHECK_FALSE(FieldInList("type", {"file_write", "net_connect"})(event));
}

TEST_CASE("пустой список ничему не соответствует") {
    const Event event = MakeProcessStart();

    CHECK_FALSE(FieldInList("type", {})(event));
}

TEST_CASE("одно условие настраивается под разные значения") {
    // Это и есть то, чего не умеет функция: состояние живёт в объекте,
    // а не в имени функции.
    const Event event = MakeProcessStart();

    const FieldEndsWith js("cmdline", ".js");
    const FieldEndsWith exe("cmdline", ".exe");

    CHECK(js(event));
    CHECK_FALSE(exe(event));
}

TEST_CASE("условие можно применить к нескольким событиям") {
    // У условия нет состояния между вызовами: один объект, много событий,
    // ответ зависит только от события.
    EventParts other;
    other.ts = "1730000002000";
    other.type = "file_write";
    other.fields.push_back(Field{"path", "C:\\work\\a.locked"});

    const FieldEndsWith locked("path", ".locked");

    CHECK_FALSE(locked(MakeProcessStart()));
    CHECK(locked(Event(other)));
    CHECK_FALSE(locked(MakeProcessStart()));
}
