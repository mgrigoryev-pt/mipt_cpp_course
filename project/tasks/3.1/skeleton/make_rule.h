// Заготовка фабрики правил. Занятие 3.1.
//
// Скопируйте файл в `src/` и допишите тело. С этого момента он ваш.
//
// Смысл фабрики не в экономии букв, а в одном месте, где правило создаётся:
// если завтра правила начнут регистрироваться в реестре или получать общий
// префикс идентификатора, править придётся здесь, а не в каждом вызове.
//
// Два параметра шаблона разной природы, и это видно по тому, как они
// задаются. `Rule` называется явно — `MakeRule<ThresholdRule<128>>(...)`, —
// потому что из аргументов его вывести неоткуда. `Args...` выводится
// из вызова и не пишется никогда.

#ifndef NANO_EDR_MAKE_RULE_H
#define NANO_EDR_MAKE_RULE_H

#include <memory>
#include <string>

#include "rule.h"

namespace nano_edr {

template <typename Rule, typename... Args>
std::unique_ptr<Rule> MakeRule(const std::string& id, Severity severity,
                               Args... args);

}  // namespace nano_edr

#endif  // NANO_EDR_MAKE_RULE_H
