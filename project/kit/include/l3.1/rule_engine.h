// Движок правил. Занятие 2.3.
//
// Всё, что он делает, — гоняет событие по правилам и сообщает, какие сработали.
// Ни печати, ни знания о том, что бывают детекты и телеметрия, ни про сеть.
//
// Печати здесь нет намеренно, и это третья редакция этого решения. На занятии
// 1.3 печать жила внутри CheckRules, потому что развести её было нечем.
// На 2.2 она переехала в агента. Теперь движок вообще не знает, что со сработкой
// делают: он отдаёт список сработавших правил и умолкает. На занятии 3.2
// на этом месте появится AlertSink, и движку опять не придётся меняться.
//
// Правило добавляется по владению: `AddRule(std::make_unique<MatchRule>(...))`.
// Движок владеет правилами, потому что больше некому: у правил есть состояние,
// и жить они должны столько же, сколько движок.

#ifndef NANO_EDR_KIT_RULE_ENGINE_H
#define NANO_EDR_KIT_RULE_ENGINE_H

#include <cstddef>
#include <memory>
#include <vector>

#include "event.h"
#include "rule.h"

namespace nano_edr {

class RuleEngine {
 public:
    RuleEngine() = default;

    // Владения — одно, копирования нет: движок с правилами не копируется
    // осмысленно, а перемещение появится на занятии 3.3.
    RuleEngine(const RuleEngine&) = delete;
    RuleEngine& operator=(const RuleEngine&) = delete;

    void AddRule(std::unique_ptr<IRule> rule);

    // Прогоняет событие по всем правилам.
    //
    // Возвращает число сработок. Если fired не nullptr, в него дописываются
    // указатели на сработавшие правила — невладеющие, действительные пока
    // жив движок.
    //
    // Список именно дописывается, а не заменяется: вызывающий вправе собирать
    // сработки за несколько событий. Очищать чужой контейнер — не дело
    // функции, которая его не создавала.
    std::size_t ProcessEvent(const Event& event,
                             std::vector<const IRule*>* fired);

    std::size_t size() const { return rules_.size(); }

    // Доступ к правилам для отчёта: сколько раз какое сработало.
    const IRule& rule(std::size_t index) const { return *rules_[index]; }

 private:
    std::vector<std::unique_ptr<IRule>> rules_;
};

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_RULE_ENGINE_H
