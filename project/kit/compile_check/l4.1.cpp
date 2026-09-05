// Заготовки занятия 4.1. См. l1.2.cpp.
//
// Список из 1.2 удалён: его место заняли стандартные контейнеры. Модель
// сущностей стоит на словарях с ключом-парой и специализацией std::hash,
// у правил появился тик для вытеснения.

#include "alert_sink.h"
#include "combinators.h"
#include "conditions.h"
#include "copy_stats.h"
#include "detection.h"
#include "edr_handle.h"
#include "entity_model.h"
#include "event.h"
#include "event_source.h"
#include "field_traits.h"
#include "fields.h"
#include "function.h"
#include "os_handle.h"
#include "parse.h"
#include "response.h"
#include "ring_buffer.h"
#include "rule.h"
#include "rule_engine.h"
#include "rules.h"
#include "window_entry.h"
