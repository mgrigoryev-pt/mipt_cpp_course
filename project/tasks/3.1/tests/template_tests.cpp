// Тесты кольцевого буфера, порогового правила, комбинаторов и типизированного
// доступа к полям. Занятие 3.1.
//
// Тесты занятий 2.1 и 2.3 остаются подключёнными: событие, разбор, обёртка,
// условия, движок и два вида правил не изменились. Кольцевой буфер заменил
// список в агенте, но сам список остался в коде — на нём построен примитив
// занятия, и его тесты с 2.1 продолжают проходить.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

#include "combinators.h"
#include "conditions.h"
#include "doctest.h"
#include "event.h"
#include "field_traits.h"
#include "make_rule.h"
#include "ring_buffer.h"
#include "rule.h"
#include "rules.h"

using nano_edr::AllOfStatic;
using nano_edr::AnyOfStatic;
using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::EventType;
using nano_edr::EventTypeIs;
using nano_edr::Field;
using nano_edr::FieldEndsWith;
using nano_edr::FieldEquals;
using nano_edr::FieldTraits;
using nano_edr::GetField;
using nano_edr::GetFieldOr;
using nano_edr::MakeCondition;
using nano_edr::MakeRule;
using nano_edr::MatchRule;
using nano_edr::RingBuffer;
using nano_edr::SequenceRule;
using nano_edr::Severity;
using nano_edr::ThresholdRule;
using nano_edr::Timestamp;

namespace {

Event MakeWrite(const std::string& pid, std::uint64_t ts_ms,
                const std::string& path) {
    EventParts parts;
    parts.ts = std::to_string(ts_ms);
    parts.type = "file_write";
    parts.pid = pid;
    parts.fields.push_back(Field{"path", path});
    parts.fields.push_back(Field{"size", "4096"});
    return Event(parts);
}

}  // namespace

// ---------------------------------------------------------------------------
// RingBuffer
// ---------------------------------------------------------------------------

TEST_CASE("пустой буфер") {
    const RingBuffer<int, 4> ring;

    CHECK(ring.empty());
    CHECK(ring.size() == 0);
    CHECK_FALSE(ring.full());
    CHECK(RingBuffer<int, 4>::capacity() == 4);
}

TEST_CASE("размер попадает в тип") {
    // N — non-type параметр, значит буферы разной вместимости это разные типы,
    // и перепутать их нельзя. Проверка компиляторная: capacity() объявлена
    // constexpr, и если она не известна при компиляции, файл не соберётся.
    static_assert(RingBuffer<int, 4>::capacity() == 4);
    CHECK(RingBuffer<int, 4>::capacity() == 4);
    CHECK(RingBuffer<int, 64>::capacity() == 64);
    CHECK_FALSE(std::is_same_v<RingBuffer<int, 4>, RingBuffer<int, 64>>);
}

TEST_CASE("заполнение до предела") {
    RingBuffer<int, 4> ring;
    for (int i = 1; i <= 4; ++i) {
        ring.PushBack(i);
    }

    REQUIRE(ring.size() == 4);
    CHECK(ring.full());
    CHECK(ring[0] == 1);
    CHECK(ring[3] == 4);
    CHECK(ring.back() == 4);
}

TEST_CASE("новое затирает самое старое") {
    RingBuffer<int, 4> ring;
    for (int i = 1; i <= 6; ++i) {
        ring.PushBack(i);
    }

    // Вместимость четыре, вставили шесть — остались последние четыре.
    REQUIRE(ring.size() == 4);
    CHECK(ring[0] == 3);
    CHECK(ring[1] == 4);
    CHECK(ring[2] == 5);
    CHECK(ring[3] == 6);
    CHECK(ring.back() == 6);
}

TEST_CASE("обход в логическом порядке, от старого к новому") {
    RingBuffer<int, 3> ring;
    for (int i = 1; i <= 5; ++i) {
        ring.PushBack(i);
    }

    std::vector<int> seen;
    for (const int value : ring) {
        seen.push_back(value);
    }

    REQUIRE(seen.size() == 3);
    CHECK(seen[0] == 3);
    CHECK(seen[1] == 4);
    CHECK(seen[2] == 5);
}

TEST_CASE("обход пустого буфера не делает ничего") {
    const RingBuffer<int, 4> ring;

    int visits = 0;
    for (const int value : ring) {
        (void)value;
        ++visits;
    }

    CHECK(visits == 0);
}

TEST_CASE("очистка возвращает буфер в исходное состояние") {
    RingBuffer<int, 4> ring;
    for (int i = 1; i <= 6; ++i) {
        ring.PushBack(i);
    }

    ring.Clear();

    REQUIRE(ring.empty());
    ring.PushBack(42);
    CHECK(ring.size() == 1);
    CHECK(ring[0] == 42);
}

TEST_CASE("частично заполненный буфер обходится только по записанному") {
    RingBuffer<int, 4> ring;
    ring.PushBack(1);
    ring.PushBack(2);

    REQUIRE(ring.size() == 2);
    CHECK_FALSE(ring.full());
    CHECK(ring[1] == 2);
    CHECK(ring.back() == 2);

    std::vector<int> seen;
    for (const int value : ring) {
        seen.push_back(value);
    }
    CHECK(seen == std::vector<int>{1, 2});
}

TEST_CASE("после многих оборотов остаются последние N по порядку") {
    // Шесть вставок в буфер на четыре места сдвигают начало на два, и ошибка
    // в переходе через конец массива может не проявиться. Сто три вставки —
    // это двадцать пять полных оборотов.
    RingBuffer<int, 4> ring;
    for (int i = 1; i <= 103; ++i) {
        ring.PushBack(i);
    }

    REQUIRE(ring.size() == 4);
    CHECK(ring[0] == 100);
    CHECK(ring[3] == 103);
    CHECK(ring.back() == 103);

    std::vector<int> seen;
    for (const int value : ring) {
        seen.push_back(value);
    }
    CHECK(seen == std::vector<int>{100, 101, 102, 103});
}

TEST_CASE("очистка после затирания, затем снова больше N вставок") {
    RingBuffer<int, 4> ring;
    for (int i = 1; i <= 6; ++i) {
        ring.PushBack(i);
    }
    ring.Clear();
    for (int i = 7; i <= 12; ++i) {
        ring.PushBack(i);
    }

    std::vector<int> seen;
    for (const int value : ring) {
        seen.push_back(value);
    }
    CHECK(seen == std::vector<int>{9, 10, 11, 12});
}

TEST_CASE("буфер на одно место") {
    RingBuffer<int, 1> ring;
    ring.PushBack(1);
    REQUIRE(ring.full());
    CHECK(ring[0] == 1);

    ring.PushBack(2);
    CHECK(ring.size() == 1);
    CHECK(ring[0] == 2);
    CHECK(ring.back() == 2);

    int visits = 0;
    for (const int value : ring) {
        CHECK(value == 2);
        ++visits;
    }
    CHECK(visits == 1);
}

TEST_CASE("буфер хранит не только числа") {
    // Окно агента кладёт в буфер записи со строками. Затирание строки —
    // присваивание объекта, а не перезапись байтов.
    RingBuffer<std::string, 2> ring;
    ring.PushBack("первая");
    ring.PushBack("вторая");
    ring.PushBack("третья");

    REQUIRE(ring.size() == 2);
    CHECK(ring[0] == "вторая");
    CHECK(ring.back() == "третья");

    ring.Clear();
    ring.PushBack("четвёртая");
    CHECK(ring.size() == 1);
    CHECK(ring[0] == "четвёртая");
}

// ---------------------------------------------------------------------------
// «Не аллоцирует вообще» — проверяемое утверждение
// ---------------------------------------------------------------------------

// Подмена глобального operator new. Грубо, но иначе утверждение
// «не аллоцирует» остаётся словами.
//
// Под ASan на Windows подменять нельзя: среда ASan там компонуется
// статически и определяет operator new сама, и линкер видит два
// определения (duplicate symbol). Тогда подмены нет, а случай ниже
// помечен пропущенным. На Linux ASan перехватывает слабыми символами,
// и подмена работает и под ним.
#if defined(_WIN32) && defined(__has_feature)
#if __has_feature(address_sanitizer)
#define NANO_EDR_ASAN_OWNS_NEW 1
#endif
#endif

#ifdef NANO_EDR_ASAN_OWNS_NEW

TEST_CASE("кольцевой буфер не аллоцирует вообще" * doctest::skip()) {
    // Считать нечем: operator new принадлежит ASan. Проверяется
    // в сборке без санитайзера.
}

#else

namespace {

std::size_t allocation_count = 0;
bool counting_allocations = false;

}  // namespace

void* operator new(std::size_t size) {
    if (counting_allocations) {
        ++allocation_count;
    }
    void* p = std::malloc(size);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t size) { return operator new(size); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

TEST_CASE("кольцевой буфер не аллоцирует вообще") {
    // Критерий занятия дословно. Проверяется на типе без собственных
    // аллокаций: у Event внутри строки, и они аллоцируют сами по себе —
    // проверять на нём было бы бессмысленно.
    //
    // Создание буфера — внутри окна счёта, а не до него. «Не аллоцирует
    // вообще» значит в том числе «не аллоцирует при создании»: реализация
    // на std::vector с одним выделением в конструкторе тоже не выделяет
    // на вставку, но памятью в объекте уже не является.
    std::uint64_t sum = 0;

    allocation_count = 0;
    counting_allocations = true;
    {
        RingBuffer<Timestamp, 64> ring;
        for (int i = 0; i < 10000; ++i) {
            ring.PushBack(Timestamp{static_cast<std::uint64_t>(i)});
        }
        for (const Timestamp& stamp : ring) {
            sum += stamp.ms;
        }
    }
    counting_allocations = false;

    CHECK(allocation_count == 0);
    CHECK(sum > 0);  // чтобы цикл не выбросили
}

#endif  // NANO_EDR_ASAN_OWNS_NEW

// ---------------------------------------------------------------------------
// ThresholdRule: границы
// ---------------------------------------------------------------------------

TEST_CASE("девятнадцать не срабатывает") {
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 20, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));

    for (int i = 0; i < 19; ++i) {
        CHECK_FALSE(rule.Check(
            MakeWrite("42", 1000 + static_cast<std::uint64_t>(i) * 100, "a")));
    }
    CHECK(rule.hits() == 0);
}

TEST_CASE("ровно двадцать срабатывает") {
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 20, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));

    for (int i = 0; i < 19; ++i) {
        rule.Check(
            MakeWrite("42", 1000 + static_cast<std::uint64_t>(i) * 100, "a"));
    }
    CHECK(rule.Check(MakeWrite("42", 1000 + 19 * 100, "a")));
    CHECK(rule.hits() == 1);
}

TEST_CASE("двадцать за одиннадцать секунд не срабатывает") {
    // Порог не в количестве, а в скорости. Двадцать записей за одиннадцать
    // секунд — обычная работа, за десять — уже нет.
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 20, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));

    for (int i = 0; i < 20; ++i) {
        // Шаг 580 мс: двадцать событий укладываются в 11,02 секунды.
        const std::uint64_t ts = 1000 + static_cast<std::uint64_t>(i) * 580;
        CHECK_FALSE(rule.Check(MakeWrite("42", ts, "a")));
    }
    CHECK(rule.hits() == 0);
}

TEST_CASE("порог считается по ключу, а не по всему хосту") {
    // Иначе десять безобидных процессов сложились бы в один детект.
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 5, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));

    // По четыре события от каждого из трёх процессов — двенадцать всего,
    // и ни одного детекта.
    for (int i = 0; i < 4; ++i) {
        for (const char* pid : {"1", "2", "3"}) {
            rule.Check(MakeWrite(
                pid, 1000 + static_cast<std::uint64_t>(i) * 100, "a"));
        }
    }
    CHECK(rule.hits() == 0);
    CHECK(rule.bucket_count() == 3);

    // Пятое от первого — детект.
    CHECK(rule.Check(MakeWrite("1", 1500, "a")));
    CHECK(rule.hits() == 1);
}

TEST_CASE("после детекта счёт начинается заново") {
    // Иначе одно превышение порога давало бы детект на каждом следующем
    // событии, и на потоке это стало бы шумом.
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 3, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));

    rule.Check(MakeWrite("42", 1000, "a"));
    rule.Check(MakeWrite("42", 1100, "a"));
    CHECK(rule.Check(MakeWrite("42", 1200, "a")));        // третье — детект
    CHECK_FALSE(rule.Check(MakeWrite("42", 1300, "a")));  // счёт начат заново
    CHECK(rule.hits() == 1);
}

TEST_CASE("события, не подходящие под условие, не считаются") {
    ThresholdRule<64> rule("mass_write", Severity::kHigh, 3, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));

    EventParts other;
    other.ts = "1000";
    other.type = "net_connect";
    other.pid = "42";

    for (int i = 0; i < 10; ++i) {
        CHECK_FALSE(rule.Check(Event(other)));
    }
    CHECK(rule.hits() == 0);
}

TEST_CASE("порог больше вместимости буфера не срабатывает") {
    // Правило, которое не может сработать, лучше правила, которое срабатывает
    // не так. Буфер помнит четыре отметки, порог просит десять.
    ThresholdRule<4> rule("impossible", Severity::kLow, 10, 10000, "pid");
    rule.SetCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));

    for (int i = 0; i < 50; ++i) {
        CHECK_FALSE(rule.Check(
            MakeWrite("42", 1000 + static_cast<std::uint64_t>(i) * 10, "a")));
    }
    CHECK(rule.hits() == 0);
}

// ---------------------------------------------------------------------------
// Комбинаторы на шаблонах
// ---------------------------------------------------------------------------

TEST_CASE("AllOfStatic требует всех") {
    const Event event = MakeWrite("42", 1000, "C:\\work\\a.locked");

    const AllOfStatic all(EventTypeIs({EventType::kFileWrite}),
                          FieldEndsWith("path", ".locked"));
    CHECK(all(event));

    const AllOfStatic broken(EventTypeIs({EventType::kFileWrite}),
                             FieldEndsWith("path", ".docx"));
    CHECK_FALSE(broken(event));
}

TEST_CASE("AnyOfStatic требует хотя бы одного") {
    const Event event = MakeWrite("42", 1000, "C:\\work\\a.locked");

    const AnyOfStatic any(FieldEndsWith("path", ".docx"),
                          FieldEndsWith("path", ".locked"));
    CHECK(any(event));

    const AnyOfStatic none(FieldEndsWith("path", ".docx"),
                           FieldEndsWith("path", ".xlsx"));
    CHECK_FALSE(none(event));
}

TEST_CASE("пустые комбинаторы: AllOfStatic истина, AnyOfStatic ложь") {
    // То же соглашение, что у динамических: нейтральные элементы для && и ||.
    // Асимметрия не выдумана — она следует из самих операторов.
    const Event event = MakeWrite("42", 1000, "a");

    const AllOfStatic<> all;
    const AnyOfStatic<> any;

    CHECK(all(event));
    CHECK_FALSE(any(event));
}

TEST_CASE("комбинаторы вкладываются друг в друга") {
    const Event event = MakeWrite("42", 1000, "C:\\work\\a.locked");

    const AllOfStatic nested(
        EventTypeIs({EventType::kFileWrite}),
        AnyOfStatic(FieldEndsWith("path", ".locked"),
                    FieldEndsWith("path", ".encrypted")));

    CHECK(nested(event));
}

TEST_CASE("шаблонный комбинатор подключается к движку через мост") {
    // Движок разговаривает через ICondition; комбинатор его не реализует
    // и не должен — реализовав, потерял бы то, за чем создан. Мост даёт
    // один виртуальный вызов на весь составной комбинатор.
    const Event event = MakeWrite("42", 1000, "C:\\work\\a.locked");

    MatchRule rule("ransom", Severity::kCritical);
    rule.AddCondition(MakeCondition(
        AllOfStatic(EventTypeIs({EventType::kFileWrite}),
                    FieldEndsWith("path", ".locked"))));

    CHECK(rule.Check(event));
    CHECK_FALSE(rule.Check(MakeWrite("42", 1100, "C:\\work\\a.docx")));
}

// ---------------------------------------------------------------------------
// MakeRule
// ---------------------------------------------------------------------------

TEST_CASE("фабрика создаёт правило нужного типа") {
    auto match = MakeRule<MatchRule>("m", Severity::kLow);
    CHECK(match->id() == "m");
    CHECK(match->severity() == Severity::kLow);

    auto threshold = MakeRule<ThresholdRule<64>>("t", Severity::kHigh,
                                                 std::size_t(20),
                                                 std::uint64_t(10000),
                                                 std::string("pid"));
    CHECK(threshold->id() == "t");
    CHECK(threshold->severity() == Severity::kHigh);
}

TEST_CASE("фабрика передаёт конструктору все аргументы") {
    // Аргументы без явных типов, как в постановке. Что count и окно дошли
    // до правила, видно по поведению: порог три — третье событие детект.
    auto rule = MakeRule<ThresholdRule<8>>("t", Severity::kHigh, 3, 10000,
                                           "pid");
    rule->SetCondition(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));

    CHECK_FALSE(rule->Check(MakeWrite("42", 1000, "a")));
    CHECK_FALSE(rule->Check(MakeWrite("42", 1100, "a")));
    CHECK(rule->Check(MakeWrite("42", 1200, "a")));

    auto beacon = MakeRule<SequenceRule>("beacon", Severity::kCritical, 10000);
    CHECK(beacon->id() == "beacon");
    CHECK(beacon->severity() == Severity::kCritical);
}

// ---------------------------------------------------------------------------
// FieldTraits
// ---------------------------------------------------------------------------

TEST_CASE("типизированный доступ к полю") {
    const Event event = MakeWrite("42", 1000, "C:\\work\\a.txt");

    std::string path;
    REQUIRE(GetField<std::string>(event, "path", &path));
    CHECK(path == "C:\\work\\a.txt");

    std::uint64_t size = 0;
    REQUIRE(GetField<std::uint64_t>(event, "size", &size));
    CHECK(size == 4096);
}

TEST_CASE("шапка события тоже доступна по имени") {
    const Event event = MakeWrite("42", 1000, "a");

    std::uint64_t pid = 0;
    REQUIRE(GetField<std::uint64_t>(event, "pid", &pid));
    CHECK(pid == 42);
}

TEST_CASE("отсутствующее поле — false, а не исключение") {
    const Event event = MakeWrite("42", 1000, "a");

    std::uint64_t rport = 7;
    CHECK_FALSE(GetField<std::uint64_t>(event, "rport", &rport));
}

TEST_CASE("неразбираемое значение — false") {
    EventParts parts;
    parts.ts = "1000";
    parts.type = "file_write";
    parts.pid = "42";
    parts.fields.push_back(Field{"size", "много"});
    parts.fields.push_back(Field{"huge", "99999999999999999999999999"});
    const Event event(parts);

    std::uint64_t value = 0;
    CHECK_FALSE(GetField<std::uint64_t>(event, "size", &value));
    CHECK_FALSE(GetField<std::uint64_t>(event, "huge", &value));
}

TEST_CASE("флаги разбираются в bool") {
    EventParts parts;
    parts.ts = "1000";
    parts.type = "file_write";
    parts.fields.push_back(Field{"a", "1"});
    parts.fields.push_back(Field{"b", "true"});
    parts.fields.push_back(Field{"c", "0"});
    parts.fields.push_back(Field{"d", "no"});
    parts.fields.push_back(Field{"e", "может быть"});
    const Event event(parts);

    bool value = false;
    REQUIRE(GetField<bool>(event, "a", &value));
    CHECK(value);
    REQUIRE(GetField<bool>(event, "b", &value));
    CHECK(value);
    REQUIRE(GetField<bool>(event, "c", &value));
    CHECK_FALSE(value);
    REQUIRE(GetField<bool>(event, "d", &value));
    CHECK_FALSE(value);
    CHECK_FALSE(GetField<bool>(event, "e", &value));
}

TEST_CASE("значение по умолчанию") {
    const Event event = MakeWrite("42", 1000, "a");

    CHECK(GetFieldOr<std::uint64_t>(event, "size", 7) == 4096);
    CHECK(GetFieldOr<std::uint64_t>(event, "rport", 7) == 7);
}

// Дальше — Parse напрямую, строка за строкой из таблицы постановки.

TEST_CASE("строка берётся как есть, в том числе пустая") {
    std::string value = "прежнее";
    CHECK(FieldTraits<std::string>::Parse("", &value));
    CHECK(value.empty());
    CHECK(FieldTraits<std::string>::Parse(" 42 ", &value));
    CHECK(value == " 42 ");
}

TEST_CASE("число: ноль и границы uint64_t") {
    std::uint64_t value = 7;
    REQUIRE(FieldTraits<uint64_t>::Parse("0", &value));
    CHECK(value == 0);

    REQUIRE(FieldTraits<uint64_t>::Parse("18446744073709551615", &value));
    CHECK(value == UINT64_MAX);

    // На единицу больше максимума. И число, у которого переполнение
    // при умножении на 10 даёт результат больше прежнего: проверка
    // «новое меньше старого» его не замечает.
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("18446744073709551616", &value));
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("20000000000000000000", &value));
}

TEST_CASE("число: только десятичные цифры") {
    // std::stoull принимает пробел и знак в начале и мусор в конце, а "-1"
    // превращает в 18446744073709551615. Здесь всё это — false.
    std::uint64_t value = 0;
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("", &value));
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("+42", &value));
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("-1", &value));
    CHECK_FALSE(FieldTraits<uint64_t>::Parse(" 42", &value));
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("42 ", &value));
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("12ab", &value));
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("0x10", &value));
}

TEST_CASE("флаги: все шесть слов и регистр") {
    bool value = false;
    REQUIRE(FieldTraits<bool>::Parse("yes", &value));
    CHECK(value);
    REQUIRE(FieldTraits<bool>::Parse("false", &value));
    CHECK_FALSE(value);
    REQUIRE(FieldTraits<bool>::Parse("Yes", &value));
    CHECK(value);
    REQUIRE(FieldTraits<bool>::Parse("FALSE", &value));
    CHECK_FALSE(value);
    REQUIRE(FieldTraits<bool>::Parse("True", &value));
    CHECK(value);
    REQUIRE(FieldTraits<bool>::Parse("No", &value));
    CHECK_FALSE(value);

    CHECK_FALSE(FieldTraits<bool>::Parse("", &value));
    CHECK_FALSE(FieldTraits<bool>::Parse("y", &value));
    CHECK_FALSE(FieldTraits<bool>::Parse("2", &value));
}

TEST_CASE("при отказе значение по указателю не меняется") {
    // Отказ — «поля нет или оно не разбирается», и вызывающий вправе
    // рассчитывать, что прежнее значение на месте. Разбор, который копит
    // число прямо в *out, ломается на "12ab" после первых двух цифр.
    std::uint64_t number = 7;
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("12ab", &number));
    CHECK(number == 7);
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("18446744073709551616", &number));
    CHECK(number == 7);
    CHECK_FALSE(FieldTraits<uint64_t>::Parse("", &number));
    CHECK(number == 7);

    bool flag = true;
    CHECK_FALSE(FieldTraits<bool>::Parse("может быть", &flag));
    CHECK(flag);
}
