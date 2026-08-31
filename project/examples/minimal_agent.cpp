// Минимальный агент: подписаться на поток событий и печатать их. С него
// начинается занятие 2.1 — дальше на этом месте вырастает nano-edr.
#include <exception>
#include <print>

#include "os.h"

namespace {

// Колбэк помечен noexcept, потому что раскрутка стека через кадры C —
// неопределённое поведение: исключение обязано умереть внутри.
void OnEvent(const os_event* ev, void* ctx) noexcept {
    ++*static_cast<int*>(ctx);
    std::print("{:>14} {:<14} pid={}", ev->ts, ev->type, ev->pid);
    for (size_t i = 0; i < ev->field_count; ++i) {
        std::print(" {}={}", ev->fields[i].key, ev->fields[i].value);
    }
    std::print("\n");
}

}  // namespace

int main(int argc, char** argv) try {
    if (argc != 2) {
        std::print("использование: minimal_agent <сценарий.cfg | журнал.log>\n");
        return 2;
    }

    os_handle* h = nullptr;
    if (const os_status s = os_init(argv[1], &h); s != OS_OK) {
        std::print("os_init: {}\n", os_status_str(s));
        return 1;
    }

    int seen = 0;
    os_status status = os_event_subscribe(h, OnEvent, &seen);
    if (status == OS_OK) status = os_start(h);
    if (status == OS_OK) {
        // Таймаут здесь — тик агента: место для вытеснения старых записей.
        while ((status = os_wait(h, 1000)) == OS_TIMEOUT) {
        }
    }
    os_free(h);  // подразумевает os_stop и пишет отчёт

    std::print("событий: {}, итог: {}\n", seen, os_status_str(status));
    return status == OS_OK ? 0 : 1;
} catch (const std::exception& e) {
    // Последний рубеж: программа обязана сообщить причину, а не умереть молча.
    std::print("необработанное исключение: {}\n", e.what());
    return 1;
}
