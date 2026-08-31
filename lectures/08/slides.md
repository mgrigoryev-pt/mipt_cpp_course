---
marp: true
theme: gaia
footer: 'Программирование на языке C++. ВШПИ МФТИ 2026'
paginate: true
---
<style>
section {
    font-size: 25px;
}
</style>

# Лекция 8.

## Шаблоны: введение

---

<style scoped>
/* Тринадцать пунктов — самый длинный план в курсе, и при обычном интервале
   последний уезжает под колонтитул. Правка местная, только для этого слайда. */
ol { line-height: 1.15; }
</style>

# План

1. Зачем нужны шаблоны
2. Шаблонные функции
3. Шаблонные классы
4. CTAD — вывод параметров класса (C++17)
5. Алиасы и шаблонные константы
6. Модель компиляции шаблонов
7. Перегрузка шаблонов
8. Специализация шаблонов
9. Шаблонные аргументы по умолчанию
10. Non-type template parameters
11. Зависимые имена
12. Концепты — упоминание (C++20)
13. `constexpr` — упоминание

---
<!-- header: 1. Зачем нужны шаблоны -->

# 1. Зачем нужны шаблоны

Без шаблонов «один и тот же» алгоритм нужно дублировать для каждого типа:

```cpp
int max_int(int a, int b)       { return a > b ? a : b; }
double max_d(double a, double b){ return a > b ? a : b; }
```

С шаблонами — один код для всех типов:

```cpp
template <typename T>
T max_val(T a, T b) { return a > b ? a : b; }

max_val(3, 5);       // T = int
max_val(3.0, 5.0);   // T = double
```

---

## Шаблоны — это compile-time

Шаблоны — это **«инструкции компилятору, как породить код»**, не классы и функции сами по себе.

```cpp
max_val(3, 5);       // компилятор сгенерировал int max_val(int, int)
max_val(3.0, 5.0);   // компилятор сгенерировал double max_val(double, double)
```

Каждый набор шаблонных параметров → отдельная функция в бинарнике. Это называется **инстанцирование**.

Цена — больше времени компиляции и больший бинарник. Выигрыш — нулевые накладные расходы и compile-time проверка типов.

---
<!-- header: 2. Шаблонные функции -->

# 2. Шаблонные функции

Базовый синтаксис:

```cpp
template <typename T>
T add(T a, T b) {
    return a + b;
}

add(2, 3);              // T = int, выведено
add<double>(2, 3);      // T = double, указали явно
add<int>(2.0, 3.0);     // T = int, явно — double обрежется
```

- `typename T` — объявляет параметр-тип. `class T` — тот же смысл, исторически.
- Если все `T` выводятся из аргументов — указывать `<...>` не нужно.

---

## Несколько параметров

```cpp
template <typename T, typename U>
auto pair_sum(T a, U b) {
    return a + b;
}

pair_sum(1, 2.5);    // T = int, U = double, результат double
```

Можно использовать тот же параметр несколько раз:

```cpp
template <typename T>
void swap_(T& a, T& b) {
    T tmp = a; a = b; b = tmp;
}
```

---

## Когда нужно указать тип явно

Если из аргументов тип **не выводится** — нужно явно:

```cpp
template <typename T>
T parse_number(const std::string& s);  // T в возврате, не в аргументах

parse_number<int>("42");      // ok
parse_number("42");           // CE: cannot deduce T
```

Также — если хотите принудительно использовать конкретный тип, отличный от выведенного.

---
<!-- header: 3. Шаблонные классы -->

# 3. Шаблонные классы

```cpp
template <typename T>
class Stack {
public:
    void push(const T& x) { data_.push_back(x); }
    T pop() { T top = data_.back(); data_.pop_back(); return top; }
    size_t size() const { return data_.size(); }
private:
    std::vector<T> data_;
};

Stack<int> s_int;
Stack<std::string> s_str;
```

`Stack<int>` и `Stack<std::string>` — **разные типы**, не имеют общего родителя.

---

## Методы вне класса

Если определяете метод **снаружи** — нужно повторить шаблонную часть:

```cpp
template <typename T>
class Stack {
public:
    void push(const T& x);
};

template <typename T>            // ← здесь шаблонная часть
void Stack<T>::push(const T& x) {
    data_.push_back(x);
}
```

`Stack<T>::push` — имя метода полностью квалифицировано: класс с параметрами + имя.

---
<!-- header: 4. CTAD -->

# 4. CTAD — вывод параметров класса (C++17)

Для **функций** компилятор выводит `T` из аргументов. Для **классов** до C++17 приходилось писать явно:

```cpp
std::vector<int> v{1, 2, 3};            // <int> обязательно
std::pair<int, double> p{1, 2.0};       // <int, double> обязательно
```

С C++17 параметры класса выводятся из **аргументов конструктора**:

```cpp
std::vector v{1, 2, 3};                 // CTAD: vector<int>
std::pair  p{1, 2.0};                   // CTAD: pair<int, double>
std::tuple t{1, 2.0, "x"};              // tuple<int, double, const char*>
```

---

## CTAD для своих классов

Работает автоматически, если параметры видны в конструкторе:

```cpp
template <typename T>
struct Wrapper {
    T value;
};

Wrapper w{42};        // CTAD: Wrapper<int>
```

Компилятор строит **неявные deduction guides** из конструкторов. В большинстве случаев этого достаточно, и ничего писать не нужно.

---

## Deduction guides

Иногда параметры **не выводятся** напрямую — например, конструктор принимает итераторы, а параметр класса это тип элемента. Тогда пишут **явный guide**:

```cpp
template <typename It>
Container(It, It) -> Container<typename std::iterator_traits<It>::value_type>;

std::vector<int> v;
Container c(v.begin(), v.end());        // Container<int>
```

Синтаксис: сигнатура конструктора `->` желаемый тип. Пользовательские guides — инструмент авторов библиотек; в прикладном коде почти не встречаются.

---
<!-- header: 5. Алиасы и константы -->

# 5. Алиасы и шаблонные константы

**Шаблонные алиасы** (since C++11) — удобные сокращения:

```cpp
template <typename T>
using Vec = std::vector<T>;

Vec<int> v;            // эквивалент std::vector<int>
```

**Шаблонные константы** (since C++14):

```cpp
template <typename T>
constexpr T pi = T(3.1415926535897932385L);

double d = pi<double>;
float  f = pi<float>;
```

На них построены `_v` и `_t` из стандартной библиотеки.

---
<!-- header: 6. Модель компиляции -->

# 6. Модель компиляции шаблонов

C++ компилирует шаблоны в **два прохода**:

1. **Первый проход** — при чтении определения. Проверяется синтаксис, имена, не зависящие от шаблонных параметров.
2. **Второй проход** — при инстанцировании, когда компилятор знает `T`. Проверяются зависимые имена.

Поэтому ошибки шаблонов часто всплывают **в месте использования**, а не в месте определения.

---

## Шаблоны живут в заголовках

Чтобы инстанцировать шаблон, компилятор должен видеть **его определение**.

```cpp
// header.h
template <typename T>
void f(T x) { /* тело */ }
```

```cpp
// some.cpp
f(42);    // нужно тело f, иначе нечего инстанцировать
```

Поэтому **определения шаблонов** обычно живут в **заголовочных файлах**, не в `.cpp`. Отсюда же — рост времени компиляции в шаблонном коде.

---
<!-- header: 7. Перегрузка -->

# 7. Перегрузка шаблонов

Шаблонную функцию можно перегружать обычной, шаблонной с другими параметрами, и более специализированной:

```cpp
void f(int x)         { std::print("int\n"); }              // 1
template <typename T>
void f(T x)           { std::print("template T\n"); }        // 2
template <typename T>
void f(T* x)          { std::print("template T*\n"); }       // 3

f(42);          // 1: невыборный кандидат всегда лучше шаблона
f(42.0);        // 2: T = double
int* p = nullptr;
f(p);           // 3: T = int, лучше, чем 2
```

Правило: **не-шаблон лучше шаблона**, более специализированный шаблон лучше менее специализированного.

---
<!-- header: 8. Специализация -->

# 8. Специализация шаблонов

**Полная специализация** — отдельная реализация для конкретных параметров:

```cpp
template <typename T>
struct TypeName { static constexpr const char* name = "?"; };

template <>
struct TypeName<int> { static constexpr const char* name = "int"; };

template <>
struct TypeName<double> { static constexpr const char* name = "double"; };

TypeName<int>::name;    // "int"
TypeName<char>::name;   // "?"
```

Полная специализация работает и для классов, и для функций.

---

## Частичная специализация

**Только для классов!** Часть параметров фиксируется, часть остаётся свободной:

```cpp
template <typename T, typename U>
struct Encoder { /* general */ };

// частичная: U зафиксирован как string, T остаётся свободным
template <typename T>
struct Encoder<T, std::string> { /* T → string */ };

// полная: оба зафиксированы
template <>
struct Encoder<int, std::string> { /* specific */ };
```

```cpp
Encoder<double, std::string>   // частичная (T = double)
Encoder<int,    std::string>   // полная (точнее)
Encoder<int,    int>           // general
```

---

## Как работает выбор

Компилятор выбирает специализацию по **специфичности**: чем уже паттерн, тем выше приоритет.

```cpp
template <typename T> struct IsPointer            { static constexpr bool value = false; };
template <typename T> struct IsPointer<T*>        { static constexpr bool value = true; };
template <typename T> struct IsPointer<const T*>  { static constexpr bool value = true; };
```

`IsPointer<const int*>` matches **третью** специализацию — она самая специфичная. Через такие паттерны строятся `type_traits` и SFINAE — подробный разбор в курс не входит.

---

## Функции — только полная специализация

```cpp
template <typename T>
T abs_val(T x);

template <>
int abs_val(int x);          // полная специализация для T = int

template <typename T>
T* abs_val(T* x);            // это НЕ специализация — это новый шаблон-перегрузка
```

Третья строка **компилируется**, но это **не** частичная специализация (их для функций не существует), это **отдельный шаблон-перегрузка**. Работает по правилам разрешения перегрузки: более специализированный шаблон побеждает менее (см. раздел 6).

На практике для функций вместо «частичной специализации» используют именно перегрузку — она часто и предпочтительнее, потому что играет по правилам overload resolution.

---
<!-- header: 9. Аргументы по умолчанию -->

# 9. Шаблонные аргументы по умолчанию

```cpp
template <typename T = int>
struct Container { T value; };

Container<> c1;            // T = int
Container<double> c2;      // T = double
```

Может зависеть от предыдущих параметров:

```cpp
template <typename T, typename Alloc = std::allocator<T>>
class vector { /* ... */ };
```

Так устроены практически все контейнеры STL.

---
<!-- header: 10. Non-type параметры -->

# 10. Non-type template parameters

В шаблон можно передать не только тип, но и **значение**:

```cpp
template <typename T, size_t N>
class Array {
public:
    T data[N];
    constexpr size_t size() const { return N; }
};

Array<int, 5> a;
```

Так устроен `std::array<T, N>`. `N` — это **значение**, известное на этапе компиляции.

---

## Что можно как non-type параметр

- Целые типы: `int`, `size_t`, `bool`, `char`...
- Указатели и ссылки на функции / глобальные объекты
- `enum` значения
- С C++20 — почти любые «structural types»: пользовательские структуры с `constexpr` полями

```cpp
template <bool LoudErrors>
class Logger { /* ... */ };

Logger<true> verbose;
Logger<false> silent;
```

**Не** допускаются: `float`, `std::string` (до C++20), указатели на временные объекты.

---
<!-- header: 11. Зависимые имена -->

# 11. Зависимые имена

В шаблонном коде часто возникает ситуация: имя зависит от `T`, и компилятор не знает, что это — тип или значение.

```cpp
template <typename T>
void f() {
    T::value_type x;       // CE: что такое T::value_type?
}
```

Компилятор не знает, **тип** ли `T::value_type` или **переменная**. По умолчанию считает переменной.

---

## `typename` disambiguator

Скажите компилятору явно:

```cpp
template <typename T>
void f() {
    typename T::value_type x;     // ok: это тип
}
```

Это нужно везде, где зависимое имя — это тип:

```cpp
template <typename Container>
void g(Container& c) {
    typename Container::iterator it = c.begin();
}
```

Без `typename` — CE. С C++20 он во многих случаях необязателен.

---

## `template` disambiguator

Аналогичная проблема — когда зависимое имя это **шаблонный метод**:

```cpp
template <typename T>
void f(T t) {
    t.template foo<int>();    // ← template нужно, иначе CE
}
```

Без `template` компилятор парсит `t.foo<int>` как «сравнение `t.foo < int`, потом `>`». С `template` — «шаблонный метод `foo<int>`».

Это синтаксические подсказки для парсера.

---
<!-- header: 12. Концепты -->

# 12. Концепты — упоминание (C++20)

Ошибки шаблонов исторически бывают **жуткими**:

```cpp
template <typename T>
T sum(const std::vector<T>& v) {
    T result{};
    for (auto x : v) result += x;
    return result;
}
```

Если у `T` нет `+=` — компилятор выдаст простыню на 100 строк из глубины инстанцирования. Это называется «template error spew».

---

## Концепты решают проблему

C++20 даёт способ выразить **требования к T** напрямую:

```cpp
template <std::integral T>     // T должен быть целочисленным
T add(T a, T b) { return a + b; }
```

Если ограничение не выполнено — компилятор пишет **читаемое** сообщение: «T должен удовлетворять concept `std::integral`».

Подробный разбор — за рамками курса. Сейчас знайте, что эта возможность есть.

---
<!-- header: 13. constexpr — упоминание -->

# 13. `constexpr` — упоминание

Шаблонные функции часто бывают **вычислимыми на этапе компиляции**, если пометить их `constexpr`:

```cpp
template <typename T>
constexpr T square(T x) { return x * x; }

constexpr int x = square(5);    // 25, вычислено компилятором
```

Стандарт постепенно делал всё больше функций `constexpr`. К C++20 значительная часть `<algorithm>`, `<numeric>`, `std::vector` — `constexpr`.

Полный разбор `constexpr` / `consteval` / `constinit` — за рамками курса.

---
<!-- header: Итоги -->

# Итоги лекции

- **Шаблон** — инструкция компилятору, как породить код для каждого набора параметров
- **Инстанцирование** — генерация конкретной функции/класса для конкретных `T`
- **Два прохода компиляции:** ошибки часто всплывают в месте использования
- **Шаблонные функции, классы, алиасы, константы** — все они есть
- **Перегрузка:** не-шаблон лучше шаблона, более специализированный лучше менее
- **Специализация:** полная для функций и классов, частичная — только для классов
- **Non-type parameters** — целые типы и значения
- **`typename`/`template`** в зависимых именах
- **Концепты** (C++20) делают шаблоны выразительнее — подробный разбор в курс не входит

---

# Дальше

Следующая лекция — **variadic templates и правила вывода типов**:

- Variadic templates (пакеты параметров)
- Fold expressions (C++17)
- Применения в реальном коде (`make_unique`, `tuple`, `format`)
- Правила вывода типов в шаблонах
