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

# Лекция 14.

## Утилитарные типы и обработка ошибок

---

# План

1. Зачем нам эти типы
2. `std::string_view`
3. `std::span` (C++20)
4. `std::optional` (C++17)
5. `std::variant` и `std::visit`
6. `std::expected` (C++23)
7. Третий подход и выбор между ними
8. Ошибки на границе с C-API
9. Exception safety guarantees
10. `std::error_code`

---
<!-- header: 1. Зачем нам эти типы -->

# 1. Зачем нам эти типы

К этому моменту мы уже умеем писать классы, шаблоны, лямбды и пользоваться контейнерами — этого хватит, чтобы писать рабочий код. Но в современном production C++ почти всегда встречается несколько утилитарных типов, которые закрывают типичные дыры старого C++:

- «функция может вернуть значение, а может — не вернуть» — `optional`
- «переменная хранит значение одного из нескольких типов» — `variant`
- «передать строку в функцию, не копируя её» — `string_view`
- «передать массив в функцию, не теряя длину» — `span`
- «функция может вернуть либо результат, либо ошибку» — `expected`

---

## Обработка ошибок как отдельная тема

Последняя часть лекции — про **обработку ошибок**. К этому моменту в распоряжении три конкурирующих механизма: коды возврата, исключения и result-типы.

Разберём, чем они отличаются, как переводить чужие коды возврата в `expected` на границе с C-API и какие гарантии стоит давать пользователю своей функции.

---
<!-- header: 2. std::string_view -->

# 2. `std::string_view`

## Почему `const std::string&` не всегда подходит

```cpp
size_t count_dots(const std::string& s) {
    size_t result = 0;
    for (char c : s) if (c == '.') ++result;
    return result;
}

int main() {
    count_dots("hello.world");  // компилируется. Что произошло?
}
```

---

## Что происходит при вызове

При вызове `count_dots("hello.world")`:

1. Из C-строки `"hello.world"` сконструируется **временный `std::string`** (с аллокацией!)
2. Этот `std::string` передастся в функцию по `const&`
3. После возврата временный `std::string` уничтожится

Если функция действительно «просто прочитать строку» — это **лишний `malloc` на каждом вызове**. Хочется передавать любую последовательность символов без копирования.

---

## Почему «лишний malloc» — это много

Разбор одной строки телеметрии: вырезать путь, имя процесса, аргументы командной строки — пять-десять подстрок.

```cpp
Event parse(const std::string& line);   // и внутри ещё substr'ы
```

Каждая подстрока через `std::string::substr` — своя аллокация. Умножаем на поток событий:

```
8 аллокаций × 20 000 событий/с = 160 000 malloc/с
```

И всё это — чтобы **посмотреть** на символы, которые уже лежат в памяти. Ни одна из копий не нужна дольше, чем длится разбор.

---

## Что такое `string_view`

```cpp
#include <string_view>

size_t count_dots(std::string_view s) {
    size_t result = 0;
    for (char c : s) if (c == '.') ++result;
    return result;
}

int main() {
    count_dots("hello.world");           // ok, без аллокации
    std::string s = "hello.world";
    count_dots(s);                       // ok, без аллокации
}
```

`std::string_view` — это **non-owning view** над последовательностью `char`. Внутри это пара `(const char*, size_t)`, ничего не выделяется.

---

## Что важно знать про `string_view`

- Конвертируется из `const char*` и `std::string` неявно
- Имеет почти все методы строки: `size`, `empty`, `find`, `substr`, `==`, итераторы
- `substr` тоже возвращает `string_view` — это **дёшево** (новый view, не новая строка)
- **Не гарантирует null-терминированности!** В функцию C-API передавать нельзя — там ждут `\0` в конце

---

## Главный подвох — время жизни

```cpp
std::string_view bad() {
    std::string s = "hello";
    return s;        // s умрёт здесь — view ссылается на мёртвую память
}

std::string_view also_bad = std::string("hi") + "!";
                            // временный объект умрёт сразу после ;
```

`string_view` — это **ссылка**, не владелец. Применяются те же правила, что и для `const std::string&`:

- Не возвращать из функций без понимания владения
- Не хранить в полях класса, если объект может пережить источник

**Правило: `string_view` — для аргументов функций, которые «только читают» строку.**

---
<!-- header: 3. std::span -->

# 3. `std::span` (C++20)

## Как принять произвольный массив

Как написать функцию, которая принимает массив? До C++20 у нас было три плохих варианта:

```cpp
void print(const int* arr, size_t n);   // легко перепутать длину
void print(const std::vector<int>& v);  // только vector

template <typename Container>
void print(const Container& c);         // принимает что угодно, но шаблонная
```

Хочется: принимать **любой непрерывный массив**, не теряя длину и не быть шаблонным.

---

## Что такое `span`

```cpp
#include <span>

void print(std::span<const int> s) {
    for (int x : s) std::cout << x << ' ';
}
```

`std::span<T>` — это пара `(T*, size_t)`. Принимает массивы, `vector`, `array`, любой контейнер с contiguous памятью.

---

## `span` — единая перегрузка для всех

```cpp
int arr[5] = {1, 2, 3, 4, 5};
std::vector<int> v = {6, 7, 8};
std::array<int, 3> a = {9, 10, 11};

print(arr);   // ok
print(v);     // ok
print(a);     // ok
```

Одна функция принимает любой непрерывный массив — без шаблона и без потери длины.

---

## Static vs dynamic extent

```cpp
std::span<int>           // длина известна в рантайме (по умолчанию)
std::span<int, 5>        // длина фиксирована — ровно 5 элементов
```

С фиксированным размером — компилятор проверяет, что вы передали именно столько элементов, сколько обещали. На практике чаще используется dynamic extent.

**Подвох тот же, что и у `string_view`**: span не владеет данными, надо следить за временем жизни источника.

---
<!-- header: 4. std::optional -->

# 4. `std::optional` (C++17)

## Что вернуть, если результата нет

```cpp
int parse_int(std::string_view s) {
    // что вернуть, если строка — не число?
    return ???;
}
```

До C++17 у нас были плохие варианты:

- Возвращать «магическое значение» (`-1`, `INT_MIN`) — нельзя отличить от валидного результата
- Возвращать через out-параметр + `bool`: `bool parse_int(sv, int& out)` — некрасиво и легко проигнорировать
- Бросать исключение — дорого, и не подходит для **ожидаемого** отсутствия результата

---

## Что такое `optional`

```cpp
#include <optional>
#include <charconv>

std::optional<int> parse_int(std::string_view s) {
    int result;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
    if (ec != std::errc{}) return std::nullopt;
    return result;
}
```

`std::optional<T>` хранит либо значение типа `T`, либо ничего.

---

## Проверка и извлечение значения

```cpp
if (auto x = parse_int("42")) {
    std::cout << *x;             // 42
} else {
    std::cout << "not a number";
}
```

Если в `optional` есть значение — `operator bool` возвращает `true`, и до значения добираемся через `*` или `->`. Если нет — `false`.

---

## Что внутри `optional<T>`

Грубо говоря:

```cpp
template <typename T>
class optional {
    alignas(T) std::byte storage_[sizeof(T)];
    bool has_value_ = false;
    // ...
};
```

То есть — буфер под `T` без аллокации плюс флаг. `T` сам конструируется через **placement new** только когда нужно. Это важно: `optional<int>` стоит ровно `sizeof(int) + sizeof(bool) + padding`, без лишних аллокаций.

---

## Интерфейс `optional`

- `has_value()` / `operator bool` — есть ли значение
- `value()` — кидает `std::bad_optional_access`, если нет
- `*` / `->` — UB, если нет (как у указателей)
- `value_or(default)` — значение или fallback
- `reset()` — сделать пустым
- `emplace(args...)` — сконструировать значение in-place
- `=` от `nullopt` или от `T` — переприсваивание

```cpp
auto x = parse_int("nope");
int n = x.value_or(0);   // 0
```

---

## Монадические операции `optional` (C++23)

```cpp
std::optional<int> parse_int(std::string_view s);
std::optional<int> double_it(int x) { return x * 2; }

auto result = parse_int("21")
        .and_then(double_it)                       // optional<int>
        .transform([](int x) { return x + 1; });   // optional<int> => 43
```

- `and_then(f)` — применить `f`, если есть значение; `f` возвращает `optional`
- `transform(f)` — применить `f` к значению; `f` возвращает обычный тип
- `or_else(f)` — выполнить альтернативу, если значения нет

Это **функциональный стиль** обработки опциональных значений. Без вложенных `if`.

---
<!-- header: 5. std::variant -->

# 5. `std::variant` и `std::visit`

## Чего не хватает `union`

Иногда переменная должна хранить **одно из нескольких** значений разного типа. Классические задачи: узел AST, токен лексера, JSON-значение.

В C для этого был `union`. Но `union`:

- не помнит, какой именно тип сейчас в нём лежит — это надо хранить рядом руками
- не вызывает деструкторов
- не дружит с типами, которые сами имеют конструкторы/деструкторы

В C++17 появился `std::variant` — type-safe sum type.

---

## Базовое использование `variant`

```cpp
#include <variant>

std::variant<int, double, std::string> v;

v = 42;
v = 3.14;
v = std::string("hello");

std::cout << v.index();                                // 2
std::cout << std::holds_alternative<std::string>(v);   // true
std::cout << std::get<std::string>(v);                 // "hello"
std::cout << std::get<2>(v);                           // "hello"
```

---

## Интерфейс `variant`

- `index()` — номер активной альтернативы
- `holds_alternative<T>(v)` — содержит ли T
- `get<T>(v)` / `get<I>(v)` — бросает `std::bad_variant_access`, если не тот тип
- `get_if<T>(&v)` — возвращает указатель или `nullptr`, не бросает
- `emplace<T>(args...)` — сконструировать альтернативу in-place
- `=` от значения совместимого типа

---

## `std::visit`

Вместо цепочки `if (holds_alternative<int>(v)) ... else if ...` есть **visitor**:

```cpp
std::variant<int, double, std::string> v = 3.14;

std::visit([](const auto& x) {
    std::cout << x;
}, v);
```

`visit` вызывает функтор с **правильным типом аргумента**. Компилятор проверяет, что функтор обрабатывает **все** варианты.

---

## Overload trick

Хотим обрабатывать каждую альтернативу **отдельным кодом**. Идиома:

```cpp
template <typename... Ts>
struct overload : Ts... { using Ts::operator()...; };

template <typename... Ts>
overload(Ts...) -> overload<Ts...>;   // deduction guide
```

`overload` наследуется от всех лямбд и через `using Ts::operator()...` собирает их `operator()` в один класс. Получается тип с несколькими перегрузками.

---

## Overload trick — использование

```cpp
std::variant<int, double, std::string> v = std::string("hi");

std::visit(overload{
    [](int x)                { std::cout << "int: " << x; },
    [](double x)             { std::cout << "double: " << x; },
    [](const std::string& x) { std::cout << "string: " << x; }
}, v);
```

`std::visit` выбирает нужную перегрузку по активной альтернативе. **Стандартная идиома**, попадается в любом современном production-коде.

---
<!-- header: 6. std::expected -->

# 6. `std::expected` (C++23)

## Значение или причина ошибки

`optional<T>` решает «значение или ничего». А если хочется «значение **или информация об ошибке**»?

- Можно бросить исключение — иногда дорого, иногда нельзя (HFT, embedded, `-fno-exceptions`)
- Можно вернуть пару `(bool, T)` — некрасиво и легко проигнорировать
- Можно использовать `variant<T, Error>` — почти то, что нужно, но неудобный интерфейс

В C++23 это узаконено как `std::expected<T, E>`.

---

## Возврат `expected` из парсера

```cpp
#include <expected>

enum class ParseError { Empty, NotANumber, Overflow };

std::expected<int, ParseError> parse_int(std::string_view s) {
    if (s.empty()) return std::unexpected(ParseError::Empty);

    int result;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
    if (ec == std::errc::result_out_of_range)
        return std::unexpected(ParseError::Overflow);
    if (ec != std::errc{})
        return std::unexpected(ParseError::NotANumber);

    return result;
}
```

---

## Интерфейс `expected`

- `has_value()` / `operator bool` — успех
- `value()` — значение, бросает `bad_expected_access` при ошибке
- `error()` — ошибка
- `value_or(default)` — значение или fallback
- `*`, `->` — как у `optional` (UB при ошибке)
- Монадические `and_then`, `or_else`, `transform`, `transform_error`

```cpp
auto x = parse_int("nope");
if (!x) {
    std::cerr << "error code: " << static_cast<int>(x.error());
}
```

---

## Что делать до C++23

Если стандарта пока нет:

- **`tl::expected`** (Sy Brand) — популярная header-only реализация
- **`std::variant<T, Error>`** — рабочая альтернатива, но без удобного интерфейса
- **`absl::StatusOr<T>`** — у Google

К C++23 эта идея созрела достаточно, чтобы войти в стандарт. Если ваш проект на C++20 — обычно используют `tl::expected`.

---
<!-- header: 7. Подходы к обработке ошибок -->

# 7. Третий подход и выбор между ними

На лекции 3 мы разобрали два механизма: **код возврата** и **исключения**. Теперь у нас появился третий — **result-типы** (`optional`, `expected`).

Пора выложить их рядом и научиться выбирать. Спойлер: «лучшего» подхода не существует, каждый правильный для своих ситуаций.

---

## Напоминание: первые два

| | Код возврата | Исключение |
|---|---|---|
| Стоимость | ноль | throw дорог, happy path почти бесплатен |
| Можно проигнорировать | да, молча | нет, программа упадёт |
| Ошибка видна в сигнатуре | нет | нет |
| Через 5 уровней стека | тащить руками | сами |
| Работает с `-fno-exceptions` | да | нет |

Главная претензия к обоим одна: **из сигнатуры функции не видно, что она может не сработать**.

---

## Что добавляет result-тип

```cpp
std::expected<Config, ParseError> load_config(std::string_view path);
```

Сигнатура говорит всё: функция вернёт либо конфиг, либо `ParseError`. Забыть проверку нельзя — `*` до проверки это UB, а `[[nodiscard]]` не даст выкинуть результат.

**Плюсы:** ошибка в типе; поток управления явный; нет `-fno-exceptions`-ограничений; хорошо для библиотечных API.

---

## Чем платим

```cpp
auto data = read_file(path);
if (!data) return std::unexpected(data.error());
auto parsed = parse(*data);
if (!parsed) return std::unexpected(parsed.error());
```

- **Код «лесенится»** проверками — частично лечится монадическими `and_then` / `or_else`
- **Плохо масштабируется на «ошибка пятью уровнями ниже»** — вернулись к ручному пробросу, от которого спасали исключения
- Без `[[nodiscard]]` теряют половину своей ценности

---

## Когда что использовать

| Ситуация | Подход |
|---|---|
| Ожидаемая локальная ошибка | `optional` / `expected` |
| Редкая неустранимая «где угодно» | исключения |
| `-fno-exceptions` (HFT, embedded) | коды возврата / `expected` |
| Системный API | `error_code` |
| Парсеры, валидаторы | `expected` |
| Конструкторы | исключения |

---

## Главное правило

**Внутри одного проекта или слоя используйте один подход последовательно.**

Смешивать механизмы — самый плохой вариант: непонятно, ловить ли исключения или проверять возврат, легко пропустить ошибку.

Развёрнутое сравнение — в конспекте (`lecture.md`).

---

## Как «один подход на слой» выглядит вживую

Агент защиты, три слоя — три решения:

| Слой | Подход | Почему |
|---|---|---|
| Обращения к ОС | `error_code` | это и есть интерфейс системы |
| Разбор события (горячий путь) | `expected` | ошибка — норма: битая строка не редкость, а данность |
| Загрузка конфигурации | исключения | старт, один раз, продолжать без конфига бессмысленно |

Ключ — **не** «где быстрее», а «насколько ошибка ожидаема». Битая строка в потоке — рутина. Отсутствующий конфиг — конец истории.

---
<!-- header: 8. Ошибки на границе с C-API -->

# 8. Ошибки на границе с C-API

Типичная C-библиотека возвращает перечислимый статус, а результат кладёт в out-параметр:

```c
typedef enum {
    LIB_OK = 0,
    LIB_ERR_BAD_HANDLE = 1,
    LIB_ERR_NOT_FOUND = 2,
    LIB_ERR_IO = 3
} status_t;

status_t lib_get_value(handle_t* h, int id, value_t* out);
```

Результат и признак успеха разъехались: `out` осмыслен только при `LIB_OK`, а сам статус легко проигнорировать — вызов без проверки компилируется молча.

---

## Тонкая обёртка над C-функцией

```cpp
enum class Error { BadHandle, NotFound, Io, Unknown };

[[nodiscard]] std::expected<Value, Error> get_value(handle_t* h, int id) {
    value_t raw{};
    switch (lib_get_value(h, id, &raw)) {
        case LIB_OK:             return Value{raw.id, raw.payload};
        case LIB_ERR_BAD_HANDLE: return std::unexpected(Error::BadHandle);
        case LIB_ERR_NOT_FOUND:  return std::unexpected(Error::NotFound);
        case LIB_ERR_IO:         return std::unexpected(Error::Io);
    }
    return std::unexpected(Error::Unknown);
}
```

Обёртка не делает ничего, кроме перевода статуса в `Error`. Дальше по коду `status_t` уже не встречается.

---

## Цепочка вызовов через `and_then` / `or_else`

```cpp
std::expected<double, Error> to_score(const Value& v);
std::expected<double, Error> fallback(Error e);

double read_score(handle_t* h, int id) {
    return get_value(h, id)
        .and_then(to_score)     // выполнится только при успехе
        .or_else(fallback)      // выполнится только при ошибке
        .value_or(-1.0);
}
```

Ни одной ручной проверки статуса: `and_then` продолжает счастливый путь, `or_else` разбирает ошибку.

---

## Правила для обёрток над C

- **`[[nodiscard]]` на обёртке** — результат нельзя молча выбросить.
- **Статус не теряется** — каждому коду библиотеки соответствует значение `Error`, на неизвестные есть `Error::Unknown`.
- **Один слой — один механизм** — обёртка полностью скрывает `status_t`, наружу выходит только `expected`.

Так устроена любая обёртка над системной или сторонней C-библиотекой — например над библиотекой, отдающей поток событий с машины.

---
<!-- header: 9. Exception safety guarantees -->

# 9. Exception safety guarantees

Если выбран механизм исключений — нужно понимать **гарантии**, которые даёт функция. Это контракт между функцией и её пользователем.

Дэвид Абрахамс выделил три уровня:

1. **No-throw** (nothrow guarantee)
2. **Strong** (commit-or-rollback)
3. **Basic** (no leaks, valid state)

И есть четвёртый, «минусовой»: **no guarantee** — функция оставляет объект в произвольном состоянии. Так писать плохо.

---

## Basic guarantee

После исключения:

- **Нет утечек ресурсов**
- Объекты остаются в **валидном** (но возможно неопределённом) состоянии — у них можно вызывать деструктор, можно присваивать новое значение

Это **минимальный приемлемый уровень**. Достигается за счёт **RAII**.

```cpp
void f() {
    auto p = std::make_unique<int>(42);   // RAII
    may_throw();
    // если бросило — *p корректно уничтожится
}
```

---

## Strong guarantee

После исключения:

- Никаких утечек
- Состояние **такое же, как было до вызова** функции

Классическая реализация — **copy-and-swap**:

```cpp
T& operator=(const T& other) {
    T tmp(other);    // если бросит — *this не задет
    swap(tmp);       // swap — noexcept
    return *this;
}                    // tmp умирает вместе с временной копией
```

Если на этапе `T tmp(other)` бросится исключение — `*this` остался ровно таким, каким был.

---

## No-throw guarantee

Функция **гарантированно не бросает** исключений. Гарантирует это ключевым словом `noexcept`.

```cpp
void swap(T& a, T& b) noexcept { /*...*/ }
~T() noexcept { /*...*/ }       // деструкторы — noexcept по умолчанию
```

**Зачем это нужно практически:** некоторые операции STL **выбирают между move и copy** в зависимости от того, помечен ли move-конструктор `noexcept`. Если не помечен — STL копирует (потому что не может гарантировать strong guarantee при move).

Это особенно важно для `vector::reserve`.

---

## Пример: `vector::push_back`

`vector::push_back` даёт **strong guarantee**:

1. Если памяти не хватило — `reserve` сначала аллоцирует новую, и только потом начинает перекладывать элементы
2. Если копирование элемента бросило — освобождаем новую память, оставляем старую как была
3. Только когда все элементы скопированы успешно — освобождаем старую память

Это требование стандарта. Поэтому `reserve` использует `move_if_noexcept` (детали — за рамками курса).

---

## Что от вас требуется при написании класса

- **Деструктор** — всегда noexcept (по умолчанию). Бросок из деструктора во время размотки = `std::terminate`.
- **Move-конструктор и move-присваивание** — желательно noexcept. Иначе STL будет копировать вместо мува.
- **swap** — всегда noexcept.
- **Конструкторы** могут бросать — это нормально.

**Правило:** для каждой написанной функции надо уметь ответить, какую из трёх гарантий она даёт.

---
<!-- header: 10. std::error_code -->

# 10. `std::error_code`

Параллельно с исключениями и `expected` в стандартной библиотеке есть **`std::error_code`** — лёгкий объект с **категорией** и **кодом** ошибки.

```cpp
#include <filesystem>

std::error_code ec;
std::filesystem::create_directory("/path", ec);  // не бросает, пишет в ec
if (ec) {
    std::cerr << ec.message();   // человекочитаемое описание
    std::cerr << ec.value();     // числовой код
}
```

Используется там, где исключения не подходят: `std::filesystem`, асинхронные API. По духу — типизированный `errno`. Подробный разбор — на семинаре.

---
<!-- header: Итоги -->

# Итоги лекции

- **`string_view` / `span`** — non-owning views, для аргументов функций без копирования. Следите за временем жизни источника.
- **`optional`** — значение или ничего; для функций «может не вернуть».
- **`variant`** — type-safe union; обходим через `std::visit` + overload trick.
- **`expected`** — значение или ошибка; современный способ возвращать результат из библиотечного API.
- **Три подхода к ошибкам** — коды возврата, исключения, result-типы. Внутри слоя — последовательно один.
- **Граница с C-API** — статус C-библиотеки переводится в `expected` в тонкой `[[nodiscard]]`-обёртке.
- **Три гарантии исключений** — basic / strong / nothrow. Понимайте, какую даёт каждая ваша функция.
- **`error_code`** — есть, используется в системных API.

---

# Дальше

Следующая лекция — **умные указатели и основы многопоточности**:

- `unique_ptr` изнутри, custom deleter
- `shared_ptr` и control block, `make_shared` vs `make_unique`
- `weak_ptr` и циклические ссылки
- `std::thread`, `std::jthread`, `std::mutex`
- `std::condition_variable`, `std::future` / `std::promise` / `std::async`
