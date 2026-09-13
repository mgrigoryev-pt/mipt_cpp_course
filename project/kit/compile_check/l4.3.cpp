// Заготовки занятия 4.3. См. l1.2.cpp.
//
// Добавился reporter.h — поток отправки детектов наверх. Счётчики копий
// стали атомарными, записи модели живут под shared_ptr.

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
#include "os_error.h"
#include "os_handle.h"
#include "parse.h"
#include "reporter.h"
#include "response.h"
#include "ring_buffer.h"
#include "rule.h"
#include "rule_engine.h"
#include "rule_loader.h"
#include "rule_text.h"
#include "rules.h"
