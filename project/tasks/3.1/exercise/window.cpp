// Стенд занятия 3.1: ваш кольцевой буфер против вашего односвязного списка.
//
// Программа ничего не реализует — она меряет. Обе структуры здесь ваши: список
// написан на занятии 1.2 и повторён ниже, чтобы стенд собирался сам по себе,
// а кольцевой буфер берётся из src/ring_buffer.h, то есть из того, что вы
// написали час назад. Пока этого файла нет, стенд не соберётся.
//
// Порядок работы:
//   1. Написать примитив. Интерфейс задан выданными тестами — восемью случаями
//      в tasks/3.1/tests/template_tests.cpp; прочитать их как спецификацию
//      это часть задания.
//   2. Предсказать на бумаге: во сколько раз кольцевой буфер быстрее
//      на миллионе вставок? И сколько аллокаций сделает каждый?
//   3. Запустить, сверить.
//   4. Объяснить разницу. Двух причин достаточно, и обе называются словами.
//
// Замеров два, вставка и обход, и объяснения у них разные. Числа на вставке
// расходятся между платформами в разы, а время списка на обходе двумодально —
// один и тот же прогон даёт то одно, то вдвое большее, смотря как легли узлы
// в куче. Время буфера не меняется, и это тоже результат.
//
// Сборка — из каталога, где лежит этот файл:
//   g++ -std=c++23 -O2 -Wall -Wextra -I ../../../src -o window window.cpp
//   cl /nologo /std:c++latest /O2 /W4 /EHsc /I ..\..\..\src window.cpp
//
// Ключ -I (у cl это /I) указывает, где искать ring_buffer.h: путь ведёт
// в src/ вашего проекта.
//
// Только Release: в отладочной сборке ничего не встроено, и сравнение
// измеряет работу отладочных проверок, а не структур данных.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <print>
#include <utility>

#include "ring_buffer.h"

using nano_edr::RingBuffer;

// ---------------------------------------------------------------------------
// Счётчик аллокаций
// ---------------------------------------------------------------------------
//
// Подменяется глобальный operator new. Грубо, но честно: «не аллоцирует
// вообще» — проверяемое утверждение, и проверять его надо, а не верить.

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
// Односвязный список с занятия 1.2
// ---------------------------------------------------------------------------

struct Node {
    explicit Node(std::uint64_t v) : value(v) {}
    std::uint64_t value;
    std::unique_ptr<Node> next;
};

class SimpleList {
 public:
    explicit SimpleList(std::size_t capacity) : capacity_(capacity) {}

    ~SimpleList() { Clear(); }

    void PushBack(std::uint64_t value) {
        if (capacity_ != 0 && size_ >= capacity_) {
            PopFront();
        }
        auto node = std::make_unique<Node>(value);
        Node* added = node.get();
        if (head_ == nullptr) {
            head_ = std::move(node);
        } else {
            tail_->next = std::move(node);
        }
        tail_ = added;
        ++size_;
    }

    void PopFront() {
        if (head_ == nullptr) {
            return;
        }
        head_ = std::move(head_->next);
        --size_;
        if (head_ == nullptr) {
            tail_ = nullptr;
        }
    }

    void Clear() {
        while (head_ != nullptr) {
            head_ = std::move(head_->next);
        }
        tail_ = nullptr;
        size_ = 0;
    }

    // Сумма по всем элементам — чтобы измерить не только вставку, но и обход.
    std::uint64_t Sum() const {
        std::uint64_t total = 0;
        for (const Node* it = head_.get(); it != nullptr; it = it->next.get()) {
            total += it->value;
        }
        return total;
    }

    std::size_t size() const { return size_; }

 private:
    std::unique_ptr<Node> head_;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_;
};

// ---------------------------------------------------------------------------
// Сумма по кольцевому буферу
// ---------------------------------------------------------------------------
//
// У списка сумма своим методом — он тут же, в этом файле. У буфера её нет
// и заводить незачем: стенду хватает того, что по буферу можно пройти
// range-based for, а это и есть весь его интерфейс обхода.

template <typename Ring>
std::uint64_t SumOf(const Ring& ring) {
    std::uint64_t total = 0;
    for (const std::uint64_t value : ring) {
        total += value;
    }
    return total;
}

int main() {
    constexpr std::size_t kWindow = 64;
    constexpr int kInsertions = 1000000;

    std::print("=== Вставки: {} штук, окно {} ===\n", kInsertions, kWindow);

    std::uint64_t checksum = 0;

    // --- список ---
    {
        SimpleList list(kWindow);
        allocations = 0;
        counting = true;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kInsertions; ++i) {
            list.PushBack(static_cast<std::uint64_t>(i));
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        counting = false;

        checksum += list.Sum();
        std::print("  список:          {:>8.1f} нс на вставку, аллокаций {}\n",
                   static_cast<double>(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(
                           elapsed).count()) / kInsertions,
                   allocations);
    }

    // --- кольцевой буфер ---
    {
        RingBuffer<std::uint64_t, kWindow> ring;
        allocations = 0;
        counting = true;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kInsertions; ++i) {
            ring.PushBack(static_cast<std::uint64_t>(i));
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        counting = false;

        checksum += SumOf(ring);
        std::print("  кольцевой буфер: {:>8.1f} нс на вставку, аллокаций {}\n",
                   static_cast<double>(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(
                           elapsed).count()) / kInsertions,
                   allocations);
    }

    std::print("\n=== Обход полного окна, {} раз ===\n", kInsertions / 100);

    {
        SimpleList list(kWindow);
        for (std::size_t i = 0; i < kWindow; ++i) {
            list.PushBack(i);
        }
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t total = 0;
        for (int i = 0; i < kInsertions / 100; ++i) {
            total += list.Sum();
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        checksum += total;
        std::print("  список:          {:>8.1f} нс на обход\n",
                   static_cast<double>(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(
                           elapsed).count()) / (kInsertions / 100));
    }

    {
        RingBuffer<std::uint64_t, kWindow> ring;
        for (std::size_t i = 0; i < kWindow; ++i) {
            ring.PushBack(i);
        }
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t total = 0;
        for (int i = 0; i < kInsertions / 100; ++i) {
            total += SumOf(ring);
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        checksum += total;
        std::print("  кольцевой буфер: {:>8.1f} нс на обход\n",
                   static_cast<double>(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(
                           elapsed).count()) / (kInsertions / 100));
    }

    std::print("\n=== Размеры ===\n");
    std::print("  sizeof(Node)                     {}\n", sizeof(Node));
    std::print("  sizeof(SimpleList)               {}\n", sizeof(SimpleList));
    std::print("  sizeof(RingBuffer<uint64_t, 64>) {}\n",
               sizeof(RingBuffer<std::uint64_t, kWindow>));

    std::print("\n(контрольная сумма {} — печатается, чтобы циклы не выбросили)\n",
               checksum);
    return 0;
}
