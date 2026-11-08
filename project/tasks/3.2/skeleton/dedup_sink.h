// Заготовка получателя, подавляющего дубликаты. Занятие 3.2.
//
// Скопируйте файл в `src/` и допишите приватные поля. Методы реализуются
// в `src/sinks.cpp`, как у остальных получателей. С этого момента файл ваш.
//
// ЧТО ЗДЕСЬ ЛЕЖИТ
//
// Публичный интерфейс целиком, без реализации. Интерфейс менять нельзя —
// против него собраны выданные тесты. Приватной части здесь нет: чем помнить
// уже пропущенные детекты, решаете вы, и выбор этот не единственный
// правильный. Словарей ещё нет — они на занятии 4.1.
//
// Как работает подавление — таблица шага 8 в постановке занятия.

#ifndef NANO_EDR_DEDUP_SINK_H
#define NANO_EDR_DEDUP_SINK_H

#include <cstddef>
#include <cstdint>

#include "alert_sink.h"
#include "detection.h"

namespace nano_edr {

class DedupSink {
 public:
    // inner — куда пропускать; period_ms — на сколько замолкать; suppressed —
    // куда считать подавленные, может быть nullptr.
    DedupSink(AlertSink inner, uint64_t period_ms, std::size_t* suppressed);

    void operator()(const Detection& detection);
};

}  // namespace nano_edr

#endif  // NANO_EDR_DEDUP_SINK_H
