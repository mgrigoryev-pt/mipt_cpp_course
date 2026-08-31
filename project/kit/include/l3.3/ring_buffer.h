// Кольцевой буфер. Занятие 3.1.
//
// Первая замена своего примитива своим же, более подходящим: на этом месте
// с занятия 1.2 стоял односвязный список. Список был правильным выбором тогда —
// на нём разбиралось владение, — и перестал быть правильным сейчас.
//
// Что даёт кольцевой буфер и чего не давал список:
//
//   не аллоцирует вообще        память в объекте, `std::array` внутри;
//   элементы лежат рядом        один проход по буферу — один проход по памяти,
//                               а не прыжки по узлам, разбросанным по куче;
//   вытеснение бесплатно        новый элемент пишется поверх самого старого,
//                               освобождать нечего.
//
// Чего он не даёт: размер задаётся на этапе компиляции. Это настоящая плата,
// а не мелочь: окно агента больше нельзя настроить произвольным числом
// из командной строки. Ровно тот размен, который нужно уметь называть словами.
//
// N — non-type параметр шаблона (лекция 8): размер попадает в тип, поэтому
// буферы разного размера — разные типы, и перепутать их нельзя.

#ifndef NANO_EDR_KIT_RING_BUFFER_H
#define NANO_EDR_KIT_RING_BUFFER_H

#include <array>
#include <cstddef>
#include <utility>

namespace nano_edr {

template <typename T, std::size_t N>
class RingBuffer {
 public:
    static_assert(N > 0, "кольцевой буфер нулевого размера ничего не хранит");

    // Обход в логическом порядке — от самого старого к самому новому.
    //
    // Итератор минимальный: только то, что нужно range-based for. Полноценные
    // категории итераторов — лекция 13; здесь достаточно ++, * и !=.
    class Iterator {
     public:
        Iterator(const RingBuffer* owner, std::size_t offset)
            : owner_(owner), offset_(offset) {}

        const T& operator*() const { return owner_->At(offset_); }

        Iterator& operator++() {
            ++offset_;
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return offset_ != other.offset_;
        }

     private:
        const RingBuffer* owner_;
        std::size_t offset_;
    };

    // Дописывает элемент. Если буфер полон, самый старый затирается —
    // окно, а не архив. Возвращать что-либо незачем: вытеснение здесь
    // не событие, а способ работы.
    void PushBack(const T& value) {
        storage_[(begin_ + size_) % N] = value;
        Advance();
    }

    // ЗАНЯТИЕ 3.3. Вторая перегрузка: элемент не копируется, а перемещается.
    //
    // Ради одной строки различия — `std::move(value)` вместо `value` —
    // приходится писать всю функцию заново. Так и есть, и это не изъян
    // кольцевого буфера: ровно так же устроен любой контейнер стандартной
    // библиотеки, у vector те же две перегрузки push_back. Устранить это
    // дублирование можно только идеальной передачей, а её курс
    // не проходит.
    //
    // Обратите внимание: внутри стоит std::move(value), хотя value — уже
    // rvalue-ссылка. Без него не сработает: сама ссылка — lvalue, и
    // присваивание выбрало бы копирующую форму. Это самая частая ошибка
    // в первом коде на move-семантике, и компилятор о ней молчит.
    void PushBack(T&& value) {
        storage_[(begin_ + size_) % N] = std::move(value);
        Advance();
    }

    void Clear() {
        begin_ = 0;
        size_ = 0;
    }

    std::size_t size() const { return size_; }
    static constexpr std::size_t capacity() { return N; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == N; }

    // По логическому индексу: 0 — самый старый.
    const T& operator[](std::size_t index) const { return At(index); }

    // Самый новый элемент. Вызывать на пустом буфере нельзя — это ошибка
    // в коде вызывающего, а не случай, который надо обрабатывать.
    const T& back() const { return At(size_ - 1); }

    Iterator begin() const { return Iterator(this, 0); }
    Iterator end() const { return Iterator(this, size_); }

 private:
    // Сдвиг границ после записи. Вынесено потому, что двух перегрузок
    // PushBack стало две, а логика вытеснения у них одна.
    void Advance() {
        if (size_ < N) {
            ++size_;
        } else {
            // Буфер полон: начало сдвигается, самый старый элемент только что
            // перезаписан. Ни освобождения, ни сдвига содержимого.
            begin_ = (begin_ + 1) % N;
        }
    }

    const T& At(std::size_t offset) const {
        return storage_[(begin_ + offset) % N];
    }

    std::array<T, N> storage_{};
    std::size_t begin_ = 0;
    std::size_t size_ = 0;
};

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_RING_BUFFER_H
