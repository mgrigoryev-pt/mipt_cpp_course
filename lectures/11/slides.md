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

# Лекция 11.

## Move-семантика и идеальная передача

---

# План

1. Проблема: лишнее копирование
2. `std::move` как инструмент
3. Value categories (lvalue / rvalue)
4. Move-конструктор
5. Move-`operator=`
6. Правило пяти и автогенерация
7. `emplace_back` — конструирование на месте
8. `std::forward` — отдать дальше тем же, чем получили
9. Когда **не** надо писать `std::move`

---
<!-- header: 1. Проблема -->

# 1. Проблема: лишнее копирование

Классическая реализация `swap`:

```cpp
template <typename T>
void swap(T& a, T& b) {
    T tmp = a;        // копия
    a = b;            // копия
    b = tmp;          // копия
}
```

Для `T = std::vector<int>` размером миллион — **три полных копирования миллиона элементов**.

А ведь мы просто меняем местами! Содержимое никому новое не нужно.

---

## Ещё пример

```cpp
// собрали пачку событий из буфера ядра
std::vector<Event> read_batch() {
    std::vector<Event> batch;
    /* ... заполнили ... */
    return batch;
}

std::vector<std::vector<Event>> pending;
pending.push_back(read_batch());   // копирование всей пачки?!
```

`read_batch()` вернул временный вектор. Он **копируется** в `pending`, потом разрушается. Зачем копировать десять тысяч событий, если оригинал всё равно сейчас умрёт?

Хочется механизм «переложить ресурс» вместо «скопировать его».

---
<!-- header: 2. std::move -->

# 2. `std::move` как инструмент

C++11 ввёл **move-семантику** — способ передать «начинку» объекта без копирования:

```cpp
template <typename T>
void swap(T& a, T& b) {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}
```

Для `std::vector` это **три перестановки трёх указателей** — `O(1)` вместо `O(n)`.

---

## Что делает `std::move`

`std::move` **не двигает ничего сам**. Он просто **разрешает компилятору** относиться к выражению как к «временному, которое сейчас умрёт». То есть как к **rvalue**.

```cpp
std::string s = "hello";
std::string t = std::move(s);   // ресурс уехал в t

// s теперь "valid but unspecified" — обычно пустая
std::cout << s.size();           // ok, легально (обычно 0)
s = "new value";                 // ok, можно присваивать
```

После `std::move(s)` объект `s` **валиден, но в неопределённом состоянии**. Использовать его до переприсваивания не стоит.

---
<!-- header: 3. Value categories -->

# 3. Value categories (lvalue / rvalue)

В C++ выражения делятся на две основные категории:

- **lvalue** — то, у чего есть **имя** и **адрес**. Может стоять слева от `=`.
- **rvalue** — временное значение. Литерал, результат функции, результат арифметики.

```cpp
int x = 5;             // x — lvalue, 5 — rvalue
int y = x + 1;         // y — lvalue, x+1 — rvalue
foo();                 // вызов функции — rvalue (если не возвращает T&)
```

**Rvalue → потенциально умрёт прямо сейчас → можно переиспользовать.**

---

## Правила

```cpp
int x = 5;
int& r = x;            // ok: lvalue-ссылка на lvalue
int& r2 = 5;           // CE: lvalue-ссылка не привязывается к rvalue
int&& r3 = 5;          // ok: rvalue-ссылка к rvalue
int&& r4 = x;          // CE: rvalue-ссылка не привязывается к lvalue
int&& r5 = std::move(x);  // ok: std::move превращает в rvalue
```

`std::move(x)` ничего не двигает — оно **превращает lvalue в rvalue**, чтобы перегрузка выбрала move-версию.

---
<!-- header: 4. Move-конструктор -->

# 4. Move-конструктор

Сигнатура — конструктор от **rvalue-ссылки** на свой тип:

```cpp
class String {
    char* data_ = nullptr;
    size_t size_ = 0;
public:
    String(const String& other);     // copy ctor (как раньше)

    String(String&& other) noexcept  // move ctor — новый
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }
};
```

Забираем указатель у источника, **зануляем** источник — чтобы его деструктор не удалил нашу память.

---

## Когда вызовется move

```cpp
String a("hello");
String b = a;             // copy ctor (a — lvalue)
String c = std::move(a);  // move ctor (std::move делает a rvalue)
String d = String("hi");  // move ctor (литерал — rvalue)
```

Компилятор выбирает между `String(const String&)` и `String(String&&)` по тому, что **слева** от `=`: lvalue или rvalue.

---

## `noexcept` обязателен (почти)

```cpp
String(String&& other) noexcept { /*...*/ }
```

Без `noexcept` `std::vector` при перевыделении будет **копировать** элементы вместо move. Причина — strong exception safety (разберём на лекции 14 про утилитарные типы).

**Правило:** **всегда** помечайте move-конструктор `noexcept`, если он реально не бросает.

---
<!-- header: 5. Move = -->

# 5. Move-`operator=`

```cpp
class String {
public:
    String& operator=(String&& other) noexcept {
        if (this == &other) return *this;

        delete[] data_;              // освобождаем своё
        data_ = other.data_;          // забираем чужое
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;

        return *this;
    }
};
```

Тоже `noexcept`, тоже зануляет источник.

---

## Через copy-and-swap (короче, но не оптимально)

```cpp
String& operator=(String other) noexcept {
    swap(*this, other);
    return *this;
}
```

Один метод закрывает и copy, и move (компилятор выберет, какой конструктор вызвать для параметра `other`). Цена — лишний swap для случая copy, для move — оптимально.

В критичном коде пишут отдельные `operator=(const String&)` и `operator=(String&&)`. В обычном — copy-and-swap проще.

---
<!-- header: 6. Правило пяти -->

# 6. Правило пяти и автогенерация

«Special member functions» — пять функций, которые компилятор генерирует за вас:

1. Default constructor — `T()`
2. Copy constructor — `T(const T&)`
3. Copy assignment — `T& operator=(const T&)`
4. Move constructor — `T(T&&)`
5. Move assignment — `T& operator=(T&&)`

Плюс деструктор (`~T()`).

---

## Правило пяти

Написали **хотя бы один из пяти** — осознанно решите про **остальные** (часто `= default` / `= delete`).

Особенно: если вы написали **свой деструктор**, компилятор **не сгенерирует** move-функции автоматически. Класс будет копироваться по полям — а вы, возможно, хотели move.

```cpp
class Buffer {
public:
    ~Buffer() { delete[] data_; }
    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;
    // ... copy ctor / operator= руками
};
```

---

## Правило нуля

**Самое важное правило:** если ваш класс **не владеет ресурсом напрямую**, не пишите **ничего из пятёрки**.

Поля вроде `std::vector`, `std::string`, `std::unique_ptr` сами умеют правильно копироваться и муваться. Компилятор сгенерирует за вас правильное поведение.

```cpp
class Profile {
    std::string name_;
    std::vector<int> scores_;
    // вообще ничего не пишем — всё работает само
};
```

Большинство хороших C++-классов следуют именно этому правилу.

---
<!-- header: 7. emplace_back -->

# 7. `emplace_back` — конструирование на месте

```cpp
std::vector<std::string> v;
v.push_back("hello");          // создание временного string + move
v.emplace_back("hello");       // создание string прямо в векторе
```

`emplace_back` принимает аргументы **конструктора** элемента и конструирует его **на месте**, без временного объекта.

```cpp
std::vector<Point> v;
v.emplace_back(3, 4);          // вызов Point(3, 4) прямо в памяти вектора
v.push_back(Point(3, 4));      // создание Point, потом move (хуже)
```

В новом коде предпочитайте `emplace_back`/`emplace` — обычно эффективнее и короче.

---
<!-- header: 8. std::forward -->

# 8. `std::forward` — отдать дальше тем же, чем получили

```cpp
template <typename... Args>
void emplace_back(Args&&... args) {
    new (next_slot) T(args...);   // всегда copy
}
```

`Args&&` в шаблоне — **forwarding reference** (лекция 9), не rvalue-ссылка: для lvalue `T = int&`, для rvalue `T = int`.

Информация о том, чем был аргумент, записана в `T`. Теряется она в теле: `args` — имя, у имени есть адрес, значит lvalue.

---

## `std::move` здесь неверен

```cpp
new (next_slot) T(std::move(args)...);   // ПЛОХО
```

`std::move` безусловен: он сделает rvalue и из именованного объекта вызывающего.

```cpp
Pattern named("\\Documents\\");
rules.emplace_back(id, named);   // named опустошён
```

Вызывающий не писал `std::move` и отдавать ресурс не собирался.

---

## Условный cast

```cpp
new (next_slot) T(std::forward<Args>(args)...);
```

| | снаружи lvalue | снаружи rvalue |
|---|---|---|
| `std::move(x)` | rvalue — забрали у вызывающего | rvalue |
| `std::forward<T>(x)` | lvalue — копия | rvalue — перемещение |

**Правило:** `std::forward` — условный `std::move`. Перемещает только то, что и снаружи было временным.

---

## Замер: где разница есть

| что и куда | по значению + `move` | `forward` |
|---|---|---|
| цель по значению, rvalue | 0 копий, 2 перем. | то же |
| цель по значению, lvalue | 1 копия, 2 перем. | 1 копия, **1** |
| цель по `const&`, lvalue | **2 копии** | **1 копия** |

**Подвох:** на временном объекте выигрыша нет — copy elision из C++17 убирает материализацию параметра. Разница только на именованных аргументах.

---

## Как это писать

**Правило:** `std::forward<T>` — только там, где `T` **выведен** из параметра `T&&`, и один раз на аргумент.

```cpp
std::forward(args)...       // CE: тип обязателен
sink(std::forward<T>(x));
sink(std::forward<T>(x));   // второй раз — уже опустошённое
```

```cpp
template <typename T> class Holder {
    void set(T&& value);    // НЕ forwarding reference
};
```

Стоит за `make_unique`, `make_shared`, `emplace_back`.

---
<!-- header: 9. RVO -->

# 9. Когда **не** надо писать `std::move`

```cpp
std::string make() {
    std::string result = "hello";
    return result;           // НЕ std::move(result)!
}
```

Современный компилятор делает **NRVO** (named return value optimization): возвращаемый объект **сразу строится** в месте вызывающей стороны. Никаких копий, никаких move.

`return std::move(result)` это **сломает**: вместо NRVO будет настоящий move (хоть и дешёвый, но всё равно лишний).

С C++17 для prvalue это гарантия стандарта (mandatory copy elision).

---

## Когда `std::move` нужен

- Передаём что-то дальше, что не вернётся: `set(std::move(value))`
- Извлекаем из контейнера: `auto x = std::move(v.back()); v.pop_back();`
- Move в `swap`: `auto tmp = std::move(a); a = std::move(b); b = std::move(tmp);`

---

## Когда `std::move` НЕ нужен

- `return local_var;` — компилятор сделает RVO
- `return expression;` — это уже rvalue
- В `noexcept` move-ctor STL и так выберет move (см. `noexcept` выше)

**Привычка:** не пишите `std::move` на `return` локальной переменной. Если сомневаетесь — компилятор подскажет warning'ом (`-Wpessimizing-move`).

---
<!-- header: Итоги -->

# Итоги лекции

- **Move-семантика** даёт **переместить ресурс** вместо копировать. Для `vector` — `O(1)` вместо `O(n)`.
- **`std::move(x)`** превращает lvalue в rvalue: не двигает, а даёт **разрешение** забрать ресурс.
- **lvalue** имеют имя и адрес. **rvalue** — временные, которые сейчас умрут.
- **Move ctor / move=** принимают `T&&`, забирают ресурс, **зануляют** источник. `noexcept` на них обязателен: без него STL копирует.
- **Правило пяти:** написали один — подумайте про остальные. **Правило нуля:** обычно не пишите ничего.
- **`emplace_back`** конструирует на месте — экономит один move/copy.
- **`std::forward<T>(x)`** — условный `std::move`: перемещает только то, что и снаружи было rvalue.
- **На `return` локальной переменной `std::move` НЕ нужен** — компилятор сделает RVO.

---

# Дальше

Следующая — **`auto` и `decltype`: кто пишет типы**:

- `auto` как делегирование: тип очевиден — не пишем
- Цена: `auto` *упрощает* тип, срезая `const` и `&`
- `decltype` — узнать тип, не называя его
- `decltype(auto)` — синтез обоих

В курс не входит:

- Reference qualifiers методов (`void m() &`, `void m() &&`)
- Полная классификация value categories

