// Заготовка получателя, подавляющего дубликаты. Занятие 3.2.
//
// Скопируйте файл в `src/` и допишите реализацию — она уезжает в `.cpp`,
// как у остальных получателей. С этого момента файл ваш.
//
// ЧТО ЗДЕСЬ ЛЕЖИТ
//
// Публичный интерфейс целиком и ни одного тела. Интерфейс менять нельзя —
// против него собраны выданные тесты. Приватной части здесь нет: чем помнить
// уже пропущенные детекты, решаете вы, и выбор этот не единственный
// правильный. Словарей ещё нет — они на занятии 4.1.
//
// Пять правил подавления и то, что каждое из них закрывает, — в постановке
// занятия.

#ifndef NANO_EDR_DEDUP_SINK_H
#define NANO_EDR_DEDUP_SINK_H

#include <cstddef>
#include <cstdint>

#include "alert_sink.h"
#include "detection.h"

namespace nano_edr {

class DedupSink {
 public:
    // inner — куда пропускать; window_ms — на сколько замолкать; suppressed —
    // куда считать подавленные, может быть nullptr.
    DedupSink(AlertSink inner, uint64_t window_ms, std::size_t* suppressed);

    void operator()(const Detection& detection);
};

}  // namespace nano_edr

#endif  // NANO_EDR_DEDUP_SINK_H
