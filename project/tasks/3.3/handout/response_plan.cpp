// План реагирования: действие и список действий. Занятие 3.3. См. response.h.
//
// ЭТОТ ФАЙЛ ВЫДАН ГОТОВЫМ. Скопируйте его в `src/` — и дальше он ваш.
//
// Почему выдан. Здесь арифметика плана и формат печати. Формат прибит
// эталонами сценариев побайтово, то есть выбирать в нём нечего, а
// арифметика — это `Add`, `Has` и таблица имён. Работа занятия начинается
// в `Plan()`: чтó попадает в план и в каком порядке — ваше, и это вы пишете
// в `response.cpp`.
//
// Прочитать всё равно стоит, и в первую очередь `Add`. Из него следует
// требование к вашему `Plan()`: при совпадении цели побеждает **первое**
// добавленное действие. Файл-источник процесса часто им же и создан, то есть
// попадает и под изоляцию, и под удаление, — и порядок сбора обязан
// поставить первым более осторожное действие. Обратный порядок означает,
// что улику вы уничтожили.

#include <cstddef>
#include <format>
#include <string>
#include <utility>

#include "response.h"

namespace nano_edr {

const char* ActionKindName(ActionKind kind) {
    switch (kind) {
        case ActionKind::kKillProcess:
            return "kill";
        case ActionKind::kQuarantineFile:
            return "quarantine";
        case ActionKind::kDeleteFile:
            return "delete";
    }
    return "unknown";
}

std::string ToString(const Action& action) {
    std::string text = ActionKindName(action.kind);
    if (action.kind == ActionKind::kKillProcess) {
        text += std::format(" pid={} start={}", action.pid,
                            action.expected_start.ms);
    } else {
        text += " " + action.path;
        if (!action.to.empty()) {
            text += " -> " + action.to;
        }
    }
    if (!action.reason.empty()) {
        text += "  (" + action.reason + ")";
    }
    return text;
}

bool ResponsePlan::Has(const std::string& target) const {
    for (std::size_t i = 0; i < actions_.size(); ++i) {
        if (actions_[i].target() == target) {
            return true;
        }
    }
    return false;
}

bool ResponsePlan::Add(Action action) {
    if (action.target().empty()) {
        return false;  // цели нет — нечего делать
    }
    if (Has(action.target())) {
        // Одна цель — одно действие, и побеждает первое. Порядок в Plan
        // выстроен так, чтобы первым оказывалось более осторожное: файл,
        // попавший и под изоляцию, и под удаление, будет изолирован.
        // Обратный порядок означал бы, что улику мы уничтожили.
        return false;
    }
    // emplace_back вместо push_back, и action перемещается, а не копируется:
    // внутри четыре строки, и на каждое действие плана это четыре аллокации
    // разницы. Действий десятки, а не миллионы, — но привычка ставить move
    // там, где источник больше не нужен, дешевле привычки вспоминать, где
    // это важно.
    actions_.emplace_back(std::move(action));
    return true;
}

}  // namespace nano_edr
