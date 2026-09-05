// Тесты move-семантики и цены копий. Занятие 3.3.
//
// Что здесь проверяется и почему именно так.
//
// Про копирование нельзя договориться на словах. «Здесь move» и «здесь копия»
// — утверждения одного качества, пока их не проверил компилятор или счётчик.
// Поэтому тесты этого занятия делятся ровно на два вида:
//
//   static_assert   то, что обязан проверить компилятор: правило пяти
//                   и правило нуля, noexcept у перемещения, запрет копирования;
//   счётчик копий   то, что компилятор проверить не может: какая перегрузка
//                   выбралась на конкретной строке вашего кода.
//
// Второй вид важнее первого. Ошибка «забыл std::move» не даёт ни ошибки,
// ни предупреждения — код работает, просто вдвое больше аллокаций. Единственное,
// что её ловит, — число.

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "copy_stats.h"
#include "detection.h"
#include "doctest.h"
#include "event.h"
#include "function.h"
#include "make_rule.h"
#include "ring_buffer.h"
#include "rule.h"
#include "rules.h"
#include "window_entry.h"

using nano_edr::CopyStats;
using nano_edr::Detection;
using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::Field;
using nano_edr::LambdaRule;
using nano_edr::MakeRule;
using nano_edr::ResetCopyStats;
using nano_edr::RingBuffer;
using nano_edr::Severity;
using nano_edr::Timestamp;
using nano_edr::WindowEntry;
using nano_edr::copy_stats;

namespace {

EventParts MakeParts(std::uint64_t ts_ms, const std::string& type) {
    EventParts parts;
    parts.ts = std::to_string(ts_ms);
    parts.type = type;
    parts.pid = "1042";
    // Строки заведомо длиннее пятнадцати символов: короткие std::string лежат
    // внутри самого объекта и не аллоцируют, и на них разница между копией
    // и перемещением не видна вовсе. Замер на коротких строках показал бы,
    // что move ничего не даёт, — и это был бы правдивый замер неправильной
    // величины.
    parts.fields.push_back(Field{"path", "C:\\Users\\max\\AppData\\Temp\\a.js"});
    parts.fields.push_back(Field{"image", "C:\\Windows\\System32\\wscript.exe"});
    return parts;
}

}  // namespace

// ---------------------------------------------------------------------------
// Правило пяти и правило нуля — то, что проверяет компилятор
// ---------------------------------------------------------------------------

TEST_CASE("Event следует правилу нуля") {
    // Ни одна из пяти функций у Event не написана, и все пять есть.
    // Компилятор сгенерировал их по полям — и сгенерировал правильно.
    static_assert(std::is_copy_constructible_v<Event>,
                  "событие обязано копироваться: без этого не собрать улику");
    static_assert(std::is_move_constructible_v<Event>);
    static_assert(std::is_move_assignable_v<Event>);

    // Вот главное. noexcept у перемещения — не украшение: именно от него
    // зависит, будет ли vector при росте перемещать элементы или копировать.
    // Никто его не писал, и он верен, потому что верен у каждого поля.
    static_assert(std::is_nothrow_move_constructible_v<Event>,
                  "перемещение события обязано не бросать");

    // Конструктора по умолчанию нет и быть не должно: у Event инвариант,
    // и «пустое событие» ему противоречит.
    static_assert(!std::is_default_constructible_v<Event>);
}

TEST_CASE("Detection следует правилу пяти") {
    // Копирование запрещено явно. Сработка — единичный факт, и её копия
    // означала бы второй доклад наверх.
    static_assert(!std::is_copy_constructible_v<Detection>,
                  "сработка не должна копироваться");
    static_assert(!std::is_copy_assignable_v<Detection>);

    static_assert(std::is_nothrow_move_constructible_v<Detection>);
    static_assert(std::is_nothrow_move_assignable_v<Detection>);

    // При этом создать её по умолчанию можно: собирает её движок по полям.
    static_assert(std::is_default_constructible_v<Detection>);
}

TEST_CASE("сработка перемещается вместе с уликами") {
    Detection first;
    first.rule = "test_rule";
    first.pid = "1042";
    first.evidence.push_back(std::make_unique<Event>(MakeParts(1000, "process_start")));

    const Event* address = first.evidence[0].get();

    Detection second = std::move(first);

    REQUIRE(second.evidence.size() == 1);
    CHECK(second.rule == "test_rule");
    // Улика не пересоздавалась: перемещение вектора указателей переносит
    // указатели, а не то, на что они указывают.
    CHECK(second.evidence[0].get() == address);
}

// ---------------------------------------------------------------------------
// Два конструктора события
// ---------------------------------------------------------------------------

TEST_CASE("копирующий конструктор события копирует строки") {
    ResetCopyStats();

    EventParts parts = MakeParts(1000, "process_start");
    const Event event(parts);

    // Три строки шапки плюс по две на каждое из двух полей.
    CHECK(copy_stats().event_strings_copied == 7);
    CHECK(copy_stats().event_strings_moved == 0);

    // Части остались пригодными — это и есть смысл копирующей формы.
    CHECK(parts.type == "process_start");
    CHECK(parts.fields.size() == 2);
    CHECK(event.type() == "process_start");
}

TEST_CASE("перемещающий конструктор события не копирует ничего") {
    ResetCopyStats();

    const Event event(MakeParts(1000, "process_start"));

    CHECK(copy_stats().event_strings_copied == 0);
    CHECK(copy_stats().event_strings_moved == 7);

    // И при этом событие построено полностью.
    CHECK(event.ts().ms == 1000);
    CHECK(event.type() == "process_start");
    CHECK(event.pid() == "1042");
    REQUIRE(event.fields().size() == 2);
    CHECK(event.fields()[0].key == "path");
}

TEST_CASE("именованные части требуют std::move") {
    ResetCopyStats();

    EventParts parts = MakeParts(1000, "process_start");
    // Без std::move здесь выбралась бы копирующая перегрузка: parts —
    // именованная переменная, то есть lvalue. Это самая частая ошибка первого
    // кода на move-семантике, и компилятор о ней молчит.
    const Event event(std::move(parts));

    CHECK(copy_stats().event_strings_copied == 0);
    CHECK(copy_stats().event_strings_moved == 7);
    CHECK(event.type() == "process_start");
}

TEST_CASE("битая строка не портит части до проверки") {
    EventParts parts = MakeParts(0, "process_start");
    parts.ts = "не число";

    // Перемещающий конструктор обязан сначала проверить инвариант и только
    // потом забирать строки. Иначе на битой строке журнала вызывающий остался
    // бы с пустыми частями вместо своих данных — и не узнал бы об этом.
    CHECK_THROWS_AS([&] { const Event event(std::move(parts)); }(),
                    std::invalid_argument);

    CHECK(parts.type == "process_start");
    CHECK(parts.pid == "1042");
    CHECK(parts.fields.size() == 2);
}

// ---------------------------------------------------------------------------
// Окно: две перегрузки PushBack
// ---------------------------------------------------------------------------

TEST_CASE("запись окна копируется, если её не переместить") {
    ResetCopyStats();

    RingBuffer<WindowEntry, 4> window;
    WindowEntry entry;
    entry.ts = Timestamp{1000};
    entry.type = "process_start";
    entry.pid = "1042";

    window.PushBack(entry);

    CHECK(copy_stats().window_copied == 1);
    CHECK(copy_stats().window_moved == 0);
    CHECK(entry.type == "process_start");  // источник цел
}

TEST_CASE("запись окна перемещается по std::move") {
    ResetCopyStats();

    RingBuffer<WindowEntry, 4> window;
    WindowEntry entry;
    entry.ts = Timestamp{1000};
    entry.type = "process_start";
    entry.pid = "1042";

    window.PushBack(std::move(entry));

    CHECK(copy_stats().window_copied == 0);
    CHECK(copy_stats().window_moved == 1);
    REQUIRE(window.size() == 1);
    CHECK(window[0].type == "process_start");
}

TEST_CASE("вытеснение в кольцевом буфере не копирует лишнего") {
    ResetCopyStats();

    RingBuffer<WindowEntry, 2> window;
    for (int i = 0; i < 5; ++i) {
        WindowEntry entry;
        entry.ts = Timestamp{static_cast<std::uint64_t>(i)};
        entry.type = "process_start";
        window.PushBack(std::move(entry));
    }

    // Пять записей — пять перемещений и ни одной копии. Вытеснение
    // в кольцевом буфере это перезапись на месте, а не сдвиг содержимого.
    CHECK(copy_stats().window_moved == 5);
    CHECK(copy_stats().window_copied == 0);
    CHECK(window.size() == 2);
    CHECK(window[0].ts.ms == 3);
}

// ---------------------------------------------------------------------------
// Фабрика правил и move-only аргумент
// ---------------------------------------------------------------------------

TEST_CASE("фабрика принимает move-only предикат") {
    // Ровно тот вызов, который не собирался на занятии 3.2: предикат сначала
    // собран в Function, и только потом уезжает в фабрику. Чинится он одним
    // словом в теле вашей MakeRule — `std::move(args)...`.
    //
    // Обратите внимание, что вызов с самой лямбдой собирался и тогда: лямбда,
    // захватившая указатель, копируется. Ломался не «вызов с лямбдой»,
    // а «вызов с move-only аргументом», и разница здесь принципиальна.
    std::size_t calls = 0;
    LambdaRule::Predicate predicate = [&calls](const Event&) {
        ++calls;
        return true;
    };

    auto rule = MakeRule<LambdaRule>("moved_predicate", Severity::kHigh,
                                     std::move(predicate));

    REQUIRE(rule != nullptr);
    CHECK(rule->id() == "moved_predicate");

    const Event event(MakeParts(1000, "process_start"));
    CHECK(rule->Check(event));
    CHECK(calls == 1);
    CHECK(rule->hits() == 1);
}

TEST_CASE("фабрика принимает и обычную лямбду") {
    auto rule = MakeRule<LambdaRule>(
        "plain_lambda", Severity::kLow,
        [](const Event& event) { return event.pid() == "1042"; });

    REQUIRE(rule != nullptr);
    const Event event(MakeParts(1000, "process_start"));
    CHECK(rule->Check(event));
}

// ---------------------------------------------------------------------------
// Счётчик копий как инструмент
// ---------------------------------------------------------------------------

TEST_CASE("счётчик обнуляется и суммирует") {
    ResetCopyStats();
    CHECK(copy_stats().total() == 0);

    const Event first(MakeParts(1000, "process_start"));
    const Event second(MakeParts(2000, "process_end"));
    CHECK(first.ts().ms == 1000);
    CHECK(second.ts().ms == 2000);

    // Перемещения в total не входят: это счётчик копий, а не счётчик работы.
    CHECK(copy_stats().total() == 0);
    CHECK(copy_stats().event_strings_moved == 14);

    EventParts parts = MakeParts(3000, "file_write");
    const Event third(parts);
    CHECK(third.ts().ms == 3000);
    CHECK(copy_stats().total() == 7);
}
