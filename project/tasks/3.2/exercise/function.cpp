// Стенд занятия 3.2: ваш Function против std::function и трёх других
// способов позвать чужой код.
//
// Программа ничего не реализует — она меряет, и показывает, что «своё короче»
// и «своё быстрее» это разные утверждения. Свой Function берётся
// из src/function.h, то есть из того, что вы написали на паре; пока этого
// файла нет, стенд не соберётся.
//
// Порядок работы:
//   1. Найти в своём коде обе цены стирания типа: виртуальный вызов
//      и аллокацию. И то, и то по одному.
//   2. Предсказать на бумаге: сколько аллокаций сделает каждый из двух
//      на маленькой лямбде и на большой. Четыре числа, и одинаковых среди них
//      меньше, чем кажется.
//   3. Предсказать, кто быстрее на вызове. Ответ неочевидный.
//   4. Запустить, сверить, объяснить.
//
// Сборка — из каталога, где лежит этот файл:
//   g++ -std=c++23 -O2 -Wall -Wextra -I ../../../src -o function function.cpp
//   cl /nologo /std:c++latest /O2 /W4 /EHsc /I ..\..\..\src function.cpp
//
// Ключ -I (у cl это /I) указывает, где искать function.h: путь ведёт
// в src/ вашего проекта.
//
// Только Release. В отладочной сборке не встроено ничего, и замер измеряет
// работу отладочных проверок, а не стоимость косвенности.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <print>

#include "function.h"

using nano_edr::Function;

// ---------------------------------------------------------------------------
// Счётчик аллокаций
// ---------------------------------------------------------------------------

namespace {
std::size_t allocations = 0;
bool counting = false;
}  // namespace

void* operator new(std::size_t size) {
    if (counting) {
        ++allocations;
    }
    void* p = std::malloc(size);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t size) { return operator new(size); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// ---------------------------------------------------------------------------
// Свой Function — из вашего проекта
// ---------------------------------------------------------------------------
//
// Копии здесь нет намеренно: мерить надо то, что поедет в агента, а не
// четвёртый экземпляр того же кода. Приём, напомним, тот же, что у ICondition
// на занятии 2.3 — виртуальный вызов через базовый класс. Разница в том, что
// от вызываемого объекта НЕ требуется наследовать интерфейс: наследника делает
// за него шаблон, и поэтому лямбда, которая ничего наследовать не может, тоже
// подходит.

// ---------------------------------------------------------------------------
// С чем сравнивать
// ---------------------------------------------------------------------------
//
// Три способа позвать чужой код, кроме двух стирающих тип: прямой вызов,
// указатель на функцию и виртуальный вызов через интерфейс. Первый — предел
// возможного, остальные — то, что было доступно до этого занятия.

namespace {

std::uint64_t Direct(std::uint64_t x) { return x * 3 + 1; }

// Второй вариант с тем же результатом и другим адресом. Он существует ровно
// затем, чтобы компилятор не знал, какая функция будет вызвана: иначе он
// подставит тело в цикл, развернёт его векторными командами, и замер покажет
// ноль. Ноль этот не ложь — вызова там действительно нет, — но измерять им
// стоимость косвенности нельзя.
std::uint64_t DirectAlt(std::uint64_t x) { return x * 3 + 1; }

using RawPointer = std::uint64_t (*)(std::uint64_t);

struct ITransform {
    virtual ~ITransform() = default;
    virtual std::uint64_t Apply(std::uint64_t x) const = 0;
};

struct Triple : ITransform {
    std::uint64_t Apply(std::uint64_t x) const override { return x * 3 + 1; }
};

// По той же причине, что DirectAlt: с одним наследником компилятор доказывает,
// какой Apply будет вызван, и виртуальный вызов исчезает. Это называется
// девиртуализацией, и знать про неё стоит: измерять «стоимость virtual»
// на иерархии из одного класса бессмысленно.
struct TripleAlt : ITransform {
    std::uint64_t Apply(std::uint64_t x) const override { return x * 3 + 1; }
};

constexpr int kCalls = 20000000;

double NanosPerCall(std::chrono::steady_clock::duration elapsed) {
    return static_cast<double>(
               std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)
                   .count()) /
           kCalls;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argv;
    // Выбор, который известен только во время работы. Значение всегда одно
    // и то же — argc равен единице, — но компилятор этого не знает, и вызовы
    // ниже остаются настоящими вызовами.
    const bool alternative = argc > 1;

    std::uint64_t checksum = 0;

    // -----------------------------------------------------------------------
    std::print("=== Аллокации при создании ===\n");

    // Маленький захват: одно число. Ровно тот случай, под который в стандартной
    // библиотеке сделана оптимизация малых объектов.
    {
        const std::uint64_t k = 3;
        counting = true;
        allocations = 0;
        Function<std::uint64_t(std::uint64_t)> mine(
            [k](std::uint64_t x) { return x * k + 1; });
        const std::size_t mine_alloc = allocations;

        allocations = 0;
        std::function<std::uint64_t(std::uint64_t)> theirs(
            [k](std::uint64_t x) { return x * k + 1; });
        const std::size_t theirs_alloc = allocations;
        counting = false;

        checksum += mine(1) + theirs(1);
        std::print("  захват 8 байт:   свой {}, std::function {}\n", mine_alloc,
                   theirs_alloc);
    }

    // Большой захват: массив, который в маленький буфер не влезет.
    {
        struct Big {
            std::uint64_t pad[8];
        };
        Big big{};
        big.pad[0] = 3;

        counting = true;
        allocations = 0;
        Function<std::uint64_t(std::uint64_t)> mine(
            [big](std::uint64_t x) { return x * big.pad[0] + 1; });
        const std::size_t mine_alloc = allocations;

        allocations = 0;
        std::function<std::uint64_t(std::uint64_t)> theirs(
            [big](std::uint64_t x) { return x * big.pad[0] + 1; });
        const std::size_t theirs_alloc = allocations;
        counting = false;

        checksum += mine(1) + theirs(1);
        std::print("  захват 64 байта: свой {}, std::function {}\n", mine_alloc,
                   theirs_alloc);
    }

    // -----------------------------------------------------------------------
    std::print("\n=== Время вызова, {} вызовов ===\n", kCalls);

    {
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t total = 0;
        for (int i = 0; i < kCalls; ++i) {
            total += Direct(static_cast<std::uint64_t>(i));
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        checksum += total;
        std::print("  прямой вызов:      {:>6.2f} нс\n", NanosPerCall(elapsed));
    }

    {
        RawPointer fn = alternative ? &DirectAlt : &Direct;
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t total = 0;
        for (int i = 0; i < kCalls; ++i) {
            total += fn(static_cast<std::uint64_t>(i));
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        checksum += total;
        std::print("  указатель:         {:>6.2f} нс\n", NanosPerCall(elapsed));
    }

    {
        std::unique_ptr<ITransform> transform;
        if (alternative) {
            transform = std::make_unique<TripleAlt>();
        } else {
            transform = std::make_unique<Triple>();
        }
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t total = 0;
        for (int i = 0; i < kCalls; ++i) {
            total += transform->Apply(static_cast<std::uint64_t>(i));
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        checksum += total;
        std::print("  интерфейс (2.3):   {:>6.2f} нс\n", NanosPerCall(elapsed));
    }

    {
        // Тот же приём, что с указателем и интерфейсом: без него компилятор
        // видит, какой именно Holder создан рядом, и виртуальный вызов
        // исчезает. Замер тогда получается втрое лучше — и неверный.
        Function<std::uint64_t(std::uint64_t)> mine(
            [](std::uint64_t x) { return x * 3 + 1; });
        if (alternative) {
            mine = Function<std::uint64_t(std::uint64_t)>(
                [](std::uint64_t x) { return x * 3 + 1; });
        }
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t total = 0;
        for (int i = 0; i < kCalls; ++i) {
            total += mine(static_cast<std::uint64_t>(i));
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        checksum += total;
        std::print("  свой Function:     {:>6.2f} нс\n", NanosPerCall(elapsed));
    }

    {
        std::function<std::uint64_t(std::uint64_t)> theirs(
            [](std::uint64_t x) { return x * 3 + 1; });
        if (alternative) {
            theirs = [](std::uint64_t x) { return x * 3 + 1; };
        }
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t total = 0;
        for (int i = 0; i < kCalls; ++i) {
            total += theirs(static_cast<std::uint64_t>(i));
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        checksum += total;
        std::print("  std::function:     {:>6.2f} нс\n", NanosPerCall(elapsed));
    }

    // -----------------------------------------------------------------------
    std::print("\n=== Размеры ===\n");
    std::print("  sizeof(Function<uint64_t(uint64_t)>)       {}\n",
               sizeof(Function<std::uint64_t(std::uint64_t)>));
    std::print("  sizeof(std::function<uint64_t(uint64_t)>)  {}\n",
               sizeof(std::function<std::uint64_t(std::uint64_t)>));

    std::print("\n(контрольная сумма {} — печатается, чтобы циклы "
               "не выбросили)\n",
               checksum);
    return 0;
}
