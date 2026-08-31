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

# Лекция 13.

## Итераторы, контейнеры и алгоритмы

---

# План

1. Идея итератора
2. `range-based for` под капотом
3. Категории итераторов
4. const- и reverse-итераторы
5. Последовательные контейнеры
6. Ассоциативные контейнеры
7. Адаптеры контейнеров
8. Что использовать когда
9. Алгоритмы и лямбды
10. Ограничение памяти в долгоживущем процессе
11. `ranges` — упоминание (C++20)

---
<!-- header: 1. Идея итератора -->

# 1. Идея итератора

**Итератор** — объект, ведущий себя как **обобщённый указатель** в контейнер:

```cpp
std::vector<int> v = {1, 2, 3, 4};

for (auto it = v.begin(); it != v.end(); ++it) {
    std::cout << *it << ' ';        // 1 2 3 4
}
```

- `begin()` — итератор на первый элемент
- `end()` — итератор **за** последним (полуоткрытый интервал)
- `++it` — следующий элемент
- `*it` — текущее значение

Один интерфейс работает для всех контейнеров.

---

## Зачем это нужно

Алгоритмы STL работают с **итераторами**, не с контейнерами:

```cpp
std::sort(v.begin(), v.end());
std::find(v.begin(), v.end(), 3);
std::copy(src.begin(), src.end(), std::back_inserter(dst));
```

Один и тот же `sort` работает для `vector<int>`, `vector<string>`, `deque<MyClass>`, обычного массива. Алгоритм оперирует «итераторами», контейнер ему вообще не важен.

Это и есть **обобщённое программирование** — основа STL.

---
<!-- header: 2. range-based for -->

# 2. `range-based for` под капотом

`for (auto& x : v)` — компилятор разворачивает примерно в:

```cpp
auto _begin = v.begin();
auto _end = v.end();
for (auto _it = _begin; _it != _end; ++_it) {
    auto& x = *_it;
    /* тело */
}
```

Контейнер должен иметь `begin()` и `end()` — это и есть «range».

Для **пользовательских типов**: реализуйте `begin()`/`end()` — как методы или свободные функции — и `range-based for` заработает.

---
<!-- header: 3. Категории -->

# 3. Категории итераторов

Не все итераторы одинаково мощные. Стандарт определяет **иерархию**:

```
Input / Output  →  Forward  →  Bidirectional  →  RandomAccess  →  Contiguous
                                                       ↑                ↑
                                                std::deque         std::vector
```

Чем «правее», тем больше операций поддерживает.

---

## Что умеет каждая категория

| Категория | Что нового | Контейнеры |
|---|---|---|
| Input/Output | `*it`, `++it`, чтение/запись один раз | `istream_iterator` |
| Forward | многократное чтение | `forward_list` |
| Bidirectional | `--it` | `list`, `map`, `set` |
| Random access | `it + n`, `it[n]` | `deque` |
| Contiguous (C++20) | данные **подряд** в памяти | `vector`, `array` |

Алгоритмы STL требуют **минимальную** категорию. `sort` требует RandomAccess, поэтому для `list` есть отдельный `list::sort()`.

---

## `iterator_traits`

Шаблонная утилита, чтобы по итератору достать связанные типы:

```cpp
template <typename It>
using value_type = typename std::iterator_traits<It>::value_type;

template <typename It>
using category = typename std::iterator_traits<It>::iterator_category;
```

Используется в реализации алгоритмов: позволяет шаблону «спросить» у произвольного итератора, какого он типа и какая у него категория. В обычном коде встречается редко.

---
<!-- header: 4. const и reverse -->

# 4. `const`- и reverse-итераторы

```cpp
std::vector<int> v = {1, 2, 3};

auto it1 = v.begin();        // iterator — можно менять *it
auto it2 = v.cbegin();       // const_iterator — только читать

auto it3 = v.rbegin();       // reverse_iterator — идёт с конца
auto it4 = v.crbegin();      // const + reverse
```

`++` на reverse-итераторе **идёт назад** по контейнеру.

```cpp
for (auto it = v.rbegin(); it != v.rend(); ++it) {
    std::cout << *it << ' ';   // 3 2 1
}
```

---
<!-- header: 5. Последовательные -->

# 5. Последовательные контейнеры

| Контейнер | Доступ `[i]` | Вставка в конец | Вставка в середину | Память |
|---|---|---|---|---|
| `vector` | O(1) | O(1)\* | O(n) | подряд |
| `deque` | O(1) | O(1)\* | O(n) | блоками |
| `list` | нет | O(1) | O(1) (по итератору) | связанная |
| `forward_list` | нет | O(1) front | O(1) (по итератору) | связанная, односторонняя |
| `array` | O(1) | нет | нет | подряд, фиксированный |

\* амортизированно (новая аллокация раз в N операций).

**По умолчанию — `vector`.** Это лучший выбор в ~90% случаев.

---

## `std::vector` — рабочая лошадка

```cpp
std::vector<int> v = {1, 2, 3};
v.push_back(4);          // 1 2 3 4
v.emplace_back(5);       // 1 2 3 4 5
v[0] = 10;
v.at(0) = 10;            // проверка границ, бросает out_of_range

v.reserve(100);          // зарезервировать память
v.resize(10);            // изменить размер
v.size();                // 10
v.empty();
```

Итераторы — **contiguous** (массив подряд в памяти).

---

## `std::deque` и `std::list`

```cpp
std::deque<int> d;
d.push_front(1);         // O(1) — главное отличие от vector
d.push_back(2);          // O(1)
d[0];                    // O(1), но медленнее vector[]

std::list<int> l;
l.push_back(1);
l.push_front(2);
// l[0];                 // нет: нет random access
auto it = l.begin();
l.insert(it, 99);        // O(1) — главное преимущество list
```

- `deque` — когда нужно push_front + random access (буферы сообщений)
- `list` — когда нужны частые insert/erase в середине **по итератору**

В обычном коде `list` встречается редко.

---

## `std::array`

```cpp
std::array<int, 5> a = {1, 2, 3, 4, 5};
a[0];                 // O(1)
a.size();             // 5 — известно на компиляции
```

Тонкая обёртка над C-style массивом. Размер — часть типа.

В отличие от `int arr[5]`:

- Не «распадается» до указателя при передаче в функцию
- Имеет интерфейс STL-контейнера (`size`, итераторы, `at`)
- Можно класть в `std::vector<std::array<int, 3>>`

В новом коде вместо `int arr[N]` всегда `std::array<int, N>`.

---
<!-- header: 6. Ассоциативные -->

# 6. Ассоциативные контейнеры

| Контейнер | Поиск | Структура |
|---|---|---|
| `map<K,V>` | O(log n) | red-black tree, **упорядочен** |
| `set<K>` | O(log n) | red-black tree, **упорядочен** |
| `unordered_map<K,V>` | O(1)\* | hash table |
| `unordered_set<K>` | O(1)\* | hash table |

\* амортизированно. Худший случай — O(n) при плохой hash-функции.

**По умолчанию — `unordered_map` / `unordered_set`** (быстрее). `map` / `set` нужны, когда важен **порядок ключей**.

---

## `std::map`

```cpp
std::map<std::string, int> scores;
scores["Anna"] = 90;          // вставка
scores["Bob"] = 85;
scores.insert({"Cara", 75});

if (auto it = scores.find("Anna"); it != scores.end()) {
    std::cout << it->second;  // 90
}

for (const auto& [name, score] : scores) {     // упорядочено по name
    std::cout << name << ": " << score << '\n';
}
```

Итераторы `map` — **bidirectional**, идут по ключам в отсортированном порядке.

---

## Structured bindings (C++17)

Элемент `map` — это `pair<const K, V>`. Обращаться через `.first` / `.second` неудобно:

```cpp
for (const auto& p : scores) {
    std::cout << p.first << ": " << p.second << '\n';   // что такое first?
}
```

C++17 позволяет **распаковать** его на именованные переменные:

```cpp
for (const auto& [name, score] : scores) {
    std::cout << name << ": " << score << '\n';         // читается сразу
}
```

Это и есть **structured bindings** — то, что мы уже видели выше.

---

## Structured bindings — где работают

Распаковывать можно всё «кортежеподобное»:

```cpp
std::pair<int, std::string> p{42, "hello"};
auto [n, s] = p;                       // pair / tuple

struct Point { int x, y; };
auto [x, y] = Point{3, 4};             // агрегат с публичными полями

int arr[3] = {1, 2, 3};
auto [a, b, c] = arr;                  // массив фиксированной длины
```

Модификаторы те же, что у `auto`:

```cpp
auto [k, v]        = p;    // копия
auto& [k, v]       = p;    // ссылки на поля — можно менять
const auto& [k, v] = p;    // const-ссылки
```

---

## Structured bindings — практика

Изменение значений в `map` через ссылку:

```cpp
for (auto& [name, score] : scores) {
    score += 5;            // меняет реальные значения в контейнере
}
```

**Подвох:** в `std::map<K, V>` ключи — `const K` (менять их нельзя, это сломало бы порядок дерева). Поэтому `name` будет `const std::string&` даже без `const` в биндинге.

Второе частое применение — функции, возвращающие пару:

```cpp
auto [it, inserted] = scores.insert({"Dave", 70});
if (!inserted) { /* ключ уже был */ }
```

---

## `unordered_map` и хеш

```cpp
std::unordered_map<std::string, int> scores;
scores["Anna"] = 90;          // O(1)
```

Использует **хеш-функцию** для ключа. Для встроенных типов и `std::string` хеш есть из коробки. Для **своих типов** нужно специализировать `std::hash`:

```cpp
struct Point { int x, y; };

template <>
struct std::hash<Point> {
    size_t operator()(const Point& p) const {
        return std::hash<int>{}(p.x) ^ (std::hash<int>{}(p.y) << 1);
    }
};
```

---

## `std::set` и `std::unordered_set`

То же, что `map`, только **без значения**:

```cpp
std::set<int> s = {3, 1, 4, 1, 5};
s.insert(9);
s.contains(3);       // true (C++20)

if (auto it = s.find(3); it != s.end()) { /*...*/ }
```

`set` хранит уникальные элементы. Есть мульти-версии: `multiset`, `multimap` — допускают дубликаты.

---
<!-- header: 7. Адаптеры -->

# 7. Адаптеры контейнеров

Адаптеры — **обёртки**, ограничивающие интерфейс контейнера до конкретной семантики:

```cpp
std::stack<int> st;             // LIFO
st.push(1); st.push(2);
st.top();                       // 2
st.pop();

std::queue<int> q;              // FIFO
q.push(1); q.push(2);
q.front();                      // 1
q.pop();

std::priority_queue<int> pq;    // max-heap
pq.push(3); pq.push(1); pq.push(4);
pq.top();                       // 4
```

Внутри используют `deque` (stack, queue) или `vector` (priority_queue).

---
<!-- header: 8. Что когда -->

# 8. Что использовать когда

Простой decision tree:

- **Нужен упорядоченный массив?** → `vector` (90% случаев)
- **Размер фиксирован на компиляции?** → `array`
- **push_front + random access?** → `deque`
- **Частые insert/erase по итератору?** → `list` (редко)
- **Поиск по ключу, порядок не важен?** → `unordered_map` / `unordered_set`
- **Поиск + порядок ключей?** → `map` / `set`
- **LIFO / FIFO / heap?** → `stack` / `queue` / `priority_queue`

**Правило большого пальца: всегда начинайте с `vector`.** Менять на что-то другое — только если есть конкретная причина (профиль, сложность операции).

---

## Контейнеров на самом деле гораздо больше

Стандарт продолжает добавлять новые. C++23 ввёл **`std::flat_map`** / **`std::flat_set`** — это сортированный `vector<pair<K,V>>` с `map`-подобным интерфейсом. Для маленьких/средних коллекций он быстрее `std::map` за счёт cache locality.

Кроме стандарта, в реальном коде встречаются **сторонние** контейнеры:

- `absl::flat_hash_map` (Google) — быстрее `unordered_map` в большинстве случаев
- `robin_hood::unordered_map` (martinus/robin-hood-hashing) — компактнее, быстрее
- `tsl::robin_map` — современная робин-худ хеш-таблица

Знать о существовании полезно; выбор решается замером конкретной задачи.

---
<!-- header: 9. Алгоритмы -->

# 9. Алгоритмы и лямбды

STL даёт сотни обобщённых алгоритмов в `<algorithm>` и `<numeric>`:

```cpp
std::vector<int> v = {3, 1, 4, 1, 5, 9, 2, 6};

std::sort(v.begin(), v.end());                          // 1 1 2 3 4 5 6 9
std::sort(v.begin(), v.end(), std::greater<>{});        // по убыванию

auto it = std::find_if(v.begin(), v.end(),
                       [](int x) { return x > 3; });    // первый > 3

auto cnt = std::count_if(v.begin(), v.end(),
                          [](int x) { return x % 2 == 0; });
```

С лямбдами алгоритмы становятся очень выразительными.

---

## Часто используемые алгоритмы

```cpp
std::find(b, e, value)                 // первый равный value
std::find_if(b, e, pred)               // первый, удовлетворяющий pred
std::count(b, e, value)
std::count_if(b, e, pred)
std::transform(b, e, out, f)           // применить f к каждому → out
std::accumulate(b, e, init)            // sum (или с f — fold)
std::copy_if(b, e, out, pred)          // отфильтровать в out
std::any_of / std::all_of / std::none_of
```

**Привычка:** прежде чем писать цикл руками — посмотрите, нет ли уже алгоритма.

---

## На чуть более живых данных

```cpp
struct Event { std::string image; int pid; bool suspicious; };
std::vector<Event> batch = read_batch();

// сколько подозрительных в пачке?
auto n = std::count_if(batch.begin(), batch.end(),
                       [](const Event& e) { return e.suspicious; });

// есть ли событие от процесса вне списка разрешённых?
bool alarm = std::any_of(batch.begin(), batch.end(),
                         [](const Event& e) { return !is_allowed(e.image); });
```

Код читается как условие задачи: «посчитай, у скольких…», «есть ли хоть один…». Рукописный `for` заставляет читателя *восстанавливать* это по телу цикла.

---

## Поиск в отсортированном: `lower_bound`

Если контейнер **отсортирован** — линейный `find` избыточен, есть бинарный поиск:

```cpp
std::vector<int> v = {1, 3, 5, 7, 9};              // отсортирован!

auto it = std::lower_bound(v.begin(), v.end(), 5); // первый >= 5
auto up = std::upper_bound(v.begin(), v.end(), 5); // первый > 5
bool has = std::binary_search(v.begin(), v.end(), 5);
```

`O(log n)` вместо `O(n)`. **Требование:** диапазон должен быть отсортирован — иначе UB, причём молча.

`lower_bound` также используется для вставки с сохранением порядка: `v.insert(lower_bound(...), value)`.

---

## Парадокс: `remove` ничего не удаляет

```cpp
std::vector<int> v = {1, 2, 3, 2, 4};
std::remove(v.begin(), v.end(), 2);
// v == {1, 3, 4, ?, ?}  —  size() всё ещё 5!
```

Алгоритмы работают с **итераторами**, а не с контейнером. Изменить размер они не могут — не знают, что это за контейнер.

`remove` **сдвигает** «выживших» в начало и возвращает итератор на новый логический конец. Хвост остаётся в неопределённом состоянии.

---

## Erase-remove idiom

Поэтому размер нужно поправить **вторым** шагом — через метод контейнера:

```cpp
auto new_end = std::remove(v.begin(), v.end(), 2);
v.erase(new_end, v.end());                 // теперь size() == 3
```

Или в одну строку — классическая **erase-remove idiom**:

```cpp
v.erase(std::remove(v.begin(), v.end(), 2), v.end());
v.erase(std::remove_if(v.begin(), v.end(),
                       [](int x) { return x % 2 == 0; }), v.end());
```

Эта конструкция десятилетиями была в каждом C++-проекте. Её нужно узнавать.

---

## `std::erase` / `std::erase_if` (C++20)

C++20 наконец добавил **свободные функции**, которые делают то, чего все хотели:

```cpp
std::erase(v, 2);                                  // удалить все 2
std::erase_if(v, [](int x) { return x % 2 == 0; }); // удалить чётные
```

Работает для `vector`, `deque`, `list`, `string` и других. Для `map`/`set` есть `std::erase_if`.

**В новом коде используйте `erase_if`.** Erase-remove idiom оставьте для чтения legacy — но понимать, почему она выглядела так странно, полезно: это следствие того, что алгоритмы не знают про контейнеры.

---
<!-- header: 10. Ограничение памяти -->

# 10. Ограничение памяти в долгоживущем процессе

Процесс стартует один раз и работает неделями, а поток данных не кончается.

**Контейнер, в который только добавляют, — это утечка с полезной нагрузкой.** Формально всё корректно и достижимо, фактически процесс растёт до OOM.

**Правило:** удаление старого — часть дизайна структуры, а не оптимизация «на потом».

Так устроен любой долгоживущий агент, разбирающий поток событий с машины.

---

## Две стратегии вытеснения

**По возрасту** — скользящее окно: храним записи не старше `T`.

**По объёму** — жёсткий верхний предел на количество или байты, вытесняем самые старые.

По отдельности каждая дырявая:

- только возраст не спасает от **всплеска** — миллион событий за минуту честно поместится в пятиминутное окно;
- только объём не спасает от **медленного накопления** мусора.

**Правило большого пальца:** ставьте обе границы сразу. Возраст задаёт смысл, объём — потолок.

---

## Почему для окна удобен `deque`

Скользящее окно — это добавление в конец и удаление из начала.

| Контейнер | Добавить в конец | Удалить голову |
|---|---|---|
| `std::deque` | O(1) | O(1) — `pop_front` |
| `std::vector` | O(1) | O(n) — `erase(begin())` сдвигает всё |

У `vector` на бесконечном потоке это квадратичное поведение.

Если верхняя граница известна заранее — подойдёт **кольцевой буфер на `std::array`**: фиксированный размер, аллокаций не нужно вовсе.

---

## Окно событий: две обрезки

```cpp
using Clock = std::chrono::steady_clock;
struct Event { Clock::time_point ts; std::string image; };

void trim_by_age(std::deque<Event>& events, Clock::duration max_age) {
    const auto deadline = Clock::now() - max_age;
    while (!events.empty() && events.front().ts < deadline) {
        events.pop_front();
    }
}

void trim_by_size(std::deque<Event>& events, std::size_t max_count) {
    while (events.size() > max_count) { events.pop_front(); }
}
```

Вызываются периодически — по таймеру или раз в N обработанных событий.

---

## Вытеснение из словаря: `std::erase_if`

В `unordered_map` порядка по времени нет — храним отметку последнего обращения:

```cpp
void evict_idle(std::unordered_map<std::string, Clock::time_point>& seen,
                Clock::duration max_idle) {
    const auto deadline = Clock::now() - max_idle;
    std::erase_if(seen, [deadline](const auto& item) {
        return item.second < deadline;
    });
}
```

**Подвох:** удаление во время ручного обхода — классическая ошибка: `erase(it)` инвалидирует `it`, и `++it` читает мусор. `std::erase_if` (C++20) снимает её.

---

## Чего периодическая чистка не гарантирует

**Инвалидация итераторов.** `deque::pop_front` инвалидирует все итераторы, `vector::erase` — всё от точки удаления. Не держите долгоживущий итератор на контейнер, который чистится в фоне: храните ключ или индекс.

**Чистка без верхней границы гарантии не даёт.** Если поток быстрее чистки, между её запусками контейнер вырастет — и оценка «памяти будет не больше X» перестаёт выполняться.

**Правило:** гарантию даёт только жёсткий потолок, проверяемый **в момент вставки**. Что делать при переполнении — выбросить самое старое, отбросить новое или считать ошибкой — решается явно.

---
<!-- header: 11. ranges -->

# 11. `ranges` — упоминание (C++20)

C++20 добавил **более удобный** способ работать с алгоритмами:

```cpp
#include <ranges>

std::vector<int> v = {3, 1, 4, 1, 5};

std::ranges::sort(v);                          // вместо v.begin(), v.end()

auto evens = v | std::views::filter([](int x) { return x % 2 == 0; });
```

Преимущества: не нужно передавать `begin()`/`end()`, композиция через `|`, lazy evaluation.

Подробный разбор — за рамками курса.

---
<!-- header: Итоги -->

# Итоги лекции

- **Итератор** — обобщённый указатель: `begin()`, `end()`, `++`, `*`, полуоткрытый интервал `[begin, end)`. Категории: Input → Forward → Bidirectional → RandomAccess → Contiguous, алгоритм требует минимальную.
- **`range-based for`** работает через `begin()`/`end()`, и свой тип обязан их предоставить.
- **Последовательные:** `vector` по умолчанию, `deque` для push_front, `array` для фиксированного размера.
- **Ассоциативные:** `unordered_map`/`set` по умолчанию, `map`/`set` для порядка; адаптеры — `stack`, `queue`, `priority_queue`.
- **STL-алгоритмы + лямбды** заменяют большую часть ручных циклов; `std::erase_if` — вместо удаления во время обхода.
- **Долгоживущий процесс:** контейнер, в который только добавляют, — утечка. Границы нужны обе: по возрасту и по объёму.
- **`ranges` (C++20)** — за рамками курса.

---

# Дальше

Следующая лекция — **утилитарные типы и обработка ошибок**:

- `std::string_view`, `std::span` — non-owning views
- `std::optional`, `std::variant`, `std::expected`
- Третий подход к ошибкам и сравнение всех трёх (после лекции 3)
- Ошибки на границе с C-API: коды возврата в `std::expected`
- Exception safety guarantees
- `std::error_code` (упоминание)
