// Тесты фабрики и перемещения обёртки над границей. Занятие 4.2.
//
// Эти тесты заменяют os_handle_tests.cpp из набора 2.1. Там проверялось, что
// конструктор бросает; здесь — что фабрика возвращает std::expected, а объект
// перемещается, не роняя хендл дважды.
//
// Замена, а не дополнение: конструктор стал приватным, и старые проверки
// больше не компилируются. Это нормальная цена смены канала отказа, и она
// заранее известна — набор 2.1 подключается на 4.2 без своего файла обёртки.
//
// Двойное освобождение тест поймать не может — его ловит санитайзер. Прогон
// под -fsanitize=address здесь не формальность: без него случай про
// перемещение проверяет только то, что код собрался.

#include <string>
#include <type_traits>
#include <utility>

#include "doctest.h"
#include "os_error.h"
#include "os_handle.h"

using nano_edr::OsError;
using nano_edr::OsErrorKind;
using nano_edr::OsHandle;
using nano_edr::WaitOutcome;

namespace {

std::string ScenarioPath(const std::string& name) {
    return std::string(NANO_EDR_SCENARIOS) + "/" + name;
}

int counted_events = 0;

void CountingCallback(const os_event*, void* context) noexcept {
    if (context != nullptr) {
        ++*static_cast<int*>(context);
    }
}

}  // namespace

TEST_CASE("правило пяти: копирования нет, перемещение есть") {
    // Копирование запрещено: две обёртки над одним os_handle* позвали бы
    // os_free дважды.
    CHECK_FALSE(std::is_copy_constructible_v<OsHandle>);
    CHECK_FALSE(std::is_copy_assignable_v<OsHandle>);

    // Перемещение обязано быть: без него фабрика не смогла бы вернуть объект
    // по значению. Это не украшение, это условие того, что Open вообще
    // компилируется.
    CHECK(std::is_move_constructible_v<OsHandle>);
    CHECK(std::is_move_assignable_v<OsHandle>);

    // И оба обязаны быть noexcept.
    CHECK(std::is_nothrow_move_constructible_v<OsHandle>);
    CHECK(std::is_nothrow_move_assignable_v<OsHandle>);
}

TEST_CASE("конструктор недоступен снаружи") {
    // Создать обёртку можно только через Open, то есть только с проверенным
    // хендлом. Состояния «объект есть, пользоваться нельзя» не существует.
    CHECK_FALSE(std::is_constructible_v<OsHandle, const std::string&>);
}

TEST_CASE("несуществующий файл — ошибка, а не исключение") {
    // Ключевой случай занятия. Отказ ожидаем, поэтому он в возвращаемом
    // значении, и посмотреть на него придётся.
    const auto handle = OsHandle::Open(ScenarioPath("нет-такого-файла.log"));
    REQUIRE_FALSE(handle.has_value());

    // Диагностика без причины бесполезна: сообщение обязано называть и путь,
    // и код границы.
    const std::string message = handle.error().Message();
    CHECK(message.find("нет-такого-файла") != std::string::npos);
    CHECK(message.size() > 20);
}

TEST_CASE("на исправном журнале Open даёт значение") {
    // Освобождение проверяет не этот тест, а санитайзер: под -fsanitize=address
    // незакрытый хендл виден как утечка.
    auto handle = OsHandle::Open(ScenarioPath("clean_office.log"));
    REQUIRE(handle.has_value());
}

TEST_CASE("перемещение не освобождает хендл дважды") {
    // Самая дорогая ошибка этого занятия: перемещающий конструктор, забывший
    // обнулить источник. Тест не увидит её сам — увидит санитайзер, поэтому
    // случай существует ради прогона под ASan.
    auto opened = OsHandle::Open(ScenarioPath("clean_office.log"));
    REQUIRE(opened.has_value());

    OsHandle moved(std::move(*opened));
    OsHandle assigned(std::move(moved));
    CHECK(true);
}

TEST_CASE("подписка, старт и ожидание доводят поток до конца") {
    auto opened = OsHandle::Open(ScenarioPath("clean_office.log"));
    REQUIRE(opened.has_value());
    OsHandle handle(std::move(*opened));

    counted_events = 0;
    REQUIRE(handle.Subscribe(CountingCallback, &counted_events).has_value());
    REQUIRE(handle.Start().has_value());

    // kTimeout — штатный исход, тик агента. Поэтому Wait возвращает исход
    // в значении, а не отказ: функция, у которой тик был бы ошибкой, была бы
    // непригодна в главном цикле.
    int guard = 0;
    for (;;) {
        const auto outcome = handle.Wait(100);
        REQUIRE(outcome.has_value());
        if (*outcome == WaitOutcome::kFinished) {
            break;
        }
        if (++guard > 100000) {
            FAIL("цикл ожидания не кончается");
            break;
        }
    }

    CHECK(counted_events > 0);
    handle.Stop();
}

TEST_CASE("подписка после старта — отказ, а не молчание") {
    auto opened = OsHandle::Open(ScenarioPath("clean_office.log"));
    REQUIRE(opened.has_value());
    OsHandle handle(std::move(*opened));

    REQUIRE(handle.Subscribe(CountingCallback, &counted_events).has_value());
    REQUIRE(handle.Start().has_value());

    // Нарушение порядка вызовов — ошибка кода, и вид отказа у неё свой:
    // kBadCall, а не «что-то пошло не так».
    const auto again = handle.Subscribe(CountingCallback, &counted_events);
    REQUIRE_FALSE(again.has_value());
    CHECK(again.error().kind == OsErrorKind::kBadCall);
}

TEST_CASE("Stop можно звать повторно") {
    // Остановка остановленного — нормальный ход событий, а не ошибка:
    // вызывающему всё равно нечем на неё ответить. Поэтому Stop —
    // единственная функция обёртки, которая ничего не возвращает.
    auto opened = OsHandle::Open(ScenarioPath("clean_office.log"));
    REQUIRE(opened.has_value());
    OsHandle handle(std::move(*opened));

    handle.Stop();
    handle.Stop();
    CHECK(true);
}

TEST_CASE("отказ действия несёт вид, а не только текст") {
    // На этом и стоит всё реагирование: у kNoSuchProcess, kPidReused
    // и kAccessDenied разные последствия, и различать их надо кодом,
    // а не разбором сообщения.
    auto opened = OsHandle::Open(ScenarioPath("clean_office.log"));
    REQUIRE(opened.has_value());
    OsHandle handle(std::move(*opened));

    const auto killed = handle.Kill(4294967295u, 0);
    REQUIRE_FALSE(killed.has_value());
    CHECK(killed.error().kind == OsErrorKind::kNoSuchProcess);

    const auto queried = handle.QueryProcess(4294967295u);
    REQUIRE_FALSE(queried.has_value());
    CHECK(queried.error().kind == OsErrorKind::kNoSuchProcess);
}
