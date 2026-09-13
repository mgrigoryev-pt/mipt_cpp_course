# Как читать справочник

> Дополнительный материал курса: конспект для самостоятельного чтения. Слайдов к нему нет — он не читается на паре, а учит пользоваться справочником по стандартной библиотеке: что означает каждый раздел статьи, где спрятаны подвохи и как по сообщению компилятора найти в справочнике нарушенное требование. Самый короткий из дополнительных материалов.

## 0. О чём этот материал

Лекции курса называют десятки функций стандартной библиотеки, но ни одна лекция не может рассказать о каждой всё. Остальное — в справочнике. Главный справочник для C++ — **cppreference.com**, и почти каждый, кто пишет на C++, держит его открытым.

Статья справочника читается не так, как учебник. Самое важное в ней — не описание в первой строке, а разделы ниже: сложность, исключения, инвалидация итераторов, требования к типам. Именно их пропускают, и именно из-за них пишут ошибки, которые компилятор не ловит.

К концу чтения вы должны:

- знать, на какой вопрос отвечает каждый раздел статьи о функции;
- читать объявление с пометками версий и именами параметров шаблона;
- различать три вида требований и три последствия их нарушения — от понятной ошибки компиляции до UB;
- по ошибке в шаблонном коде находить свою строку и нарушенное требование.

Формулировки стандарта ниже процитированы по рабочему черновику, опубликованному на **eel.is/c++draft**; разделы стандарта обозначены, как принято, именами в квадратных скобках. Опыты собраны clang 19 со стандартной библиотекой Microsoft.

## 1. Два справочника

**Стандарт C++** — первоисточник. Это документ, по которому пишут компиляторы и стандартные библиотеки: каждая функция описана в нём строго, по одной и той же схеме. Официальный текст платный, но рабочий черновик открыт, и eel.is/c++draft удобно показывает его постранично — у каждого раздела есть имя, например `[vector.modifiers]`, и по нему раздел находится в поиске.

**cppreference** пересказывает стандарт человеческим языком, добавляет примеры, пометки о том, в какой версии что появилось, и списки известных отличий реализаций. Для повседневной работы — он. Когда формулировка cppreference кажется двусмысленной или когда нужен точный ответ «гарантировано ли это», — стандарт.

![Статья справочника о функции стандартной библиотеки, разделы сверху вниз и вопрос, на который отвечает каждый. Defined in header — какой include нужен. Объявления с пометками since C++11 и constexpr since C++20 — какие перегрузки есть, с какого стандарта и до какого. Parameters и Type requirements — что можно передать и что должен уметь тип. Return value — что вернётся и не вставит ли функция что-нибудь сама. Complexity — сколько стоит. Exceptions — что бросает и в каком состоянии останется объект. Iterator invalidation — какие итераторы, указатели и ссылки перестанут работать. Notes, Example, See also — подвохи, как пользоваться, чем заменить. Выделены Complexity, Exceptions и Iterator invalidation — разделы, которые чаще всего пропускают](img/entry-anatomy.svg)

Статья cppreference о функции устроена одинаково: заголовок, в котором она объявлена; блок объявлений; затем разделы, которые повторяют разделы стандарта. Разделы стандарта для каждой функции перечислены в `[structure.specifications]`, и три из них прямо говорят, что будет при нарушении, — в терминах [разрешения перегрузки](../../glossary.md#overload-resolution) (overload resolution, выбора одной из [перегрузок](../../glossary.md#overload) — overloads), некорректной программы ([ill-formed](../../glossary.md#ifndr)) и [UB](../../glossary.md#ub) (undefined behavior):

> **Constraints:** the conditions for the function's participation in overload resolution
>
> **Mandates:** the conditions that, if not met, render the program ill-formed
>
> **Preconditions:** conditions that the function assumes to hold whenever it is called; violation of any preconditions results in undefined behavior

Три вида требований, три последствия — раздел 3.

## 2. Объявление

Блок объявлений — первое, что видно в статье, и в нём больше сведений, чем кажется.

**Перегрузки пронумерованы.** У функции часто несколько объявлений, и в статье они помечены номерами (1), (2), (3)…; дальше текст ссылается на них по номерам: «(2) то же, но с компаратором». У `std::sort` в стандарте четыре объявления в [пространстве имён](../../glossary.md#namespace) (namespace) `std`:

```cpp
template<class RandomAccessIterator>
  constexpr void sort(RandomAccessIterator first, RandomAccessIterator last);
template<class ExecutionPolicy, class RandomAccessIterator>
  void sort(ExecutionPolicy&& exec, RandomAccessIterator first, RandomAccessIterator last);
template<class RandomAccessIterator, class Compare>
  constexpr void sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp);
template<class ExecutionPolicy, class RandomAccessIterator, class Compare>
  void sort(ExecutionPolicy&& exec, RandomAccessIterator first, RandomAccessIterator last,
            Compare comp);
```

Две перегрузки с компаратором, две без, и у каждой вариант с `ExecutionPolicy` — параллельной сортировкой (C++17). Отдельно, в пространстве `std::ranges`, — версии, которые принимают диапазон целиком.

**Пометки версий.** Рядом с объявлениями cppreference пишет, с какой версии стандарта перегрузка есть и до какой: «since C++11», «until C++20», «constexpr since C++20», «deprecated in C++17», «removed in C++20». Курс идёт на C++23, поэтому всё с пометкой «since» до C++23 включительно доступно, а «since C++26» — ещё нет. Правую колонку блока стоит читать всегда: самая частая причина «в справочнике есть, а компилятор не знает» — функция из следующего стандарта.

**Имя параметра шаблона — это требование.** `RandomAccessIterator` в объявлении `std::sort` — не просто имя. Оно говорит: сюда нужен итератор произвольного доступа (random access iterator; [категории итераторов](../../glossary.md#iterator-category) — лекция 13, раздел 3). Компилятор эту надпись прочитать не может — для старых перегрузок это соглашение, записанное словами, — поэтому нарушение обнаружится не при вызове, а где-то внутри библиотеки. Раздел 5 показывает, как это выглядит.

## 3. Требования и что будет при нарушении

Три вида требований из `[structure.specifications]` дают три очень разных исхода.

| Вид требования | Если нарушено | Как выглядит |
|---|---|---|
| Constraints | перегрузка не участвует в выборе | ошибка компиляции с объяснением: `constraints not satisfied` |
| Mandates | программа некорректна | ошибка компиляции |
| Preconditions | UB | **ничего** при сборке; что угодно при запуске |

Первые два безопасны: ошибку покажет компилятор. Третий — самый опасный: предусловие компилятор не проверяет, и программа, которая его нарушает, собирается и часто даже работает.

Предусловия `std::sort` в `[sort]`:

> Preconditions: For the overloads in namespace `std`, `RandomAccessIterator` meets the *Cpp17ValueSwappable* requirements and the type of `*first` meets the *Cpp17MoveConstructible* and *Cpp17MoveAssignable* requirements.

И требование к компаратору, общее для всех алгоритмов сортировки, — [строгий слабый порядок](../../glossary.md#strict-weak-ordering) (strict weak ordering), в `[alg.sorting.general]`:

> For algorithms other than those described in [alg.binary.search], `comp` shall induce a strict weak ordering on the values.

Строгий слабый порядок — это то, что нарушает NaN: как именно сломалась сортировка с NaN в опыте, показано в материале [«Как устроены числа»](../numbers/notes.md), раздел 8. Компаратор вида `a.pid <= b.pid` (с `<=` вместо `<`) нарушает то же требование. Ни то ни другое компилятор не заметит.

**Правило:** раздел с требованиями читают прежде, чем вызывать функцию с собственным типом или собственным сравнением. Всё, что там сказано словами, а не проверено компилятором, — ваша ответственность.

## 4. Сложность, возвращаемое значение, исключения, инвалидация

Четыре раздела, ради которых стоит дочитывать статью до конца.

### Сложность

Сложность `std::sort` в стандарте:

> Complexity: Let N be `last - first`. O(N log N) comparisons and projections.

Две вещи в этой строке важнее самой формулы. Первая — **что считается**: сравнения, а не время. Сравнение двух `int` и сравнение двух длинных строк стоят по-разному, а время обхода зависит ещё и от того, как данные лежат в памяти (материал [«Кэш процессора»](../cpu-cache/notes.md)). Вторая — **какая это оценка**: худший случай, средний или **[амортизированный](../../glossary.md#amortized)** (amortized). Добавление в конец `std::vector` — амортизированно постоянное: большинство вызовов дешёвые, а изредка один дорогой, с выделением нового буфера и переносом элементов. В опыте из материала [«Память процесса изнутри»](../process-memory/notes.md), раздел 4, сто вызовов `push_back` дали тринадцать выделений памяти.

### Возвращаемое значение и действие

`operator[]` у `std::map` — пример функции, имя которой обманывает. Стандарт в `[map.access]` описывает её одной строкой:

> Effects: Equivalent to: `return try_emplace(x).first->second;`

`try_emplace` — **вставка**. Если ключа нет, `operator[]` создаёт элемент со значением по умолчанию и возвращает ссылку на него. Проверяется в три строки:

```cpp
std::map<std::string, int> counts{{"cmd.exe", 3}};
int unknown = counts["powershell.exe"];  // «только прочитать»
```

```
size до: 1
counts["powershell.exe"] = 0, size после: 2
```

Чтение добавило элемент. В агенте, который так «проверяет» счётчики по имени процесса, словарь растёт на каждый новый процесс — ровно та неограниченная структура, от которой предостерегает лекция 13, раздел 10. Отсюда же ошибка, которую поймаешь сразу: у константного словаря `operator[]` нет вовсе, потому что вставлять в константный объект нельзя:

```
error: no viable overloaded operator[] for type 'const std::map<std::string, int>'
note: candidate function not viable: 'this' argument has type 'const std::map<std::string, int>',
      but method is not marked const
```

Для чтения — `at`, `find` или `contains` (C++20). У `at` в стандарте:

> Throws: An exception object of type `out_of_range` if no such element is present.
>
> Complexity: Logarithmic.

```
at: std::out_of_range: invalid map<K, T> key
```

### Исключения

Раздел про исключения отвечает не только на «что бросает», но и на «что станет с объектом». Для вставки в конец `std::vector` стандарт в `[vector.modifiers]` говорит:

> If an exception is thrown while inserting a single element at the end and `T` is *Cpp17CopyInsertable* or `is_nothrow_move_constructible_v<T>` is true, there are no effects.

«No effects» — это строгая гарантия из лекции 14, раздел 9: исключение при `push_back` оставит вектор таким, каким он был. И условие при ней — ровно то, что объясняет выбор между копированием и перемещением при росте вектора (лекция 11, раздел 4; материал [«Как устроены исключения»](../exceptions-inside/notes.md), раздел 4).

### Инвалидация

Там же, в `[vector.modifiers]`, — правило [инвалидации итераторов](../../glossary.md#iterator-invalidation) (iterator invalidation):

> Causes reallocation if the new size is greater than the old capacity. Reallocation invalidates all the references, pointers, and iterators referring to the elements in the sequence, as well as the past-the-end iterator.

Одно предложение — и целый класс ошибок. Ссылка на элемент, взятая до `push_back`, после вставки сверх ёмкости указывает в освобождённую память:

```cpp
std::vector<int> pids{101, 102, 103};
int& first = pids[0];
pids.push_back(104);  // ёмкость кончилась — новый буфер
std::print("first = {}\n", first);  // ссылка в старый буфер
```

Под [ASan](../../glossary.md#asan) (AddressSanitizer) — [санитайзером](../../glossary.md#sanitizer) (sanitizer) ошибок памяти, материал [«Санитайзеры и отладчик»](../sanitizers-debugger/notes.md):

```
ERROR: AddressSanitizer: heap-use-after-free
READ of size 4
    #6 in main invalidate.cpp:10

freed by thread T0 here:
    #4 in std::vector<int>::_Emplace_reallocate<int>
    #6 in std::vector<int>::push_back(int &&)
    #7 in main invalidate.cpp:8
```

Отчёт прямо называет виновника: память освободил `push_back` в строке 8. Без санитайзера программа, скорее всего, напечатала бы старое значение и выглядела бы работающей. У каждого контейнера правила инвалидации свои, и cppreference собирает их в одну таблицу на странице обзора контейнеров; лекция 13, раздел 10, пересказывает их для удаления.

## 5. Ошибка в шаблонном коде

Второе место, где нужен справочник, — непонятная ошибка компиляции. Классический пример — `std::sort` для списка:

```cpp
std::list<int> pids{3, 1, 2};
std::sort(pids.begin(), pids.end());
```

![Как читать ошибку в шаблонном коде на примере std::sort для std::list. Сообщение идёт от места поломки внутри библиотеки к месту вызова. Первая строка — error в заголовке algorithm, строка 8404: для итераторов списка нет вычитания _ULast - _UFirst; это шаг 2 — что сломалось. Затем note — промежуточный кадр внутри библиотеки, std::sort с std::less. Затем note из cr/sort_list.cpp, строка 6 — своя строка, шаг 1. Дальше кандидаты, которые не подошли, — их обычно можно не читать. Шаг 3 — вопрос к справочнику: какое требование нарушено? У std::sort в объявлении RandomAccessIterator, а итератор std::list двунаправленный; у списка свой pids.sort()](img/template-error.svg)

Настоящее сообщение clang 19 (пути сокращены):

```
…\include\algorithm:8404:50: error: invalid operands to binary expression
    ('const _List_unchecked_iterator<…>' and 'const _List_unchecked_iterator<…>')
 8404 |     _STD _Sort_unchecked(_UFirst, _ULast, _ULast - _UFirst, _STD _Pass_fn(_Pred));
…\include\algorithm:8409:10: note: in instantiation of function template specialization
    'std::sort<std::_List_iterator<…>, std::less<void>>' requested here
sort_list.cpp:6:10: note: in instantiation of function template specialization
    'std::sort<std::_List_iterator<…>>' requested here
…\include\xutility:1977:30: note: candidate template ignored: could not match 'reverse_iterator'
    against 'std::_List_unchecked_iterator'
```

Ошибка случилась **внутри** библиотеки — там, где `std::sort` вычитает итераторы, чтобы узнать длину диапазона. Компилятор сообщает её в том порядке, в каком разворачивал шаблоны: сначала место поломки, потом по цепочке `in instantiation of … requested here` — кто это вызвал, и так до вашей строки. Порядок чтения обратный:

1. **Найти свою строку.** Первая строка с путём к своему файлу: `sort_list.cpp:6`. Всё выше неё — внутренности библиотеки, которых вы не писали.
2. **Прочитать первую строку `error`.** Что именно не получилось: вычесть два итератора списка.
3. **Спросить справочник, какое требование нарушено.** В объявлении `std::sort` — `RandomAccessIterator`; вычитание итераторов умеют только итераторы произвольного доступа, а у списка итератор двунаправленный. Ответ — в статье о самом `std::list`: среди его функций-членов есть своя сортировка, `pids.sort()`, которая переставляет не значения, а связи узлов.

Строки `candidate template ignored` ниже — перечень перегрузок оператора `-`, которые компилятор примерил и отверг. Их обычно можно не читать.

Версии алгоритмов в `std::ranges` (C++20) записывают свои требования так, что компилятор проверяет их **при вызове**. Та же ошибка с `std::ranges::sort(pids)`:

```
ranges_list.cpp:6:5: error: no matching function for call to object of type 'const _Sort_fn'
note: candidate template ignored: constraints not satisfied [with _Rng = std::list<int> &, …]
note: because 'std::list<int> &' does not satisfy 'random_access_range'
note: because 'iterator_t<list<int, allocator<int>> &>' … does not satisfy 'random_access_iterator'
```

Здесь ошибка сразу в вашей строке и сразу называет требование: `random_access_range`. Это те самые Constraints из раздела 3. Как такие требования записываются в коде — [концепты](../../glossary.md#concept) (concepts) — в курс не входит, но читать сообщения о них можно уже сейчас: слово после `does not satisfy` — это то, что нужно искать в справочнике.

## 6. Порядок работы

1. **Объявление и версия.** Та ли перегрузка, есть ли она в C++23, какой нужен `#include`.
2. **Требования.** Что должны уметь ваши типы и ваше сравнение; помнить, что нарушенное предусловие не даёт ошибки компиляции.
3. **Действие и возвращаемое значение.** Что функция делает на самом деле — например, не вставляет ли.
4. **Сложность.** Что именно считается и какая оценка: худшая, средняя, амортизированная.
5. **Исключения.** Что бросается и что остаётся от объекта.
6. **Инвалидация.** Какие итераторы, указатели и ссылки перестают работать.
7. **Примечания и пример.** Здесь обычно записаны подвохи, на которых уже кто-то споткнулся.

При ошибке компиляции — в обратную сторону: своя строка, первая строка `error`, нарушенное требование, статья справочника.

## Итоги

- cppreference — рабочий справочник; стандарт (черновик на eel.is/c++draft) — первоисточник, по которому проверяют спорное.
- Статья о функции устроена по разделам стандарта. Complexity, Exceptions и Iterator invalidation (сложность, исключения, инвалидация итераторов) пропускают чаще всего и ошибаются из-за них же.
- Пометки версий в блоке объявлений отвечают на «есть ли это в C++23»; имя параметра шаблона вроде `RandomAccessIterator` — требование к аргументу.
- Требования трёх видов: нарушенные Constraints и Mandates дают ошибку компиляции, нарушенные Preconditions — UB без предупреждения.
- `map::operator[]` вставляет; для чтения — `at`, `find`, `contains`. `push_back` сверх ёмкости инвалидирует все ссылки на элементы.
- Ошибка в шаблонном коде читается снизу вверх по цепочке «requested here»: своя строка, первая строка `error`, нарушенное требование.

## Что дальше

Материал опирается на **лекции 8 и 9** (шаблоны и [вывод типов](../../glossary.md#deduction) (type deduction) — отсюда цепочки [инстанцирования](../../glossary.md#instantiation) (instantiation) в ошибках) и **лекцию 13**, разделы 3, 5 и 6 (категории итераторов, последовательные и ассоциативные контейнеры), и связан с материалами, где требования справочника проверялись опытом:

- [«Как устроены числа»](../numbers/notes.md) — строгий слабый порядок и NaN;
- [«Память процесса изнутри»](../process-memory/notes.md) — амортизированная сложность `push_back`;
- [«Как устроены исключения»](../exceptions-inside/notes.md) — строгая гарантия и `noexcept` при росте вектора;
- [«Санитайзеры и отладчик»](../sanitizers-debugger/notes.md) — как поймать нарушенное правило инвалидации.
