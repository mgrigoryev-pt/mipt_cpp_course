// Тесты окна последних событий. Занятие 2.1.
//
// Поведение то же, что на 1.2, но следит за ним теперь сам тип. На 1.2 тесты
// считали узлы отдельно от поля size, потому что за их согласованность
// не отвечал никто. Здесь проверка остаётся — но уже как проверка инварианта,
// а не как страховка от забывчивости вызывающего.

#include <cstddef>
#include <string>
#include <type_traits>

#include "doctest.h"
#include "event.h"
#include "event_list.h"

using nano_edr::Event;
using nano_edr::EventList;
using nano_edr::EventNode;
using nano_edr::EventParts;

namespace {

Event MakeEvent(int number) {
    EventParts parts;
    parts.ts = std::to_string(1730000000000LL + number);
    parts.type = "file_write";
    parts.pid = "1";
    return Event(parts);
}

// Сколько узлов в цепочке на самом деле — против того, что говорит size().
std::size_t CountNodes(const EventList& list) {
    std::size_t count = 0;
    for (const EventNode* it = list.head(); it != nullptr; it = it->next.get()) {
        ++count;
    }
    return count;
}

}  // namespace

TEST_CASE("окно нельзя копировать") {
    // Случайная копия тысячи узлов — не то, что хочется получить молча.
    CHECK_FALSE(std::is_copy_constructible_v<EventList>);
    CHECK_FALSE(std::is_copy_assignable_v<EventList>);
}

TEST_CASE("пустой список") {
    const EventList list;

    CHECK(list.size() == 0);
    CHECK(list.empty());
    CHECK(list.head() == nullptr);
    CHECK(list.back() == nullptr);
    CHECK(CountNodes(list) == 0);
}

TEST_CASE("убрать из пустого и очистить пустой — не ошибка") {
    EventList list;

    list.PopFront();
    list.Clear();

    CHECK(list.empty());
    CHECK(list.head() == nullptr);
    CHECK(list.back() == nullptr);
}

TEST_CASE("один элемент: голова и хвост — один узел") {
    EventList list;
    list.PushBack(MakeEvent(1));

    REQUIRE(list.size() == 1);
    REQUIRE(list.head() != nullptr);
    CHECK(list.back() == list.head());
    CHECK(list.head()->next == nullptr);
    CHECK(CountNodes(list) == 1);
}

TEST_CASE("порядок дописывания сохраняется") {
    EventList list;
    for (int i = 1; i <= 3; ++i) {
        list.PushBack(MakeEvent(i));
    }

    REQUIRE(list.size() == 3);
    REQUIRE(CountNodes(list) == 3);

    const EventNode* it = list.head();
    CHECK(it->event.ts().ms == 1730000000001ULL);
    it = it->next.get();
    CHECK(it->event.ts().ms == 1730000000002ULL);
    it = it->next.get();
    CHECK(it->event.ts().ms == 1730000000003ULL);
    CHECK(it->next == nullptr);
    CHECK(list.back() == it);
}

TEST_CASE("убирается самое старое") {
    EventList list;
    for (int i = 1; i <= 3; ++i) {
        list.PushBack(MakeEvent(i));
    }

    list.PopFront();

    REQUIRE(list.size() == 2);
    CHECK(list.head()->event.ts().ms == 1730000000002ULL);
    CHECK(CountNodes(list) == 2);
}

TEST_CASE("список, опустошённый до конца, забывает хвост") {
    // Забытый tail — ошибка отложенная: список выглядит рабочим, а падает
    // на следующем дописывании, обращаясь к освобождённому узлу.
    EventList list;
    list.PushBack(MakeEvent(1));
    list.PopFront();

    REQUIRE(list.empty());
    CHECK(list.head() == nullptr);
    CHECK(list.back() == nullptr);

    list.PushBack(MakeEvent(2));
    REQUIRE(list.size() == 1);
    CHECK(list.back() == list.head());
}

TEST_CASE("нулевая вместимость означает «без ограничения»") {
    EventList list;
    REQUIRE(list.capacity() == 0);

    for (int i = 0; i < 1000; ++i) {
        list.PushBack(MakeEvent(i));
    }

    CHECK(list.size() == 1000);
    CHECK(CountNodes(list) == 1000);
    CHECK(list.head()->event.ts().ms == 1730000000000ULL);
    CHECK(list.back()->event.ts().ms == 1730000000999ULL);
}

TEST_CASE("окно держит последние capacity событий") {
    EventList list(3);

    for (int i = 1; i <= 5; ++i) {
        list.PushBack(MakeEvent(i));
    }

    REQUIRE(list.size() == 3);
    REQUIRE(CountNodes(list) == 3);
    CHECK(list.head()->event.ts().ms == 1730000000003ULL);
    CHECK(list.back()->event.ts().ms == 1730000000005ULL);
}

TEST_CASE("окно размером один") {
    // Вырожденный случай, на котором чаще всего и ломается вытеснение:
    // выбросить надо тот самый узел, который является и головой, и хвостом.
    EventList list(1);

    for (int i = 1; i <= 3; ++i) {
        list.PushBack(MakeEvent(i));
    }

    REQUIRE(list.size() == 1);
    CHECK(list.head()->event.ts().ms == 1730000000003ULL);
    CHECK(list.back() == list.head());
    CHECK(list.head()->next == nullptr);
}

TEST_CASE("очистка возвращает список в исходное состояние") {
    EventList list(10);
    for (int i = 0; i < 5; ++i) {
        list.PushBack(MakeEvent(i));
    }

    list.Clear();

    REQUIRE(list.empty());
    CHECK(list.head() == nullptr);
    CHECK(list.back() == nullptr);
    CHECK(list.capacity() == 10);  // вместимость — настройка, а не состояние

    list.PushBack(MakeEvent(42));
    CHECK(list.size() == 1);
}

TEST_CASE("тысяча элементов освобождается деструктором") {
    // Сам тест ничего не проверяет, кроме того, что прогон дошёл до конца.
    // Смысл в санитайзере: утечка и двойное освобождение вылезут там.
    //
    // И отдельно: пустое тело деструктора здесь НЕ даёт утечки — поле head_
    // разрушится само. Меняется не наличие освобождения, а способ: вместо
    // цикла цепочку разберёт рекурсия. Тысячу узлов она выдержит, десятки
    // тысяч — нет.
    {
        EventList list;
        for (int i = 0; i < 1000; ++i) {
            list.PushBack(MakeEvent(i));
        }
        REQUIRE(list.size() == 1000);
    }

    CHECK(true);
}
