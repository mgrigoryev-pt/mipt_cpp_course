// Правило как класс. Занятие 2.2.
//
// До этого правило было указателем на функцию, а всё, что правилу нужно помнить,
// лежало рядом в структуре: идентификатор, важность, действия. Как только
// у правила появляется состояние — хотя бы счётчик срабатываний, — такое
// разделение перестаёт работать: два места надо держать согласованными,
// и однажды они разойдутся.
//
// Отсюда наследование, и здесь оно применено ровно по назначению: RuleBase —
// то, что есть у **всякого** правила независимо от способа проверки; MatchRule —
// один такой способ. На занятии 3.1 появятся SequenceRule и ThresholdRule,
// и общая часть у них будет та же.
//
// Метод Check **невиртуальный**, и это временно. Виртуальность — лекция 7,
// то есть следующее занятие. Пока это значит, что правила приходится хранить
// вектором конкретного типа: `std::vector<MatchRule>`, а не
// `std::vector<RuleBase>`.
//
// Вопрос «почему нельзя вектор базового класса» — главный вопрос занятия,
// и ответ на него стоит уметь произнести: вектор хранит объекты по значению,
// объект базового класса меньше объекта наследника, и при копировании
// в него от наследника останется только базовая часть. Это называется
// срезкой, компилятор про неё молчит, а вектор условий из MatchRule
// при этом просто исчезнет.

#ifndef NANO_EDR_KIT_RULE_H
#define NANO_EDR_KIT_RULE_H

#include <cstddef>
#include <string>
#include <vector>

#include "conditions.h"
#include "event.h"
#include "rules.h"

namespace nano_edr {

// Общая часть всякого правила.
class RuleBase {
 public:
    RuleBase(const std::string& id, Severity severity);

    const std::string& id() const { return id_; }
    Severity severity() const { return severity_; }

    // Сколько раз правило сработало за прогон. Нужно для отчёта: правило,
    // не сработавшее ни разу за весь набор сценариев, скорее всего написано
    // неверно, и увидеть это дешевле, чем узнать на разборе.
    std::size_t hits() const { return hits_; }

    // Что делать по детекту. Пока просто имена — "kill_process",
    // "quarantine_file". Настоящие действия появятся на занятии 3.3, когда
    // будет чем их выполнять: реагирование идёт через границу os.h.
    const std::vector<std::string>& actions() const { return actions_; }
    void AddAction(const std::string& action);

 protected:
    // Наследник считает срабатывание сам: только он знает, когда оно
    // случилось. Поле при этом остаётся приватным — protected-метод
    // и protected-поле это очень разные вещи по цене.
    void CountHit() { ++hits_; }

 private:
    std::string id_;
    Severity severity_;
    std::vector<std::string> actions_;
    std::size_t hits_ = 0;
};

// Правило-совпадение: все условия должны выполниться на одном событии.
class MatchRule : public RuleBase {
 public:
    MatchRule(const std::string& id, Severity severity);

    void AddCondition(const Condition& condition);

    const std::vector<Condition>& conditions() const { return conditions_; }

    // Все условия выполнены — сработало. Пустой набор условий срабатывает
    // на всём, и это не защищено намеренно: правило без условий — ошибка
    // автора правила, а не случай, который надо обрабатывать.
    //
    // Метод неконстантный: он считает срабатывание. Альтернатива —
    // mutable-счётчик и const-метод — прячет изменение состояния за словом
    // const, и это хуже.
    bool Check(const Event& event);

 private:
    std::vector<Condition> conditions_;
};

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_RULE_H
