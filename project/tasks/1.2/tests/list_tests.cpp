// Тесты окна последних событий. Занятие 1.2.
//
// Случаи подобраны по домашнему заданию: пустой список, один элемент, тысяча
// элементов, освобождение без утечек. Последнее проверяется не тестом,
// а санитайзером: соберите с -fsanitize=address,undefined и прогоните.
// Тест, который «проверяет отсутствие утечек» сам, обычно проверяет что-то
// другое.

#include <cstddef>
#include <string>

#include "doctest.h"
#include "event.h"
#include "event_list.h"

using nano_edr::Event;
using nano_edr::EventList;
using nano_edr::EventNode;
using nano_edr::ListClear;
using nano_edr::ListPopFront;
using nano_edr::ListPushBack;

namespace {

// Событие с узнаваемым ts — по нему видно, какие именно события остались
// в окне после вытеснения.
Event MakeEvent(int number) {
    Event event;
    event.ts = std::to_string(number);
    event.type = "file_write";
    event.pid = "1";
    return event;
}

// Сколько узлов в цепочке на самом деле. Отдельно от поля size намеренно:
// расхождение между ними — самая частая ошибка в этом задании.
std::size_t CountNodes(const EventList* list) {
    std::size_t count = 0;
    for (const EventNode* it = list->head.get(); it != nullptr;
         it = it->next.get()) {
        ++count;
    }
    return count;
}

}  // namespace

TEST_CASE("пустой список") {
    EventList list;

    CHECK(list.size == 0);
    CHECK(list.head == nullptr);
    CHECK(list.tail == nullptr);
    CHECK(CountNodes(&list) == 0);
}

TEST_CASE("убрать из пустого списка — не ошибка") {
    EventList list;

    ListPopFront(&list);

    CHECK(list.size == 0);
    CHECK(list.head == nullptr);
    CHECK(list.tail == nullptr);
}

TEST_CASE("очистить пустой список — не ошибка") {
    EventList list;

    ListClear(&list);

    CHECK(list.size == 0);
    CHECK(list.head == nullptr);
}

TEST_CASE("один элемент: голова и хвост — один узел") {
    EventList list;
    const Event event = MakeEvent(1);

    ListPushBack(&list, &event);

    REQUIRE(list.size == 1);
    REQUIRE(list.head != nullptr);
    CHECK(list.tail == list.head.get());
    CHECK(list.head->event.ts == "1");
    CHECK(list.head->next == nullptr);
    CHECK(CountNodes(&list) == 1);
}

TEST_CASE("порядок дописывания сохраняется") {
    EventList list;
    for (int i = 1; i <= 3; ++i) {
        const Event event = MakeEvent(i);
        ListPushBack(&list, &event);
    }

    REQUIRE(list.size == 3);
    REQUIRE(CountNodes(&list) == 3);

    const EventNode* it = list.head.get();
    CHECK(it->event.ts == "1");
    it = it->next.get();
    CHECK(it->event.ts == "2");
    it = it->next.get();
    CHECK(it->event.ts == "3");
    CHECK(it->next == nullptr);
    CHECK(list.tail == it);
}

TEST_CASE("убирается самое старое") {
    EventList list;
    for (int i = 1; i <= 3; ++i) {
        const Event event = MakeEvent(i);
        ListPushBack(&list, &event);
    }

    ListPopFront(&list);

    REQUIRE(list.size == 2);
    CHECK(list.head->event.ts == "2");
    CHECK(CountNodes(&list) == 2);
}

TEST_CASE("список, опустошённый до конца, забывает хвост") {
    // Забытый tail — ошибка отложенная: список выглядит рабочим, а падает
    // на следующем дописывании.
    EventList list;
    const Event event = MakeEvent(1);
    ListPushBack(&list, &event);

    ListPopFront(&list);

    REQUIRE(list.size == 0);
    CHECK(list.head == nullptr);
    CHECK(list.tail == nullptr);

    // И снова годен к работе.
    ListPushBack(&list, &event);
    REQUIRE(list.size == 1);
    CHECK(list.tail == list.head.get());
}

TEST_CASE("нулевая ёмкость означает «без ограничения»") {
    EventList list;
    REQUIRE(list.capacity == 0);

    for (int i = 0; i < 1000; ++i) {
        const Event event = MakeEvent(i);
        ListPushBack(&list, &event);
    }

    CHECK(list.size == 1000);
    CHECK(CountNodes(&list) == 1000);
    CHECK(list.head->event.ts == "0");
    CHECK(list.tail->event.ts == "999");
}

TEST_CASE("окно держит последние capacity событий") {
    EventList list;
    list.capacity = 3;

    for (int i = 1; i <= 5; ++i) {
        const Event event = MakeEvent(i);
        ListPushBack(&list, &event);
    }

    REQUIRE(list.size == 3);
    REQUIRE(CountNodes(&list) == 3);
    CHECK(list.head->event.ts == "3");
    CHECK(list.tail->event.ts == "5");
}

TEST_CASE("окно размером один") {
    // Вырожденный случай, на котором чаще всего и ломается вытеснение:
    // выбросить надо тот самый узел, который является и головой, и хвостом.
    EventList list;
    list.capacity = 1;

    for (int i = 1; i <= 3; ++i) {
        const Event event = MakeEvent(i);
        ListPushBack(&list, &event);
    }

    REQUIRE(list.size == 1);
    CHECK(list.head->event.ts == "3");
    CHECK(list.tail == list.head.get());
    CHECK(list.head->next == nullptr);
}

TEST_CASE("очистка возвращает список в исходное состояние") {
    EventList list;
    list.capacity = 10;
    for (int i = 0; i < 5; ++i) {
        const Event event = MakeEvent(i);
        ListPushBack(&list, &event);
    }

    ListClear(&list);

    REQUIRE(list.size == 0);
    CHECK(list.head == nullptr);
    CHECK(list.tail == nullptr);
    CHECK(list.capacity == 10);  // ёмкость — настройка, а не состояние

    const Event event = MakeEvent(42);
    ListPushBack(&list, &event);
    CHECK(list.size == 1);
    CHECK(list.head->event.ts == "42");
}

TEST_CASE("тысяча элементов освобождается деструктором") {
    // Сам тест ничего не проверяет, кроме того, что прогон дошёл до конца.
    // Смысл в санитайзере: под -fsanitize=address утечка или двойное
    // освобождение здесь и вылезут. Под обычной сборкой тест бесполезен —
    // и это нормально, тесты бывают разные.
    {
        EventList list;
        for (int i = 0; i < 1000; ++i) {
            const Event event = MakeEvent(i);
            ListPushBack(&list, &event);
        }
        REQUIRE(list.size == 1000);
    }

    CHECK(true);
}
