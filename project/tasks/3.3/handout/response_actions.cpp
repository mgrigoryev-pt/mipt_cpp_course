// Исполнение действий через границу os.h. Занятие 3.3. См. response.h.
//
// ЭТОТ ФАЙЛ ВЫДАН ГОТОВЫМ. Скопируйте его в `src/` — и дальше он ваш.
//
// Почему выдан. Здесь всё, что в исполнителе нельзя проверить выданными
// сценариями. Три функции действий устроены одинаково: вызвать границу,
// напечатать строку `[ACT]`, по коду возврата прибавить счётчик; формат
// строки прибит эталонами побайтово, а кодов кроме `OS_OK` открытые сценарии
// не дают. `ResolveStart` спрашивает систему, только когда модель не видела
// запуска процесса, а в открытых сценариях она видела все. `Recognize`
// на них не меняет ни одного детекта. Ваше в исполнителе — `Execute`
// в `response.cpp`.
//
// Прочитать всё равно стоит, и в первую очередь `Kill`. Без времени старта он
// границу не зовёт: ноль вместо времени означает «не проверять», и завершён
// был бы тот, у кого номер сейчас.

#include <cstdint>
#include <print>
#include <string>

#include "event.h"
#include "field_traits.h"
#include "fields.h"
#include "os.h"
#include "os_handle.h"
#include "response.h"

namespace nano_edr {
namespace {

// Номер процесса числом: в плане он строка, граница принимает uint32_t.
// Строка, которая не число, даёт ноль, а такого процесса у границы нет.
uint32_t PidNumber(const std::string& text) {
    uint64_t value = 0;
    if (!FieldTraits<uint64_t>::Parse(text, &value)) {
        return 0;
    }
    if (value > 0xFFFFFFFFull) {
        return 0;
    }
    return static_cast<uint32_t>(value);
}

}  // namespace

bool Responder::ResolveStart(const std::string& pid, Timestamp planned,
                             uint64_t* out) {
    if (planned.ms != 0) {
        *out = planned.ms;
        return true;
    }

    // Модель не знает времени старта: процесс существовал до подписки,
    // и события process_start про него не было. Спрашиваем систему — это
    // единственное место в агенте, где он её о чём-то спрашивает.
    os_process_info info{};
    ++stats_.queries;
    if (os_->ProcessInfo(PidNumber(pid), &info) != OS_OK) {
        return false;  // такого живого процесса нет — завершать некого
    }
    *out = info.start_time;
    return true;
}

void Responder::Kill(const Action& action) {
    uint64_t start = 0;
    if (!ResolveStart(action.pid, action.expected_start, &start)) {
        ++stats_.kill_gone;
        std::print("[ACT] kill pid={} — процесса уже нет\n", action.pid);
        return;
    }

    const os_status status = os_->Kill(PidNumber(action.pid), start);
    switch (status) {
        case OS_OK:
            ++stats_.killed;
            break;
        case OS_NO_SUCH_PROCESS:
            // Успел завершиться сам между детектом и действием.
            ++stats_.kill_gone;
            break;
        case OS_PID_REUSED:
            // Тот процесс завершился, номер достался другому. Повтора
            // с нулём вместо времени старта нет: он завершил бы непричастного.
            ++stats_.kill_reused;
            break;
        default:
            ++stats_.kill_denied;
            break;
    }
    std::print("[ACT] kill pid={} start={} {}\n", action.pid, start,
               os_status_str(status));
}

void Responder::MoveToQuarantine(const Action& action) {
    const os_status status = os_->MoveFile(action.path, action.to);
    switch (status) {
        case OS_OK:
            ++stats_.quarantined;
            break;
        case OS_BUSY:
            ++stats_.file_busy;
            break;
        case OS_NO_SUCH_FILE:
            ++stats_.file_gone;
            break;
        default:
            ++stats_.file_denied;
            break;
    }
    std::print("[ACT] quarantine {} -> {} {}\n", action.path, action.to,
               os_status_str(status));
}

void Responder::Delete(const Action& action) {
    const os_status status = os_->DeleteFile(action.path);
    switch (status) {
        case OS_OK:
            ++stats_.deleted;
            break;
        case OS_BUSY:
            // Файл занят живым процессом. Поэтому в плане завершения стоят
            // раньше действий с файлами.
            ++stats_.file_busy;
            break;
        case OS_NO_SUCH_FILE:
            ++stats_.file_gone;
            break;
        default:
            ++stats_.file_denied;
            break;
    }
    std::print("[ACT] delete {} {}\n", action.path, os_status_str(status));
}

bool Responder::Recognize(const Event& event) {
    // Поле actor есть только у process_end процесса, который завершили мы.
    // В нём наш собственный номер: os.h его иначе не сообщает.
    const std::string* actor = FindField(event, "actor");
    if (actor != nullptr && !actor->empty()) {
        if (self_pid_.empty()) {
            self_pid_ = *actor;
        }
        ++stats_.own_events;
        return true;
    }

    // Перенос и удаление файла приходят событиями с нашим номером в pid.
    if (!self_pid_.empty() && event.pid() == self_pid_) {
        ++stats_.own_events;
        return true;
    }

    return false;
}

}  // namespace nano_edr
