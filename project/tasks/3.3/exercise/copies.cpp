// Примитив занятия 3.3: сколько стоит копия. См. README.md рядом.
//
// Собирать только с оптимизацией:
//   g++ -std=c++23 -O2 -Wall -Wextra -o copies copies.cpp
//   ./copies
//
// Здесь нет ни симулятора, ни агента — один файл, который можно унести
// и запустить где угодно. Разбирается одна вещь: во что обходится строка
// `parts.fields = other.fields;` и что меняет одно слово перед ней.

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {

// --- учёт аллокаций --------------------------------------------------------
//
// Глобальный operator new перехватывается целиком. Это законно и это
// единственный способ увидеть аллокации, не заглядывая в реализацию
// std::string: свои конструкторы можно посчитать, чужие — нет.

std::size_t g_allocations = 0;
std::size_t g_bytes = 0;

}  // namespace

void* operator new(std::size_t size) {
    ++g_allocations;
    g_bytes += size;
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    return memory;
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

// --- то, что копируем ------------------------------------------------------

struct Field {
    std::string key;
    std::string value;
};

// Части события — ровно как в агенте: агрегат без обещаний, который живёт
// от разбора строки до создания события.
struct Parts {
    std::string ts;
    std::string type;
    std::string pid;
    std::vector<Field> fields;
};

// ВАЖНО ПРО ДЛИНУ СТРОК
//
// Короткая std::string (примерно до пятнадцати символов) лежит внутри самого
// объекта и не аллоцирует вовсе. Замер на коротких строках покажет, что move
// не даёт ничего, — и это будет правдивый замер неправильной величины.
//
// Поэтому пути здесь настоящей длины. Ровно такие приходят из журнала.
Parts MakeParts(std::size_t n) {
    Parts parts;
    parts.ts = "1730000001000";
    parts.type = "process_start";
    parts.pid = std::to_string(1000 + n);
    parts.fields.push_back(
        Field{"image", "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"});
    parts.fields.push_back(
        Field{"cmdline", "powershell.exe -w hidden -enc SQBFAFgAIAAoAE4AZQB3AC0ATwBiAA=="});
    parts.fields.push_back(
        Field{"path", "C:\\Users\\max\\AppData\\Local\\Temp\\stage2.bin"});
    parts.fields.push_back(Field{"user", "DESKTOP\\max"});
    return parts;
}

// Событие с инвариантом — упрощённое до одной проверки, но с той же
// структурой: два конструктора, общая проверка, поля по значению.
class Event {
 public:
    explicit Event(const Parts& parts)
        : ts_(parts.ts),
          type_(parts.type),
          pid_(parts.pid),
          fields_(parts.fields) {
        Validate();
    }

    explicit Event(Parts&& parts)
        : ts_(std::move(parts.ts)),
          type_(std::move(parts.type)),
          pid_(std::move(parts.pid)),
          fields_(std::move(parts.fields)) {
        Validate();
    }

    const std::string& pid() const { return pid_; }
    std::size_t field_count() const { return fields_.size(); }

 private:
    void Validate() const {
        if (type_.empty()) {
            std::fputs("событие без типа\n", stderr);
            std::abort();
        }
    }

    std::string ts_;
    std::string type_;
    std::string pid_;
    std::vector<Field> fields_;
};

// --- замер -----------------------------------------------------------------

struct Result {
    std::size_t allocations = 0;
    std::size_t bytes = 0;
    double nanos = 0.0;
    std::size_t checksum = 0;
};

constexpr std::size_t kEvents = 200000;

// Контрольная сумма нужна, чтобы компилятор не выбросил всю работу целиком.
// Без неё цикл, результат которого никому не нужен, законно исчезает,
// и замер показывает ноль наносекунд на любую из двух версий.
template <typename Build>
Result Measure(Build build) {
    g_allocations = 0;
    g_bytes = 0;

    std::size_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kEvents; ++i) {
        const Event event = build(i);
        checksum += event.pid().size() + event.field_count();
    }
    const auto finish = std::chrono::steady_clock::now();

    Result result;
    result.allocations = g_allocations;
    result.bytes = g_bytes;
    result.nanos =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start)
                .count()) /
        static_cast<double>(kEvents);
    result.checksum = checksum;
    return result;
}

void Print(const char* name, const Result& result) {
    std::printf("%-28s %10zu %12zu %9.1f\n", name,
                result.allocations / kEvents, result.bytes / kEvents,
                result.nanos);
}

}  // namespace

int main() {
    std::printf("событий: %zu\n\n", kEvents);
    std::printf("%-28s %10s %12s %9s\n", "как строим", "аллокаций", "байт",
                "нс");
    std::printf("%-28s %10s %12s %9s\n", "", "на событие", "на событие", "");

    // Первая версия: части живут в именованной переменной, событие строится
    // из константной ссылки. Ровно так выглядел агент до занятия 3.3.
    const Result copied = Measure([](std::size_t i) {
        Parts parts = MakeParts(i);
        return Event(parts);
    });
    Print("копия из именованных частей", copied);

    // Вторая: одно слово. Части после этой строки не нужны, и событие
    // забирает их строки вместо того, чтобы копировать каждую.
    const Result moved = Measure([](std::size_t i) {
        Parts parts = MakeParts(i);
        return Event(std::move(parts));
    });
    Print("move из именованных частей", moved);

    // Третья: временный объект. std::move не нужен — временный объект и так
    // rvalue, и перегрузка выберется та же. Числа обязаны совпасть со второй
    // строкой; если не совпали, где-то потерялась перегрузка.
    const Result temporary = Measure([](std::size_t i) {
        return Event(MakeParts(i));
    });
    Print("move из временного объекта", temporary);

    std::printf("\n");
    if (copied.allocations > 0 && moved.allocations > 0) {
        std::printf("аллокаций стало меньше в %.2f раза\n",
                    static_cast<double>(copied.allocations) /
                        static_cast<double>(moved.allocations));
    }
    if (moved.nanos > 0.0) {
        std::printf("времени стало меньше в %.2f раза\n",
                    copied.nanos / moved.nanos);
    }

    // Суммы печатаются, чтобы их нельзя было выбросить.
    std::printf("\nконтрольные суммы: %zu %zu %zu\n", copied.checksum,
                moved.checksum, temporary.checksum);
    return 0;
}
