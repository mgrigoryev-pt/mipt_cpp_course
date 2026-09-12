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

# Лекция 9.

## Variadic templates и правила вывода типов

---

# План

1. Variadic templates
2. Раскрытие пакета
3. Классическая рекурсия по пакету
4. Fold expressions (C++17)
5. Применения variadic в реальности
6. Правила вывода типов

---
<!-- header: 1. Variadic templates -->

# 1. Variadic templates

Иногда функция должна принимать **любое число** аргументов разных типов:

```cpp
print("Hello, ", name, " you are ", age, " years old\n");
```

В C для этого был `va_list` — без типобезопасности. C++11 ввёл **variadic templates** — типобезопасный, compile-time, и без накладных расходов.

`std::tuple`, `std::variant`, `std::make_unique`, `std::format` — всё это построено на variadic templates.

---

## Синтаксис: пакет параметров

```cpp
template <typename... Ts>
void print_all(Ts... args);
```

- `typename... Ts` — **пакет параметров шаблона** (parameter pack): ноль или больше типов
- `Ts... args` — **пакет параметров функции**: соответствующие значения
- `args...` — **раскрытие пакета** (pack expansion)

Пакет — это **не один тип/значение**, а **последовательность**. Из него нельзя «достать первый элемент» как из массива.

---

## `sizeof...`

Узнать, сколько параметров в пакете:

```cpp
template <typename... Ts>
void f(Ts... args) {
    std::print("Got {} args\n", sizeof...(args));
}

f();             // 0
f(1, 2.0);       // 2
f(1, 2.0, "x");  // 3
```

`sizeof...(args)` — отдельный оператор (не путать с `sizeof(args)`).

---
<!-- header: 2. Раскрытие пакета -->

# 2. Раскрытие пакета

Пакет раскрывается **в разные контексты**:

```cpp
template <typename... Ts>
void forward_all(Ts... args) {
    other_func(args...);              // в список аргументов
}

template <typename... Ts>
auto make_array(Ts... args) {
    return std::array{args...};       // в initializer list
}

template <typename... Bases>
class Multi : public Bases... {       // в список баз класса
};
```

Точки `...` ставятся **после** того, что нужно «размножить».

---
<!-- header: 3. Рекурсия -->

# 3. Классическая рекурсия по пакету

До C++17 единственный способ обработать каждый элемент пакета — **рекурсия**:

```cpp
void print_all() {}                    // база рекурсии

template <typename T, typename... Rest>
void print_all(T first, Rest... rest) {
    std::cout << first << ' ';
    print_all(rest...);                // рекурсивный вызов с остатком
}

print_all(1, 2.5, "hello");            // 1 2.5 hello
```

Работает, но требует двух функций, и каждый промежуточный вызов — отдельное инстанцирование (instantiation).

---

## `if constexpr` вместо базы рекурсии

```cpp
template <typename T, typename... Rest>
void print_all(T first, Rest... rest) {
    std::cout << first << ' ';
    if constexpr (sizeof...(rest) > 0) {
        print_all(rest...);            // для пустого остатка ветки нет
    }
}
```

Обычный `if` не годится: **обе** ветки обязаны компилироваться, а для последнего элемента пришлось бы инстанцировать `print_all()` без аргументов — CE.

`if constexpr` (C++17) считает условие на компиляции, и **невыбранная ветка не инстанцируется**. База рекурсии не нужна.

**Правило:** `if constexpr` нужен там, где ветки предъявляют разные требования к типу. Вне шаблона бесполезен.

---
<!-- header: 4. Fold expressions -->

# 4. Fold expressions (C++17)

C++17 даёт более выразительный способ — **fold expression**:

```cpp
template <typename... Ts>
auto sum(Ts... args) {
    return (args + ...);
}

sum(1, 2, 3, 4);    // 10
```

`(args + ...)` — **унарный правый fold** по `+`. Компилятор раскрывает его в:

```
arg1 + (arg2 + (arg3 + arg4))
```

---

## Четыре формы fold

| Форма | Раскрытие |
|---|---|
| `(args op ...)` | `arg1 op (arg2 op (... op argN))` — унарный правый |
| `(... op args)` | `((arg1 op arg2) op ...) op argN` — унарный левый |
| `(args op ... op init)` | плюс `init` справа — бинарный правый |
| `(init op ... op args)` | плюс `init` слева — бинарный левый |

Для ассоциативных операций (`+`, `*`, `&&`, `||`) разница «правый/левый» не важна. Для неассоциативных (`-`, `/`) — важна.

**Бинарные формы** нужны для случая, когда пакет может быть **пустым** — `init` задаёт значение по умолчанию.

---

## Полезные примеры

```cpp
template <typename... Ts>
auto sum(Ts... args)         { return (args + ...); }

template <typename... Ts>
bool all_true(Ts... args)    { return (args && ...); }

template <typename... Ts>
bool any_true(Ts... args)    { return (args || ...); }
```

Fold по `,` (запятой) — стандартный способ «выполнить что-то для каждого элемента»:

```cpp
template <typename... Ts>
void print_all(Ts... args) {
    ((std::cout << args << ' '), ...);
}
```

---

## С `init` для пустых пакетов

```cpp
template <typename... Ts>
int sum0(Ts... args) {
    return (0 + ... + args);   // sum0() == 0, sum0(1,2,3) == 6
}
```

Без `init` унарный fold пустого пакета — это **CE** (для большинства операторов). Бинарная с `init = 0` работает корректно при любом размере пакета.

---
<!-- header: 5. Применения -->

# 5. Применения variadic в реальности

Стандартная библиотека:

```cpp
// std::make_unique принимает любые аргументы конструктора T
template <typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args);

// std::tuple хранит любое число значений разных типов
std::tuple<int, double, std::string> t{1, 2.5, "x"};

// std::format / std::print
std::print("{} {} {}\n", a, b, c);
```

Везде, где компилятор должен «принять любое число аргументов» — это variadic под капотом.

---

## Свой `make_unique`

```cpp
template <typename T, typename... Args>
std::unique_ptr<T> my_make_unique(Args... args) {
    return std::unique_ptr<T>(new T(args...));
}

auto p = my_make_unique<Point>(3, 4);   // Point(3, 4)
```

`args...` раскрывается в список аргументов для конструктора `T`. Один шаблон обслуживает любые типы и любое число аргументов.

В настоящем `std::make_unique` используется `Args&&...` и `std::forward` — идеальная передача (perfect forwarding). Разбирается на лекции 11, раздел 8.

---
<!-- header: 6. Вывод типов -->

# 6. Правила вывода типов

Когда вы пишете `add(2, 3)`, компилятор **выводит** `T = int` (template argument deduction). У вывода есть несколько правил.

```cpp
template <typename T>
void f(T x);                  // by value

int i = 5;
const int& r = i;
f(i);     // T = int
f(r);     // T = int (отбрасывает const и &)
f(5);     // T = int
```

Для `T` по значению компилятор **отбрасывает** `const`/`volatile` и **ссылки**. Это называется **распад** (decay).

---

## Передача по `T&`

```cpp
template <typename T>
void g(T& x);

int i = 5;
const int ci = 10;
g(i);     // T = int, x — int&
g(ci);    // T = const int, x — const int&
g(5);     // CE: rvalue не привязывается к T&
```

Для `T&` const **сохраняется** (становится частью `T`). Ссылка уходит из `T`: она уже есть в сигнатуре (signature).

---

## Forwarding reference (`T&&`)

```cpp
template <typename T>
void h(T&& x);

int i = 5;
h(i);     // T = int& (lvalue → reference collapsing)
h(5);     // T = int  (rvalue)
```

`T&&` в шаблонной функции — это **не** rvalue-ссылка, а **forwarding reference** (у Мейерса — universal reference):

- передан lvalue → `T = int&`, `T&&` = `int&&&` = `int&`
- передан rvalue → `T = int`, `T&&` = `int&&`

Это позволяет одной функции принимать и lvalue, и rvalue, и передавать дальше через `std::forward`. Приём называется идеальной передачей и разбирается на лекции 11.

---

## Правило выживания

- Хотите, чтобы const сохранился? → `T& x` или `const T& x`
- Хотите менять? → `T& x`
- Принять что угодно и передать дальше? → `T&& x` и `std::forward` (лекция 11)
- Хотите копию? → `T x` (но дорого для больших объектов)

---
<!-- header: Итоги -->

# Итоги лекции

- **Variadic templates** — `template <typename... Ts>`, пакеты, раскрытие `args...`
- **`sizeof...(args)`** — размер пакета (compile-time)
- **`if constexpr` (C++17)** — невыбранная ветка не инстанцируется; рекурсия без второй функции
- **Fold expressions (C++17)** — четыре формы, бинарные для пустых пакетов
- **Fold по `,`** — стандартная идиома «сделай что-то для каждого»
- **Применения:** `make_unique`, `tuple`, `format`, ваши собственные factory-функции
- **Вывод типов:** для `T` отбрасываются `const`/`&`; для `T&` const сохраняется; `T&&` это forwarding reference (что с ней делать внутри — лекция 11)

---

# Дальше

Следующая лекция — **лямбды + `std::function`**:

- Лямбды как шаблонные функторы (function objects), которые пишет компилятор
- Списки захвата
- Захват `this` и полей класса
- Захват с инициализацией (init-capture)
- `std::function` — универсальный контейнер для callable
