// Комбинаторы условий на шаблонах. Занятие 3.1.
//
// То же самое, что AllOf и AnyOf с занятия 2.3, но решение о составе принимает
// компилятор, а не вызывающий. Отсюда и имена: суффикс Static говорит **когда**
// определяется состав, а не что-то про хранение.
//
// Разница по цене видна в подписи. Динамический AllOf хранит
// vector<unique_ptr<ICondition>>: аллокация на каждое условие и виртуальный
// вызов на каждую проверку. AllOfStatic<Cs...> хранит tuple<Cs...>: ни одной
// аллокации и ни одного виртуального вызова — всё встраивается.
//
// Разница по возможностям — обратная. Динамический набор собирается во время
// работы: правила можно прочитать из файла, чего на занятии 3.2 и захочется.
// Статический известен на этапе компиляции, и файлом его не задать.
//
// Ни один из двух не лучше. Продвинутая часть занятия — замерить оба
// на нагрузочном сценарии и назвать разницу числом.

#ifndef NANO_EDR_KIT_COMBINATORS_H
#define NANO_EDR_KIT_COMBINATORS_H

#include <memory>
#include <tuple>
#include <utility>

#include "conditions.h"
#include "event.h"

namespace nano_edr {

// Все условия выполнены. Состав задан типом.
template <typename... Conditions>
class AllOfStatic {
 public:
    explicit AllOfStatic(Conditions... parts) : parts_(parts...) {}

    bool operator()(const Event& event) const {
        // std::apply разворачивает tuple в аргументы, а fold-выражение
        // (лекция 9) склеивает их оператором &&. Ни цикла, ни рекурсии,
        // ни виртуального вызова: компилятор раскрывает это в цепочку
        // проверок с коротким замыканием.
        //
        // Пустой набор даёт true — нейтральный элемент для &&, и это то же
        // соглашение, что у динамического AllOf.
        return std::apply(Apply{event}, parts_);
    }

 private:
    // Функтор, а не лямбда, — лямбды будут на лекции 10. Разница только
    // в записи: лямбда и есть автогенерируемый функтор с operator(),
    // и на занятии 3.2 эти пять строк схлопнутся в одну.
    struct Apply {
        const Event& event;
        bool operator()(const Conditions&... parts) const {
            return (parts(event) && ...);
        }
    };

    std::tuple<Conditions...> parts_;
};

// Хотя бы одно условие выполнено.
template <typename... Conditions>
class AnyOfStatic {
 public:
    explicit AnyOfStatic(Conditions... parts) : parts_(parts...) {}

    bool operator()(const Event& event) const {
        // Пустой набор даёт false — нейтральный элемент для ||. Асимметрия
        // с AllOfStatic не выдумана: она следует из самих операторов.
        return std::apply(Apply{event}, parts_);
    }

 private:
    struct Apply {
        const Event& event;
        bool operator()(const Conditions&... parts) const {
            return (parts(event) || ...);
        }
    };

    std::tuple<Conditions...> parts_;
};

// Мост между двумя мирами: любой вызываемый объект превращается в ICondition.
//
// Нужен затем, что движок правил разговаривает через интерфейс, а комбинатор
// на шаблонах интерфейса не реализует и не должен: реализовав его, он потерял
// бы ровно то, за чем создан. Поэтому виртуальный вызов остаётся, но один —
// на весь составной комбинатор, а не на каждое условие внутри.
template <typename Callable>
class ConditionOf : public ICondition {
 public:
    explicit ConditionOf(Callable callable) : callable_(callable) {}

    bool Matches(const Event& event) const override { return callable_(event); }

 private:
    Callable callable_;
};

// Обёртка с выводом типа: MakeCondition(AllOfStatic{a, b}) вместо
// std::make_unique<ConditionOf<AllOfStatic<A, B>>>(...).
template <typename Callable>
std::unique_ptr<ICondition> MakeCondition(Callable callable) {
    return std::make_unique<ConditionOf<Callable>>(callable);
}

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_COMBINATORS_H
