// Тесты события с инвариантом. Занятие 2.1.
//
// Главный вопрос здесь один: что происходит, когда объект создать нельзя.
// Правильный ответ — исключение из конструктора, и никакого is_valid().
// Если у вас есть способ получить Event, который «создался, но пользоваться
// нельзя», значит инварианта нет.

#include <sstream>
#include <stdexcept>
#include <string>

#include "doctest.h"
#include "event.h"

using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::Field;
using nano_edr::Timestamp;
using nano_edr::ToString;

namespace {

EventParts MakeParts() {
    EventParts parts;
    parts.ts = "1730000001000";
    parts.type = "file_write";
    parts.pid = "1042";
    parts.fields.push_back(Field{"path", "C:\\work\\a.txt"});
    parts.fields.push_back(Field{"size", "812"});
    return parts;
}

}  // namespace

TEST_CASE("корректные части дают корректное событие") {
    const Event event(MakeParts());

    CHECK(event.ts().ms == 1730000001000ULL);
    CHECK(event.raw_ts() == "1730000001000");
    CHECK(event.type() == "file_write");
    CHECK(event.pid() == "1042");
    REQUIRE(event.fields().size() == 2);
    CHECK(event.fields()[0].key == "path");
}

TEST_CASE("порядок полей сохраняется") {
    // Повторяющиеся ключи в журнале законны, поэтому порядок — часть данных,
    // а не деталь реализации.
    EventParts parts = MakeParts();
    parts.fields.push_back(Field{"path", "C:\\work\\b.txt"});

    const Event event(parts);

    REQUIRE(event.fields().size() == 3);
    CHECK(event.fields()[0].value == "C:\\work\\a.txt");
    CHECK(event.fields()[2].value == "C:\\work\\b.txt");
}

// Обратите внимание на фигурные скобки в CHECK_THROWS_AS(Event{parts}, ...).
// С круглыми это разбирается не как выражение, а как объявление переменной
// parts типа Event — «most vexing parse», — и компилятор жалуется на отсутствие
// конструктора по умолчанию. Ошибка сбивает с толку, потому что говорит совсем
// не о том, что случилось.
TEST_CASE("событие без type не создаётся") {
    EventParts parts = MakeParts();
    parts.type.clear();

    CHECK_THROWS_AS(Event{parts}, std::invalid_argument);
}

TEST_CASE("событие без ts не создаётся") {
    EventParts parts = MakeParts();
    parts.ts.clear();

    CHECK_THROWS_AS(Event{parts}, std::invalid_argument);
}

TEST_CASE("ts не число — событие не создаётся") {
    // Разбор строки проверяет, что ts есть; что это число — инвариант Event.
    // Один вопрос проверяется в одном месте.
    EventParts parts = MakeParts();
    parts.ts = "вчера";

    CHECK_THROWS_AS(Event{parts}, std::invalid_argument);
}

TEST_CASE("ts не переполняется молча") {
    EventParts parts = MakeParts();
    parts.ts = "99999999999999999999999999";

    CHECK_THROWS_AS(Event{parts}, std::invalid_argument);
}

TEST_CASE("событие без pid допустимо") {
    // Не у всякого события есть процесс: у загрузки системы его нет.
    EventParts parts = MakeParts();
    parts.pid.clear();

    const Event event(parts);
    CHECK(event.pid().empty());
}

TEST_CASE("сообщение исключения называет причину") {
    EventParts parts = MakeParts();
    parts.ts = "вчера";

    try {
        const Event event(parts);
        FAIL("исключения не было");
    } catch (const std::invalid_argument& e) {
        const std::string message = e.what();
        CHECK(message.find("вчера") != std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// Timestamp
// ---------------------------------------------------------------------------

TEST_CASE("Timestamp сравнивается") {
    // Шесть операций из одной строчки `= default` — то, ради чего в лекции 5
    // разбирался <=>.
    const Timestamp early{100};
    const Timestamp late{200};

    CHECK(early < late);
    CHECK(late > early);
    CHECK(early <= early);
    CHECK(early == Timestamp{100});
    CHECK(early != late);
}

TEST_CASE("время события сравнимо с другим временем") {
    const Event first(MakeParts());

    EventParts later_parts = MakeParts();
    later_parts.ts = "1730000002000";
    const Event second(later_parts);

    CHECK(first.ts() < second.ts());
}

// ---------------------------------------------------------------------------
// Печать
// ---------------------------------------------------------------------------

TEST_CASE("ToString содержит шапку и поля") {
    const Event event(MakeParts());
    const std::string text = ToString(event);

    CHECK(text.find("1730000001000") != std::string::npos);
    CHECK(text.find("file_write") != std::string::npos);
    CHECK(text.find("1042") != std::string::npos);
    CHECK(text.find("path") != std::string::npos);
    CHECK(text.find("812") != std::string::npos);
}

TEST_CASE("operator<< печатает то же, что ToString") {
    // Два формата одного объекта разойдутся на первой правке, и разойдутся
    // молча. Поэтому один формат в одном месте.
    const Event event(MakeParts());

    std::ostringstream out;
    out << event;

    CHECK(out.str() == ToString(event));
}

TEST_CASE("событие форматируется через std::format") {
    // Специализация std::formatter выдана готовой — шаблоны это лекция 8.
    // Проверяется, что она сведена к вашему ToString, а не к своей печати.
    const Event event(MakeParts());

    CHECK(std::format("{}", event) == ToString(event));
}
