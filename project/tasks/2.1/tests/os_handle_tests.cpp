// Тесты обёртки над границей os.h. Занятие 2.1.
//
// Проверяется не симулятор, а обёртка: что конструктор бросает вместо того,
// чтобы отдать полуживой объект, что копирования нет, что деструктор
// освобождает хендл.
//
// Утечку хендла тест поймать не может — её ловит санитайзер. Поэтому прогон
// под -fsanitize=address здесь не формальность: без него один из случаев ниже
// не проверяет ничего.

#include <stdexcept>
#include <string>
#include <type_traits>

#include "doctest.h"
#include "os_handle.h"

using nano_edr::OsHandle;

namespace {

std::string ScenarioPath(const std::string& name) {
    // Каталог сценариев передан сборкой: тест не должен знать, где лежит
    // репозиторий.
    return std::string(NANO_EDR_SCENARIOS) + "/" + name;
}

int counted_events = 0;

void CountingCallback(const os_event*, void* context) noexcept {
    if (context != nullptr) {
        ++*static_cast<int*>(context);
    }
}

}  // namespace

TEST_CASE("хендл нельзя копировать") {
    // Проверка на этапе компиляции: две обёртки над одним os_handle* позвали бы
    // os_free дважды. Запретить дешевле, чем придумывать, что копия значит.
    CHECK_FALSE(std::is_copy_constructible_v<OsHandle>);
    CHECK_FALSE(std::is_copy_assignable_v<OsHandle>);
}

TEST_CASE("несуществующий файл — исключение из конструктора") {
    // Ключевой случай занятия. У конструктора нет канала для кода возврата,
    // значит либо объект, либо исключение. Никакого is_valid().
    CHECK_THROWS_AS(OsHandle(ScenarioPath("нет-такого-файла.log")),
                    std::runtime_error);
}

TEST_CASE("сообщение исключения называет путь и причину") {
    // Диагностика без причины бесполезна: «не удалось инициализировать»
    // не отвечает ни на один вопрос.
    try {
        const OsHandle handle(ScenarioPath("нет-такого-файла.log"));
        FAIL("исключения не было");
    } catch (const std::runtime_error& e) {
        const std::string message = e.what();
        CHECK(message.find("нет-такого-файла") != std::string::npos);
        CHECK(message.size() > 20);  // не пустая заглушка
    }
}

TEST_CASE("на исправном журнале хендл создаётся и освобождается") {
    // Освобождение проверяет не этот тест, а санитайзер: под -fsanitize=address
    // незакрытый хендл виден как утечка. Без санитайзера случай проверяет
    // только то, что конструктор не бросил.
    {
        const OsHandle handle(ScenarioPath("clean_office.log"));
        CHECK(true);
    }
    CHECK(true);
}

TEST_CASE("подписка, старт и ожидание доводят поток до конца") {
    OsHandle handle(ScenarioPath("clean_office.log"));

    counted_events = 0;
    handle.Subscribe(CountingCallback, &counted_events);
    handle.Start();

    // OS_TIMEOUT — штатный исход, тик агента. Поэтому Wait возвращает код,
    // а не бросает: функция, которая бросала бы на нём, была бы непригодна.
    os_status status = OS_TIMEOUT;
    int guard = 0;
    while ((status = handle.Wait(100)) == OS_TIMEOUT) {
        if (++guard > 100000) {
            FAIL("цикл ожидания не кончается");
            break;
        }
    }

    CHECK(status == OS_OK);
    CHECK(counted_events > 0);

    handle.Stop();
}

TEST_CASE("Stop можно звать повторно") {
    // Остановка остановленного — нормальный ход событий, а не ошибка:
    // вызывающему всё равно нечем на неё ответить.
    OsHandle handle(ScenarioPath("clean_office.log"));

    handle.Stop();
    handle.Stop();
    CHECK(true);
}
