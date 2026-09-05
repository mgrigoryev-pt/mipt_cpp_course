// Заготовки занятия 4.2. См. l1.2.cpp.
//
// Границы заворачиваются в std::expected: появились os_error.h и загрузчик
// правил из файла. Конструктор OsHandle уехал в приватные, наружу торчит
// статическая фабрика.

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
#include "os_error.h"
#include "os_handle.h"
#include "parse.h"
#include "response.h"
#include "rule.h"
#include "rule_base.h"
#include "rule_loader.h"
#include "rules.h"
#include "window_entry.h"
