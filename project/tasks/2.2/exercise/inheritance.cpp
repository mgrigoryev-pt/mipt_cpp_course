// Упражнение занятия 2.2: механика наследования.
//
// Тридцать минут, никакой легенды и никакого отношения к nano-edr. Ромб
// и виртуальное наследование в агенте не нужны, а знать, как они устроены,
// нужно, — поэтому упражнение стоит отдельно и честно объявлено отдельным.
//
// Порядок работы:
//
//   1. Прочитать код и **на бумаге** предсказать четыре вещи:
//        - порядок вывода при создании и разрушении Derived;
//        - sizeof каждого класса;
//        - совпадут ли три указателя в разделе 3;
//        - sizeof ромба до и после virtual.
//   2. Запустить и сверить.
//   3. Объяснить каждое расхождение. Расхождения будут — это и есть смысл.
//
// Сборка:
//   g++ -std=c++23 -Wall -Wextra -o inheritance inheritance.cpp
//   cl /std:c++latest /W4 /EHsc inheritance.cpp

#include <cstddef>
#include <print>

// ---------------------------------------------------------------------------
// 1. Порядок конструирования и разрушения
// ---------------------------------------------------------------------------

struct Member {
    explicit Member(const char* name) : name_(name) {
        std::print("  + поле {}\n", name_);
    }
    ~Member() { std::print("  - поле {}\n", name_); }

    const char* name_;
};

struct First {
    First() { std::print("  + база First\n"); }
    ~First() { std::print("  - база First\n"); }

    int a_ = 1;
};

struct Second {
    Second() { std::print("  + база Second\n"); }
    ~Second() { std::print("  - база Second\n"); }

    char b_ = 'x';
};

// Две базы и три поля. Внимание на порядок объявления: он не совпадает
// с порядком в списке инициализации намеренно.
struct Derived : First, Second {
    Derived() : two_("два"), one_("один"), three_("три") {
        std::print("  + тело Derived\n");
    }
    ~Derived() { std::print("  - тело Derived\n"); }

    Member one_;
    Member two_;
    Member three_;
};

// ---------------------------------------------------------------------------
// 2. Размер и выравнивание
// ---------------------------------------------------------------------------

struct Padded {
    char c_;      // 1 байт
    double d_;    // 8 байт, выравнивание 8
    char e_;      // 1 байт
};

struct Packed {
    double d_;
    char c_;
    char e_;
};

struct Empty {};

struct EmptyDerived : Empty {
    int value_;
};

// ---------------------------------------------------------------------------
// 3. Приведение к базам
// ---------------------------------------------------------------------------
//
// Derived содержит First и Second. Указатель на объект и указатель на его
// вторую базу — одно и то же число? Проверьте предсказание.

// ---------------------------------------------------------------------------
// 4. Ромб
// ---------------------------------------------------------------------------

struct Base {
    int value_ = 0;
};

struct LeftPlain : Base {};
struct RightPlain : Base {};
struct DiamondPlain : LeftPlain, RightPlain {};

struct LeftVirtual : virtual Base {};
struct RightVirtual : virtual Base {};
struct DiamondVirtual : LeftVirtual, RightVirtual {};

int main() {
    std::print("=== 1. Создание и разрушение ===\n");
    {
        Derived d;
        std::print("  (объект живёт)\n");
    }

    std::print("\n=== 2. Размеры ===\n");
    std::print("  sizeof(First)        {}\n", sizeof(First));
    std::print("  sizeof(Second)       {}\n", sizeof(Second));
    std::print("  sizeof(Member)       {}\n", sizeof(Member));
    std::print("  sizeof(Derived)      {}\n", sizeof(Derived));
    std::print("  sizeof(Padded)       {}   alignof {}\n", sizeof(Padded),
               alignof(Padded));
    std::print("  sizeof(Packed)       {}   alignof {}\n", sizeof(Packed),
               alignof(Packed));
    std::print("  sizeof(Empty)        {}\n", sizeof(Empty));
    std::print("  sizeof(EmptyDerived) {}\n", sizeof(EmptyDerived));

    std::print("\n=== 3. Приведение к базам ===\n");
    {
        Derived d;
        First* first = &d;
        Second* second = &d;

        const auto as_number = [](const void* p) {
            return reinterpret_cast<std::size_t>(p);
        };

        std::print("  &d      {}\n", as_number(&d));
        std::print("  First*  {}  сдвиг {}\n", as_number(first),
                   as_number(first) - as_number(&d));
        std::print("  Second* {}  сдвиг {}\n", as_number(second),
                   as_number(second) - as_number(&d));
    }

    std::print("\n=== 4. Ромб ===\n");
    std::print("  sizeof(DiamondPlain)   {}\n", sizeof(DiamondPlain));
    std::print("  sizeof(DiamondVirtual) {}\n", sizeof(DiamondVirtual));
    {
        DiamondVirtual dv;
        // С обычным наследованием эта строка не собралась бы: Base в ромбе
        // два, и обращение к value_ неоднозначно. Раскомментируйте
        // аналогичную строку для DiamondPlain и прочитайте, что скажет
        // компилятор, — сообщение стоит того.
        dv.value_ = 7;
        std::print("  DiamondVirtual::value_ = {}\n", dv.value_);
    }

    return 0;
}
