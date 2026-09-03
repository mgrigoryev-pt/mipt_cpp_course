// Примитив занятия 2.3: диспетчеризация руками и через virtual.
//
// Сорок минут. Смысл — увидеть, что виртуальный вызов не магия: это ровно та
// таблица указателей на функции, которую здесь собирают своими руками,
// плюс один указатель в объекте.
//
// После этого фраза «виртуальный вызов стоит одну косвенность» перестаёт быть
// заклинанием, потому что косвенность будет видна в собственном коде.
//
// Порядок работы. Программа печатает пять разделов, и дальше в тексте
// «раздел N» означает раздел её вывода:
//
//   1, 2. Прочитать код — сначала ручную таблицу, потом то же самое
//         через virtual. Запустить и убедиться, что обе версии работают
//         одинаково.
//   3.    Размеры. Предсказать на бумаге sizeof объекта с ручной таблицей
//         и sizeof объекта с virtual — совпадут? — и только потом сверить.
//   4.    Невиртуальный деструктор. Предсказать, что напечатает счётчик
//         незакрытых объектов, и объяснить, почему компилятор промолчал.
//   5.    Замер. Предсказать, что быстрее и во сколько раз, а потом
//         объяснить полученное — объяснение здесь важнее числа.
//
// Сборка:
//   g++ -std=c++23 -O2 -Wall -Wextra -o dispatch dispatch.cpp
//   cl /nologo /std:c++latest /O2 /W4 /EHsc dispatch.cpp
//
// Замер имеет смысл только в Release: в отладочной сборке ничего не встроено,
// и обе версии одинаково медленные.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <print>
#include <vector>

// ---------------------------------------------------------------------------
// Диспетчеризация руками (раздел 1 вывода)
// ---------------------------------------------------------------------------
//
// Никакого virtual. Таблица методов — обычная структура с указателями
// на функции; объект хранит указатель на таблицу. Ровно то, что компилятор
// делает сам, когда видит virtual.

struct ManualShape;

struct ManualVtable {
    int (*area)(const ManualShape* self);
    const char* (*name)();
};

struct ManualShape {
    const ManualVtable* vtable;  // тот самый «указатель на таблицу»
    int a;
    int b;
};

int SquareArea(const ManualShape* self) {
    return self->a * self->a;
}
int RectArea(const ManualShape* self) {
    return self->a * self->b;
}
const char* SquareName() {
    return "квадрат";
}
const char* RectName() {
    return "прямоугольник";
}

// По одной таблице на «класс», а не на объект. Это существенно: таблица одна
// для всех квадратов, и в объекте лежит только указатель на неё.
const ManualVtable kSquareVtable = {SquareArea, SquareName};
const ManualVtable kRectVtable = {RectArea, RectName};

ManualShape MakeManualSquare(int side) {
    return ManualShape{&kSquareVtable, side, 0};
}
ManualShape MakeManualRect(int width, int height) {
    return ManualShape{&kRectVtable, width, height};
}

// Вызов «метода»: разыменовать таблицу, взять указатель, позвать. Три шага,
// и все три видны.
int ManualCall(const ManualShape& shape) {
    return shape.vtable->area(&shape);
}

// ---------------------------------------------------------------------------
// То же самое через virtual (раздел 2 вывода)
// ---------------------------------------------------------------------------

class Shape {
 public:
    virtual ~Shape() = default;
    virtual int Area() const = 0;
    virtual const char* Name() const = 0;
};

class Square : public Shape {
 public:
    explicit Square(int side) : side_(side) {}
    int Area() const override { return side_ * side_; }
    const char* Name() const override { return "квадрат"; }

 private:
    int side_;
};

class Rect : public Shape {
 public:
    Rect(int width, int height) : width_(width), height_(height) {}
    int Area() const override { return width_ * height_; }
    const char* Name() const override { return "прямоугольник"; }

 private:
    int width_;
    int height_;
};

// ---------------------------------------------------------------------------
// Что бывает без виртуального деструктора (раздел 4 вывода)
// ---------------------------------------------------------------------------

int leaked_blocks = 0;

class BadBase {
 public:
    ~BadBase() {}  // НЕ virtual — в этом весь смысл примера
};

class BadDerived : public BadBase {
 public:
    BadDerived() { ++leaked_blocks; }
    ~BadDerived() { --leaked_blocks; }
};

class GoodBase {
 public:
    virtual ~GoodBase() = default;
};

class GoodDerived : public GoodBase {
 public:
    GoodDerived() { ++leaked_blocks; }
    ~GoodDerived() override { --leaked_blocks; }
};

int main() {
    std::print("=== 1. Ручная таблица работает ===\n");
    {
        const ManualShape square = MakeManualSquare(4);
        const ManualShape rect = MakeManualRect(3, 5);
        std::print("  {} площадь {}\n", square.vtable->name(), ManualCall(square));
        std::print("  {} площадь {}\n", rect.vtable->name(), ManualCall(rect));
    }

    std::print("\n=== 2. virtual работает так же ===\n");
    {
        const Square square(4);
        const Rect rect(3, 5);
        const Shape* shapes[] = {&square, &rect};
        for (const Shape* shape : shapes) {
            std::print("  {} площадь {}\n", shape->Name(), shape->Area());
        }
    }

    std::print("\n=== 3. Размеры ===\n");
    std::print("  sizeof(ManualVtable) {}\n", sizeof(ManualVtable));
    std::print("  sizeof(ManualShape)  {}   (указатель + два int)\n",
               sizeof(ManualShape));
    std::print("  sizeof(Shape)        {}\n", sizeof(Shape));
    std::print("  sizeof(Square)       {}   (указатель + один int)\n",
               sizeof(Square));
    std::print("  sizeof(Rect)         {}   (указатель + два int)\n",
               sizeof(Rect));
    std::print("  sizeof(int)          {}\n", sizeof(int));

    std::print("\n=== 4. Невиртуальный деструктор ===\n");
    {
        leaked_blocks = 0;
        BadBase* bad = new BadDerived();
        delete bad;  // деструктор BadDerived НЕ вызовется
        std::print("  без virtual: незакрытых {}\n", leaked_blocks);

        leaked_blocks = 0;
        GoodBase* good = new GoodDerived();
        delete good;
        std::print("  с virtual:   незакрытых {}\n", leaked_blocks);
    }
    std::print("  (в настоящем коде на месте счётчика была бы память,\n");
    std::print("   и её утечку показал бы санитайзер)\n");

    std::print("\n=== 5. Замер, миллион вызовов ===\n");
    {
        const int kIterations = 1000000;

        std::vector<ManualShape> manual;
        std::vector<Shape*> virtual_shapes;
        Square square(4);
        Rect rect(3, 5);
        for (int i = 0; i < 100; ++i) {
            manual.push_back(i % 2 == 0 ? MakeManualSquare(4) : MakeManualRect(3, 5));
            virtual_shapes.push_back(i % 2 == 0 ? static_cast<Shape*>(&square)
                                                : static_cast<Shape*>(&rect));
        }

        std::int64_t sum = 0;
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kIterations; ++i) {
            sum += ManualCall(manual[static_cast<std::size_t>(i) % manual.size()]);
        }
        const auto manual_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count();

        start = std::chrono::steady_clock::now();
        for (int i = 0; i < kIterations; ++i) {
            sum += virtual_shapes[static_cast<std::size_t>(i) %
                                  virtual_shapes.size()]->Area();
        }
        const auto virtual_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count();

        std::print("  ручная таблица {} нс на вызов\n",
                   static_cast<double>(manual_ns) / kIterations);
        std::print("  virtual        {} нс на вызов\n",
                   static_cast<double>(virtual_ns) / kIterations);
        std::print("  (sum {} — печатается, чтобы цикл не выбросили)\n", sum);
    }

    return 0;
}
