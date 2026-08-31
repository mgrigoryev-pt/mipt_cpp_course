// Тесты слоя доступа к полям. Занятие 1.3.
//
// Домашнее задание просит «таблицу тестов на каждую функцию». Вот она —
// и главное в ней не покрытие, а то, что каждый случай проверяет
// **канал отказа**: где nullptr, где false, где исключение. Функция, которая
// на отсутствующее поле бросает вместо nullptr, пройдёт половину тестов
// и провалит эту половину.

#include <cstdint>
#include <stdexcept>
#include <string>

#include "doctest.h"
#include "event.h"
#include "fields.h"

using nano_edr::CommandLineContains;
using nano_edr::Event;
using nano_edr::Field;
using nano_edr::FindField;
using nano_edr::GetIntField;
using nano_edr::GetRequiredField;
using nano_edr::IsFileWrite;
using nano_edr::IsNetConnect;
using nano_edr::IsProcessStart;
using nano_edr::NormalizePath;
using nano_edr::PathEndsWith;

namespace {

void AddField(Event& event, const std::string& key, const std::string& value) {
    Field field;
    field.key = key;
    field.value = value;
    event.fields.push_back(field);
}

Event MakeProcessStart() {
    Event event;
    event.ts = "1730000001000";
    event.type = "process_start";
    event.pid = "1042";
    AddField(event, "ppid", "880");
    AddField(event, "image", "C:\\Windows\\System32\\WScript.exe");
    AddField(event, "cmdline", "wscript.exe C:\\Users\\max\\AppData\\Local\\Temp\\A.JS");
    return event;
}

Event MakeFileWrite() {
    Event event;
    event.ts = "1730000002000";
    event.type = "file_write";
    event.pid = "1042";
    AddField(event, "path", "C:\\Users\\max\\Documents\\report.DOCX");
    AddField(event, "size", "812");
    return event;
}

}  // namespace

// ---------------------------------------------------------------------------
// FindField: отсутствие поля — ожидаемый исход, а не поломка
// ---------------------------------------------------------------------------

TEST_CASE("FindField находит поле") {
    const Event event = MakeFileWrite();

    const std::string* size = FindField(event, "size");
    REQUIRE(size != nullptr);
    CHECK(*size == "812");
}

TEST_CASE("FindField на отсутствующем поле даёт nullptr, а не исключение") {
    const Event event = MakeFileWrite();

    CHECK(FindField(event, "domain") == nullptr);
}

TEST_CASE("FindField при повторяющемся ключе даёт первое вхождение") {
    Event event = MakeFileWrite();
    AddField(event, "size", "999");

    const std::string* size = FindField(event, "size");
    REQUIRE(size != nullptr);
    CHECK(*size == "812");
}

TEST_CASE("FindField ищет только среди fields") {
    // ts, type и pid лежат в шапке события и доступны напрямую. Особого случая
    // для них в FindField нет намеренно: спрятанное правило кусает позже.
    const Event event = MakeFileWrite();

    CHECK(FindField(event, "ts") == nullptr);
    CHECK(FindField(event, "type") == nullptr);
    CHECK(FindField(event, "pid") == nullptr);
    CHECK(event.ts == "1730000002000");
}

TEST_CASE("FindField возвращает указатель внутрь события, а не копию") {
    const Event event = MakeFileWrite();

    const std::string* path = FindField(event, "path");
    REQUIRE(path != nullptr);
    CHECK(path == &event.fields[0].value);
}

// ---------------------------------------------------------------------------
// GetRequiredField: отсутствие поля — нарушение контракта
// ---------------------------------------------------------------------------

TEST_CASE("GetRequiredField отдаёт значение") {
    const Event event = MakeProcessStart();

    CHECK(GetRequiredField(event, "ppid") == "880");
}

TEST_CASE("GetRequiredField бросает invalid_argument") {
    const Event event = MakeFileWrite();

    CHECK_THROWS_AS(GetRequiredField(event, "image"), std::invalid_argument);
}

TEST_CASE("сообщение исключения называет поле") {
    // Диагностика, по которой нельзя найти причину, — половина диагностики.
    const Event event = MakeFileWrite();

    try {
        GetRequiredField(event, "image");
        FAIL("исключения не было");
    } catch (const std::invalid_argument& e) {
        const std::string message = e.what();
        CHECK(message.find("image") != std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// GetIntField: битое значение — внешние данные, а не ошибка программы
// ---------------------------------------------------------------------------

TEST_CASE("GetIntField разбирает целое") {
    const Event event = MakeFileWrite();

    uint64_t size = 0;
    REQUIRE(GetIntField(event, "size", &size));
    CHECK(size == 812);
}

TEST_CASE("GetIntField на отсутствующем поле даёт false") {
    const Event event = MakeFileWrite();

    uint64_t value = 42;
    CHECK_FALSE(GetIntField(event, "rport", &value));
}

TEST_CASE("GetIntField на нечисле даёт false, а не исключение") {
    Event event = MakeFileWrite();
    AddField(event, "weird", "812abc");
    AddField(event, "negative", "-5");
    AddField(event, "empty", "");

    uint64_t value = 0;
    CHECK_FALSE(GetIntField(event, "weird", &value));
    CHECK_FALSE(GetIntField(event, "negative", &value));
    CHECK_FALSE(GetIntField(event, "empty", &value));
}

TEST_CASE("GetIntField не переполняется молча") {
    Event event = MakeFileWrite();
    AddField(event, "huge", "99999999999999999999999999");

    uint64_t value = 0;
    CHECK_FALSE(GetIntField(event, "huge", &value));
}

TEST_CASE("перегрузка GetIntField со значением по умолчанию") {
    const Event event = MakeFileWrite();

    CHECK(GetIntField(event, "size", 7) == 812);
    CHECK(GetIntField(event, "rport", 7) == 7);
}

// ---------------------------------------------------------------------------
// Предикаты
// ---------------------------------------------------------------------------

TEST_CASE("предикаты типа события") {
    const Event start = MakeProcessStart();
    const Event write = MakeFileWrite();

    CHECK(IsProcessStart(start));
    CHECK_FALSE(IsProcessStart(write));

    CHECK(IsFileWrite(write));
    CHECK_FALSE(IsFileWrite(start));

    CHECK_FALSE(IsNetConnect(start));
    CHECK_FALSE(IsNetConnect(write));
}

TEST_CASE("PathEndsWith не смотрит на регистр") {
    // В файловой системе Windows «A.JS» и «a.js» — один файл, и правило,
    // которое этого не знает, обходится переименованием.
    const Event event = MakeFileWrite();

    CHECK(PathEndsWith(event, ".docx"));
    CHECK(PathEndsWith(event, ".DOCX"));
    CHECK(PathEndsWith(event, "report.docx"));
    CHECK_FALSE(PathEndsWith(event, ".js"));
}

TEST_CASE("PathEndsWith без поля path даёт false") {
    const Event event = MakeProcessStart();

    CHECK_FALSE(PathEndsWith(event, ".exe"));
}

TEST_CASE("CommandLineContains не смотрит на регистр") {
    const Event event = MakeProcessStart();

    CHECK(CommandLineContains(event, "a.js"));
    CHECK(CommandLineContains(event, "A.JS"));
    CHECK(CommandLineContains(event, "\\temp\\"));
    CHECK_FALSE(CommandLineContains(event, "powershell"));
}

TEST_CASE("CommandLineContains без поля cmdline даёт false") {
    const Event event = MakeFileWrite();

    CHECK_FALSE(CommandLineContains(event, "wscript"));
}

// ---------------------------------------------------------------------------
// NormalizePath
// ---------------------------------------------------------------------------

TEST_CASE("NormalizePath опускает регистр и приводит разделители") {
    CHECK(NormalizePath("C:/Users/Max/A.JS") == "c:\\users\\max\\a.js");
}

TEST_CASE("NormalizePath раскрывает %TEMP%") {
    // Настоящего окружения у журнала нет, поэтому раскрытие каноническое:
    // важно, чтобы %TEMP%\a.js и полный путь до Temp оказались одним каталогом.
    const std::string expanded = NormalizePath("%TEMP%\\a.js");
    const std::string full =
        NormalizePath("C:\\Users\\max\\AppData\\Local\\Temp\\a.js");

    CHECK(expanded.find("\\appdata\\local\\temp\\") != std::string::npos);
    CHECK(full.find("\\appdata\\local\\temp\\") != std::string::npos);
}

TEST_CASE("NormalizePath склеивает повторяющиеся разделители") {
    CHECK(NormalizePath("C:\\work\\\\build\\a.obj") == "c:\\work\\build\\a.obj");
}

TEST_CASE("NormalizePath не меняет исходную строку") {
    const std::string original = "C:/Users/Max/A.JS";
    const std::string normalized = NormalizePath(original);

    CHECK(original == "C:/Users/Max/A.JS");
    CHECK(normalized != original);
}
