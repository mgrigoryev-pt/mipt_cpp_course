// Условия как иерархия. Занятие 2.3.
//
// Обещание занятия 2.2 выполнено: `switch` внутри условия исчез, четыре класса
// вернулись — но уже наследниками общего интерфейса.
//
// Что изменилось и почему. На 2.2 условие пришлось свести к одному классу
// с переключателем вида сравнения: вектор разнородных функторов не выражался
// тем, что было пройдено. Появился `virtual` — и хранить разные типы стало
// можно, потому что хранятся не объекты, а указатели, а указатель одного
// размера всегда.
//
// Цена видна тут же: `std::vector<std::unique_ptr<ICondition>>` вместо
// `std::vector<Condition>`. Аллокация на каждое условие, косвенность
// на каждую проверку. За это получено то, что новый вид условия добавляется
// **новым классом**, не трогая ни одной существующей строки, — а с
// переключателем пришлось бы править и enum, и switch, и все места, где
// они перечислены.
//
// operator() оставлен намеренно, невиртуальным, поверх Matches. Условие
// остаётся функтором, и код, написанный на занятии 2.1, продолжает
// компилироваться — включая выданные тогда тесты.

#ifndef NANO_EDR_KIT_CONDITIONS_H
#define NANO_EDR_KIT_CONDITIONS_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "event.h"

namespace nano_edr {

// Интерфейс условия.
class ICondition {
 public:
    // Виртуальный деструктор обязателен, и это не формальность. Условия
    // хранятся как unique_ptr<ICondition>; когда такой указатель разрушается,
    // без virtual вызовется деструктор ICondition, а деструктор наследника —
    // нет. Строки внутри условия при этом утекут, и санитайзер это покажет.
    // Уберите virtual и посмотрите — один раз это стоит увидеть.
    virtual ~ICondition() = default;

    virtual bool Matches(const Event& event) const = 0;

    // Условие остаётся функтором: так его можно вызвать и через интерфейс,
    // и напрямую по конкретному типу.
    bool operator()(const Event& event) const { return Matches(event); }
};

// Поле равно значению целиком. Без учёта регистра — в файловой системе
// Windows «A.JS» и «a.js» один файл.
class FieldEquals : public ICondition {
 public:
    FieldEquals(const std::string& key, const std::string& value)
        : key_(key), value_(value) {}

    bool Matches(const Event& event) const override;

 private:
    std::string key_;
    std::string value_;
};

// Поле содержит подстроку.
class FieldContains : public ICondition {
 public:
    FieldContains(const std::string& key, const std::string& needle)
        : key_(key), needle_(needle) {}

    bool Matches(const Event& event) const override;

 private:
    std::string key_;
    std::string needle_;
};

// Поле заканчивается на суффикс.
class FieldEndsWith : public ICondition {
 public:
    FieldEndsWith(const std::string& key, const std::string& suffix)
        : key_(key), suffix_(suffix) {}

    bool Matches(const Event& event) const override;

 private:
    std::string key_;
    std::string suffix_;
};

// Поле равно одному из перечисленных значений.
//
// Формально это FieldEquals со списком, и на занятии 2.2, когда все условия
// свелись к одному классу с переключателем, это стало видно. Отдельным классом
// он оставлен потому, что читается лучше: «image — один из скриптовых хостов»
// и «image равен wscript.exe или image равен cscript.exe» это одна мысль,
// записанная по-разному.
class FieldInList : public ICondition {
 public:
    FieldInList(const std::string& key, const std::vector<std::string>& values)
        : key_(key), values_(values) {}

    bool Matches(const Event& event) const override;

 private:
    std::string key_;
    std::vector<std::string> values_;
};

// Тип события равен одному из перечисленных.
//
// Появилось вместе с EventType: сравнение перечислений вместо сравнения строк.
// Опечатку в имени типа теперь заметит компилятор, а не отчёт о пропущенной
// атаке.
class EventTypeIs : public ICondition {
 public:
    explicit EventTypeIs(const std::vector<EventType>& types) : types_(types) {}

    bool Matches(const Event& event) const override;

 private:
    std::vector<EventType> types_;
};


// ---------------------------------------------------------------------------
// Условия из условий
// ---------------------------------------------------------------------------
//
// Первое, что даёт интерфейс и чего не давал переключатель: условие может
// содержать другие условия, ничего не зная об их видах. Классы ниже написаны
// один раз и работают со всем, что появится потом, включая самих себя.
//
// С переключателем такое не выражается вовсе: enum пришлось бы расширить
// вариантом «составное», а в switch появилась бы рекурсия по структуре,
// которую этот же switch и определяет.
//
// Вариадический AllOf на шаблонах и правила с окном — занятие 3.1. Здесь
// вектор и виртуальный вызов: того же результата достаточно, чтобы увидеть
// саму идею.

// Все вложенные условия выполнены.
class AllOf : public ICondition {
 public:
    void Add(std::unique_ptr<ICondition> condition);

    // Пустой набор — истина. Это не произвол: «все из ничего выполнены» —
    // корректное утверждение, и оно делает AllOf нейтральным элементом.
    bool Matches(const Event& event) const override;

    std::size_t size() const { return parts_.size(); }

 private:
    std::vector<std::unique_ptr<ICondition>> parts_;
};

// Хотя бы одно вложенное условие выполнено.
class AnyOf : public ICondition {
 public:
    void Add(std::unique_ptr<ICondition> condition);

    // Пустой набор — ложь, и асимметрия с AllOf здесь намеренная: «хотя бы
    // одно из ничего» — утверждение ложное. Проверьте на бумаге, что иначе
    // AnyOf ломал бы правило, у которого забыли заполнить вариант.
    bool Matches(const Event& event) const override;

    std::size_t size() const { return parts_.size(); }

 private:
    std::vector<std::unique_ptr<ICondition>> parts_;
};

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_CONDITIONS_H
