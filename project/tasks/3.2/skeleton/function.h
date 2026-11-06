// Заготовка своего Function. Занятие 3.2.
//
// Скопируйте файл в `src/` и допишите четыре места, отмеченных `// TODO`.
// С этого момента он ваш: правьте что угодно, включая этот комментарий.
//
// Устройство то же, что у BinaryOp в лекции 10, раздел «Type erasure
// руками»: база с виртуальным вызовом, шаблонный наследник под каждый тип
// callable, указатель на базу в обёртке. Разница одна: там сигнатура
// `int(int, int)`, здесь любая — `R(Args...)`.
//
// Публичный интерфейс менять нельзя: против него собраны выданные тесты
// и выданные `alert_sink.h` и `rule.h`.

#ifndef NANO_EDR_FUNCTION_H
#define NANO_EDR_FUNCTION_H

#include <memory>
#include <utility>

namespace nano_edr {

// Объявлен, но не определён: определена только частичная специализация
// ниже, и `Function` пишется с сигнатурой в скобках —
// `Function<void(const Detection&)>`.
template <typename Signature>
class Function;

template <typename R, typename... Args>
class Function<R(Args...)> {
 public:
    Function() = default;

    // std::move — лекция 11. Без него не собрался бы получатель, который
    // не копируется, например FileSink с std::ofstream внутри.
    template <typename F>
    Function(F callable)  // NOLINT
        : impl_(std::make_unique<CallableHolder<F>>(std::move(callable))) {}

    R operator()(Args... args) const {
        // TODO: вызвать Call через impl_ с теми же аргументами
    }

    explicit operator bool() const {
        // TODO: пустой Function — false
    }

 private:
    class ICallable {
     public:
        virtual ~ICallable() = default;
        // TODO: чисто виртуальный Call — та же сигнатура, что у operator()
    };

    template <typename F>
    class CallableHolder : public ICallable {
     public:
        explicit CallableHolder(F f) : f_(std::move(f)) {}

        // TODO: Call с override — вызвать f_ с теми же аргументами

     private:
        F f_;
    };

    std::unique_ptr<ICallable> impl_;
};

}  // namespace nano_edr

#endif  // NANO_EDR_FUNCTION_H
