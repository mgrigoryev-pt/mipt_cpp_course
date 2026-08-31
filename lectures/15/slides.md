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

# Лекция 15.

## Умные указатели и основы многопоточности

---

# План

1. `unique_ptr` — устройство
2. `shared_ptr` и control block
3. `make_shared` vs `make_unique`
4. `weak_ptr` — циклические ссылки
5. `std::thread` и `std::jthread`
6. `std::mutex` — защита общего состояния
7. `std::condition_variable` — ожидание
8. `std::future` / `std::promise` / `std::async`
9. `thread_local`
10. Что осталось за рамками курса

---

# Одна идея, три ресурса

На лекции 2 мы взяли RAII как лекарство от `new`/`delete`:

> Ресурс владеется объектом. Деструктор освобождает его автоматически — при любом выходе из области видимости, включая исключение.

Тогда речь была только про **память**. В этой лекции видно, насколько идея универсальна: она работает для любого ресурса — памяти, потока, мьютекса.

---
<!-- header: 1. unique_ptr -->

# 1. `unique_ptr` — устройство

На лекции 2 мы пользовались им как «коробочкой». Внутри — ровно то, что мы уже умеем писать:

```cpp
template <typename T>
class UniquePtr {
    T* ptr_ = nullptr;
public:
    explicit UniquePtr(T* p) : ptr_(p) {}
    ~UniquePtr() { delete ptr_; }          // ← вся суть RAII

    T& operator*()  const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get()        const { return ptr_; }
};
```

Обычный шаблонный класс с `operator*` и `operator->` — те самые «роли притворщика» из лекции 5.

---

## Move-only — без копирования

Копировать нельзя: два владельца → двойной `delete` → UB.

```cpp
UniquePtr(const UniquePtr&) = delete;
UniquePtr& operator=(const UniquePtr&) = delete;
```

«Уникальное владение» = **move-only тип**. Это прямое применение правила пяти из лекции 11.

---

## Move-only — реализация перемещения

```cpp
UniquePtr(UniquePtr&& other) noexcept : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
}

UniquePtr& operator=(UniquePtr&& other) noexcept {
    if (this != &other) {
        delete ptr_;              // освободили своё
        ptr_ = other.ptr_;        // забрали чужое
        other.ptr_ = nullptr;     // занулили источник
    }
    return *this;
}
```

Забираем указатель → зануляем источник → деструктор источника ничего не удалит. Ресурс переехал, владелец по-прежнему один.

---

## Custom deleter — первый намёк на универсальность

Настоящий `unique_ptr` принимает **второй** параметр — чем освобождать:

```cpp
template <typename T, typename Deleter = std::default_delete<T>>
class unique_ptr;
```

И тут идея выходит за рамки памяти:

```cpp
auto close_file = [](FILE* f) { if (f) std::fclose(f); };
std::unique_ptr<FILE, decltype(close_file)> file(std::fopen("x", "r"), close_file);
```

Файл закроется сам. Тот же приём работает для сокетов, GPU-буферов, хендлов ОС. **Уже здесь видно: RAII — не про память, а про владение вообще.**

---
<!-- header: 2. shared_ptr -->

# 2. `shared_ptr` и control block

Иногда владельцев несколько, и заранее неясно, кто уйдёт последним:

```cpp
auto a = std::make_shared<int>(42);
{
    auto b = a;        // refcount = 2
    auto c = a;        // refcount = 3
}                      // b и c ушли → refcount = 1
// объект жив, пока жив хоть один владелец
```

Правило простое: объект умирает, когда счётчик владельцев достигает нуля.

---

## Устройство control block

```
   shared_ptr<T>
   ┌──────────┐
   │  T*    ──┼──────────────────────► T object
   │  ctrl* ──┼──┐                     ┌────────┐
   └──────────┘  │                     │ data   │
                 ▼                     └────────┘
        ┌─────────────────┐
        │ strong refcount │
        │ weak refcount   │
        │ deleter         │
        │ allocator       │
        └─────────────────┘
            Control Block
```

Два указателя вместо одного. Копирование = два присваивания + **атомарный** инкремент.

---

## Цена разделяемого владения

- **Размер:** ~16 байт против 8 у `unique_ptr`
- **Аллокации:** обычно две — объект и control block
- **Атомарный счётчик** — чтобы копирование было потокобезопасным

Отсюда правило: **`shared_ptr` только когда владельцев действительно несколько**. Если хватает уникального — берите `unique_ptr`: он быстрее, меньше и честнее выражает намерение.

Если в вашем коде половина указателей `shared` — скорее всего, дизайн владения не продуман.

---
<!-- header: 3. make_shared -->

# 3. `make_shared` vs `make_unique`

```cpp
auto p1 = std::shared_ptr<T>(new T(args));   // ОК, но не лучший вариант
auto p2 = std::make_shared<T>(args);          // предпочтительный вариант
```

Две причины:

1. **Одна аллокация вместо двух** — объект и control block кладутся рядом. Быстрее, меньше фрагментации, лучше локальность.
2. **Exception safety** — между `new T` и обёртыванием нет «дыры», где сырой указатель мог утечь.

То же для `make_unique<T>(args)` (C++14). **Правило: голый `new` внутри smart-ptr — устаревший стиль.**

---

## Когда `make_shared` подводит

```cpp
auto p = std::make_shared<HugeObject>();
std::weak_ptr<HugeObject> w = p;
p.reset();          // деструктор вызван...
                    // ...но память НЕ освобождена, пока жив weak_ptr
```

Одна аллокация — значит объект и control block освобождаются вместе. Control block жив, пока есть хоть один `weak_ptr` → память «залипает».

Для больших объектов с долгоживущими `weak_ptr` берите `shared_ptr<T>(new T(...))`.

---

## Приватный конструктор

```cpp
class Widget {
    Widget(int x) : x_(x) {}          // private
public:
    static std::shared_ptr<Widget> create(int x) {
        return std::make_shared<Widget>(x);    // CE!
    }
};
```

`make_shared` — **свободная функция**, не метод и не `friend`. Внутри она делает `new Widget(x)`, а доступа к приватному конструктору у неё нет.

Обходные пути: passkey idiom либо `shared_ptr<Widget>(new Widget(x))` — там `new` вызывается изнутри класса.

---
<!-- header: 4. weak_ptr -->

# 4. `weak_ptr` — циклические ссылки

```cpp
struct Node { std::shared_ptr<Node> next; };

auto a = std::make_shared<Node>();
auto b = std::make_shared<Node>();
a->next = b;
b->next = a;        // цикл!
```

Ни один счётчик не дойдёт до нуля — объекты держат друг друга. **Утечка, несмотря на умные указатели.**

Счётчик ссылок — не сборщик мусора: он не умеет находить циклы.

---

## Решение: не владеть

```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node>   prev;   // наблюдает, но не держит
};

a->next = b;
b->prev = a;        // цикла больше нет
```

`weak_ptr` не увеличивает strong-счётчик — он только наблюдает.

---

## Как пользоваться `weak_ptr`

Разыменовать напрямую нельзя — объект мог уже умереть. Нужно попросить временное владение:

```cpp
if (auto sp = w.lock()) {
    sp->method();        // объект жив, пока жив sp
} else {
    // уже уничтожен
}
```

`lock()` **атомарно** проверяет счётчик и выдаёт `shared_ptr`. Атомарность здесь принципиальна: между «проверил, что жив» и «начал пользоваться» другой поток не успеет его убить.

---

## `enable_shared_from_this`

Наивная попытка получить `shared_ptr` на себя ломает всё:

```cpp
std::shared_ptr<Widget> self() {
    return std::shared_ptr<Widget>(this);   // ОПАСНО: второй control block!
}
```

Правильный способ:

```cpp
class Widget : public std::enable_shared_from_this<Widget> {
public:
    std::shared_ptr<Widget> self() { return shared_from_this(); }
};
```

База хранит внутренний `weak_ptr` на существующий control block — `shared_from_this()` берёт **тот же** счётчик. Требование: объект уже должен управляться `shared_ptr`.

---
<!-- header: 5. thread -->

# 5. `std::thread` и `std::jthread`

Многопоточность стандартизирована с C++11; до неё пользовались платформенными API. Поток — такой же ресурс, как память: перед уничтожением его нужно дождаться.

```cpp
#include <thread>

void worker(int id) {
    std::print("worker {}\n", id);
}

int main() {
    std::thread t(worker, 1);
    t.join();          // ждём окончания

    std::thread t2([](int x) { std::print("lambda {}\n", x); }, 42);
    t2.join();
}
```

`std::thread` принимает **callable + аргументы** и запускает их в новом потоке.

---

## `std::thread` — RAII, которого нет

А что если забыть `join()`?

```cpp
{
    std::thread t(worker, 1);
}   // деструктор вызывает std::terminate() — программа падает
```

Деструктор **не делает** то, чего ждёшь от RAII-объекта. Он не может: молча ждать — сюрприз по времени, молча отвязать — сюрприз по данным. Стандарт выбрал «падать громко».

Отвязать поток явно можно через `t.detach()` — тогда он доработает фоном, и `join()` не нужен.

---

## `std::jthread` (C++20) — RAII восстановлен

```cpp
{
    std::jthread t(worker, 1);
}   // деструктор сам делает join()
```

Одна буква `j` (от *joining*) — и тип наконец ведёт себя как RAII-обёртка.

Плюс кооперативная отмена:

```cpp
std::jthread t([](std::stop_token stop) {
    while (!stop.stop_requested()) { do_work(); }
});
t.request_stop();          // вежливо просим остановиться
```

Принудительно убить поток в C++ нельзя — это осознанное решение: иначе останутся захваченные мьютексы и сломанные инварианты.

**Правило: в новом коде `jthread`.**

---
<!-- header: 6. mutex -->

# 6. `std::mutex` — защита общего состояния

```cpp
int counter = 0;
std::mutex m;

void inc() {
    for (int i = 0; i < 100000; ++i) {
        m.lock();
        ++counter;
        m.unlock();
    }
}
```

Без мьютекса `++counter` — это read-modify-write, три операции. Два потока успевают вклиниться друг в друга, инкременты теряются. Формально — **race condition, то есть UB**.

Но ручные `lock`/`unlock` — снова та же ошибка, что с `thread`.

---

## Почему ручной `unlock` плохо

```cpp
m.lock();
do_something();        // а если бросит исключение?
m.unlock();            // эта строка не выполнится
```

Мьютекс останется захваченным навсегда. Любой следующий поток встанет — **deadlock**.

Лечение то же, что для памяти — RAII:

```cpp
{
    std::lock_guard lock(m);   // lock() в конструкторе
    ++counter;
}                               // unlock() в деструкторе, всегда
```

`lock_guard` — самый дешёвый и самый частый лок. С C++17 работает CTAD, тип писать не нужно.

---

## Несколько мьютексов — новый класс ошибок

```cpp
// Поток 1                        // Поток 2
std::lock_guard l1(m1);           std::lock_guard l1(m2);
std::lock_guard l2(m2);           std::lock_guard l2(m1);
```

Первый держит `m1`, ждёт `m2`. Второй держит `m2`, ждёт `m1`. **Deadlock** — оба стоят навсегда.

RAII здесь не спасает: каждый лок по отдельности корректен, проблема в **порядке** захвата.

---

## `std::scoped_lock` (C++17)

```cpp
{
    std::scoped_lock lock(m1, m2);    // оба захвачены атомарно
    /* ... */
}
```

Внутри — deadlock-free алгоритм (упорядочивание или try-lock с откатом). Даже если разные потоки передадут мьютексы в разном порядке, взаимной блокировки не будет.

**Правило:** несколько mutex'ов захватывают только через `scoped_lock`, а не вложенными `lock_guard`.

---

## RAII поверх любого ресурса

| Ресурс | RAII-обёртка | Что делает деструктор |
|---|---|---|
| Память (один владелец) | `unique_ptr` | `delete` |
| Память (много владельцев) | `shared_ptr` | `delete` при refcount = 0 |
| Файл, сокет, хендл | `unique_ptr` + custom deleter | вызывает переданный deleter |
| Мьютекс | `lock_guard`, `scoped_lock` | `unlock()` |
| Поток | `jthread` (C++20) | `request_stop()` + `join()` |

Одна идея — «владение выражается временем жизни объекта» — покрывает все эти случаи.

---
<!-- header: 7. condition_variable -->

# 7. `std::condition_variable` — ожидание

RAII отвечает на вопрос «как гарантированно освободить». Второй вопрос многопоточности — **как дождаться, пока что-то произойдёт** — деструкторами не решается.

Типичная схема агента: один поток читает события из системы, несколько обрабатывают. Потребитель должен спать, пока очередь пуста, и просыпаться, когда данные появятся.

```cpp
std::mutex m;
std::condition_variable cv;
std::queue<Event> q;
```

Собирается из трёх частей: мьютекс защищает очередь, condition_variable будит, предикат проверяет условие.

---

## Producer и consumer

```cpp
// Producer — поток-читатель:
{
    std::lock_guard lock(m);
    q.push(event);
}
cv.notify_one();          // разбудить одного ждущего

// Consumer — рабочий поток:
std::unique_lock lock(m);
cv.wait(lock, [&]{ return !q.empty(); });   // спим, пока пусто
Event e = std::move(q.front()); q.pop();
```

Два нюанса:

- **`unique_lock`, не `lock_guard`** — `wait` отпускает мьютекс на время сна и захватывает обратно
- **Предикат обязателен** — защита от spurious wakeup, когда поток просыпается сам по себе

---
<!-- header: 8. future -->

# 8. `std::future` / `std::promise` / `std::async`

Иногда не нужна вся эта механика — нужно просто «посчитай там, отдай результат сюда»:

```cpp
#include <future>

std::future<int> f = std::async(std::launch::async, [] {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 42;
});

// ... делаем что-то параллельно ...

int result = f.get();      // блокируемся до готовности
```

Исключение из задачи прокинется в `get()`. Флаг `std::launch::async` пишите явно — иначе стандарт разрешает выполнить всё лениво в момент `get()`.

---

## `promise` + `future` — канал на одно значение

```cpp
std::promise<int> p;
std::future<int> f = p.get_future();

std::jthread producer([&p] {
    p.set_value(42);       // выставили из другого потока
});

int x = f.get();           // получили здесь
```

Низкоуровневая форма: один поток обещает значение, другой ждёт. Полезно, когда результат приходит из колбэка или обработчика события. `set_exception` пробросит исключение.

---
<!-- header: 9. thread_local -->

# 9. `thread_local`

Иногда лучший способ избежать гонки — **не делить данные вообще**:

```cpp
thread_local std::mt19937 rng{std::random_device{}()};

int roll() { return rng() % 6 + 1; }   // без мьютекса, у каждого потока свой
```

Каждый поток получает свою копию переменной. Живёт она всё время жизни потока.

Применения: генераторы случайных чисел, локальные буферы логгера, кэши на поток. Никаких блокировок — потому что нечего защищать.

---
<!-- header: 10. что осталось -->

# 10. Что осталось за рамками курса

Базы из этой лекции достаточно, чтобы **писать корректный** concurrent-код:

- Любой доступ к shared mutable state — под mutex'ом.
- Ожидание событий — через condition_variable.
- Асинхронность — через future/async.

Корректный — не значит быстрый: мьютекс упирается в системный вызов. Ниже есть ещё один слой, и он за рамками курса:

- **`std::atomic<T>`** и **`std::memory_order`** — работа без mutex и модель памяти
- **Lock-free** структуры данных, параллельные алгоритмы (`std::execution::par`)
- **Coroutines** (C++20) — обзорно

Здесь разобран рабочий минимум; полный объём темы — отдельный курс.

---
<!-- header: Итоги -->

# Итоги лекции

- `unique_ptr` — move-only, нулевые накладные расходы, custom deleter возможен
- `shared_ptr` — refcount + control block, атомарные операции, дороже
- `make_shared` / `make_unique` — всегда вместо голого `new`
- `weak_ptr` — разрыв циклических ссылок, доступ через `.lock()`
- `enable_shared_from_this` — когда нужен `shared_ptr` на `this`
- `std::jthread` (C++20) вместо `thread` — авто-join
- `std::scoped_lock` для нескольких mutex'ов — без deadlock
- `condition_variable` + `unique_lock` + лямбда-предикат
- `std::async` / `std::future` для асинхронных задач
- `thread_local` — своя копия переменной в каждом потоке

---

# Дальше

Это последняя лекция курса. За его рамками осталось то, что стоит изучить самостоятельно:

- **Метапрограммирование:** SFINAE, `type_traits` глубоко
- **Концепты** (C++20) — полностью
- **Ranges** — глубоко, реализация views
- **Аллокаторы** — кастомные, `allocator_traits`
- **Move-семантика deep:** universal references, perfect forwarding, reference qualifiers
- **Модель памяти C++:** atomics, memory orderings, lock-free
- **`constexpr` deep:** `consteval`, `constinit`, виртуальные в compile-time
- **Бонусы:** alignment / object model, observability
