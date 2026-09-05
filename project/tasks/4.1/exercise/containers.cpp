// Примитив занятия 4.1: что заменять, а что оставить.
//
// Один файл, три таблицы. Первая — окно событий на четырёх контейнерах,
// вторая — доступ к предпоследнему элементу, третья — поиск записи по номеру.
//
// Сборка:
//   g++ -std=c++23 -O2 -Wall -Wextra -o containers containers.cpp
//   cl /std:c++latest /O2 /W4 /EHsc containers.cpp
//
// ТОЛЬКО RELEASE. В отладочной сборке измеряются проверки стандартной
// библиотеки, а не контейнеры: у MSVC в Debug итераторы проверяемые, и разница
// между deque и vector там своя, к делу не относящаяся.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// Счётчик аллокаций.
//
// Глобальные operator new и operator delete заменяются целиком — все формы,
// а не одна. Заменить одну и оставить остальные нельзя: библиотека вправе
// звать массивную форму или форму с размером, и тогда память, выданная одним
// аллокатором, вернётся другому. g++ на такое ругается предупреждением
// -Wmismatched-new-delete, и ругается по делу.
std::size_t g_allocs = 0;
std::size_t g_bytes = 0;
bool g_counting = false;

struct Timestamp {
    std::uint64_t ms = 0;
};

// Запись окна — та же, что у агента: время и две короткие строки.
//
// Короткие — это важно, и это первое, что стоит заметить до запуска.
// `"process_start"` — тринадцать символов, `"1042"` — четыре; и то и другое
// у libstdc++ и у MSVC умещается внутри самого объекта std::string, без
// обращения к аллокатору. Small String Optimization.
struct Entry {
    Timestamp ts;
    std::string type;
    std::string pid;
};

// --- свой список: то, на чём окно стояло с занятия 1.2 ---------------------

struct Node {
    explicit Node(Entry value) : value(std::move(value)) {}
    Entry value;
    std::unique_ptr<Node> next;
};

class ListWindow {
 public:
    explicit ListWindow(std::size_t capacity) : capacity_(capacity) {}

    ~ListWindow() {
        // Нерекурсивно: цепочка unique_ptr разбирается рекурсией, и глубина
        // стека равна длине списка. Правило из лекции 2.
        while (head_ != nullptr) {
            head_ = std::move(head_->next);
        }
    }

    ListWindow(const ListWindow&) = delete;
    ListWindow& operator=(const ListWindow&) = delete;

    void PushBack(Entry value) {
        if (capacity_ != 0 && size_ >= capacity_) {
            PopFront();
        }
        auto node = std::make_unique<Node>(std::move(value));
        Node* added = node.get();
        if (head_ == nullptr) {
            head_ = std::move(node);
        } else {
            tail_->next = std::move(node);
        }
        tail_ = added;
        ++size_;
    }

    void PopFront() {
        if (head_ == nullptr) {
            return;
        }
        head_ = std::move(head_->next);
        --size_;
        if (head_ == nullptr) {
            tail_ = nullptr;
        }
    }

    const Entry& operator[](std::size_t index) const {
        const Node* node = head_.get();
        for (std::size_t i = 0; i < index; ++i) {
            node = node->next.get();
        }
        return node->value;
    }

    std::size_t size() const { return size_; }

 private:
    std::unique_ptr<Node> head_;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_;
};

// --- свой кольцевой буфер: то, на чём окно стоит с занятия 3.1 --------------

template <typename T, std::size_t N>
class RingWindow {
 public:
    void PushBack(T&& value) {
        storage_[(begin_ + size_) % N] = std::move(value);
        if (size_ < N) {
            ++size_;
        } else {
            begin_ = (begin_ + 1) % N;
        }
    }

    const T& operator[](std::size_t index) const {
        return storage_[(begin_ + index) % N];
    }

    std::size_t size() const { return size_; }

 private:
    std::array<T, N> storage_{};
    std::size_t begin_ = 0;
    std::size_t size_ = 0;
};

// --- рабочая нагрузка ------------------------------------------------------

constexpr std::size_t kCapacity = 256;
constexpr std::size_t kOps = 2000000;

const char* TypeOf(std::size_t i) {
    switch (i % 5) {
        case 0: return "process_start";
        case 1: return "file_write";
        case 2: return "file_create";
        case 3: return "net_connect";
        default: return "process_end";
    }
}

Entry MakeEntry(std::size_t i) {
    Entry entry;
    entry.ts.ms = 1730000000000ULL + i;
    entry.type = TypeOf(i);
    entry.pid = std::to_string(1000 + (i % 4000));
    return entry;
}

struct Result {
    double ms = 0;
    std::size_t allocs = 0;
    std::size_t bytes = 0;
    std::uint64_t checksum = 0;
};

// Контрольная сумма возвращается и печатается не для красоты: без неё
// компилятор вправе выбросить весь цикл, потому что его результат никому
// не нужен. Замер, показавший ноль, — это не быстрый код, это выброшенный код.
template <typename Fn>
Result Measure(Fn body) {
    g_allocs = 0;
    g_bytes = 0;
    g_counting = true;
    const auto start = std::chrono::steady_clock::now();
    const std::uint64_t checksum = body();
    const auto finish = std::chrono::steady_clock::now();
    g_counting = false;

    Result result;
    result.ms = std::chrono::duration<double, std::milli>(finish - start).count();
    result.allocs = g_allocs;
    result.bytes = g_bytes;
    result.checksum = checksum;
    return result;
}

// Выравнивание по столбцам вручную.
//
// `%-24s` у printf считает БАЙТЫ, а не символы, и на кириллице в UTF-8
// столбцы разъезжаются вдвое. Считать надо символы: в UTF-8 первый байт
// символа — любой, кроме продолжающего (10xxxxxx).
std::string Pad(const std::string& text, std::size_t width) {
    std::size_t chars = 0;
    for (const char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++chars;
        }
    }
    return chars >= width ? text : text + std::string(width - chars, ' ');
}

void Row(const char* name, const Result& r) {
    std::printf("  %s %8.0f  %12zu  %12zu\n", Pad(name, 24).c_str(), r.ms,
                r.allocs, r.bytes);
}

// --- поиск записи по номеру ------------------------------------------------

struct Record {
    std::string pid;
    std::uint64_t start = 0;
};

std::uint64_t SearchBench(std::size_t known) {
    std::vector<Record> flat;
    std::unordered_map<std::string, Record> table;
    for (std::size_t i = 0; i < known; ++i) {
        Record record;
        record.pid = std::to_string(1000 + i);
        record.start = 1730000000000ULL + i;
        flat.push_back(record);
        table.emplace(record.pid, record);
    }

    constexpr std::size_t kLookups = 200000;
    std::uint64_t sink = 0;

    // Номер берётся не по порядку: последовательный обход попадал бы в кеш
    // идеально и линейному поиску польстил.
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kLookups; ++i) {
        const std::string want = std::to_string(1000 + (i * 7919) % known);
        for (const Record& record : flat) {
            if (record.pid == want) {
                sink += record.start;
                break;
            }
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kLookups; ++i) {
        const std::string want = std::to_string(1000 + (i * 7919) % known);
        const auto it = table.find(want);
        if (it != table.end()) {
            sink += it->second.start;
        }
    }
    const auto t2 = std::chrono::steady_clock::now();

    const double linear =
        std::chrono::duration<double, std::nano>(t1 - t0).count() / kLookups;
    const double hashed =
        std::chrono::duration<double, std::nano>(t2 - t1).count() / kLookups;
    std::printf("  %8zu  %11.0f  %11.0f  %8.0f x\n", known, linear, hashed,
                linear / hashed);
    return sink;
}

}  // namespace

// -Wmismatched-new-delete здесь срабатывает ложно, и стоит понимать, почему.
// Предупреждение ловит `free` по указателю, полученному из `new`, — и ловит
// по именам функций, а не по тому, что внутри. Здесь `operator new` заменён
// целиком и выдаёт память из `malloc`, так что пара сходится; компилятор
// об этом не знает.
//
// Глушение предупреждения — почти всегда неверный ответ. Этот случай
// исключение ровно потому, что заменить глобальный `operator new` иначе,
// чем через `malloc`, нельзя: любая другая функция выделения внутри него
// либо сама зовёт `operator new`, либо её нет.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void* operator new(std::size_t size) {
    if (g_counting) {
        ++g_allocs;
        g_bytes += size;
    }
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size) { return operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main() {
    std::printf("окно на %zu мест, %zu добавлений\n\n", kCapacity, kOps);
    std::printf("  %s %8s  %12s  %12s\n", Pad("контейнер", 24).c_str(), "мс",
                "аллокаций", "байт");

    Row("свой список", Measure([] {
        ListWindow window(kCapacity);
        std::uint64_t sum = 0;
        for (std::size_t i = 0; i < kOps; ++i) {
            window.PushBack(MakeEntry(i));
            sum += window.size();
        }
        return sum;
    }));

    Row("свой кольцевой буфер", Measure([] {
        RingWindow<Entry, kCapacity> window;
        std::uint64_t sum = 0;
        for (std::size_t i = 0; i < kOps; ++i) {
            window.PushBack(MakeEntry(i));
            sum += window.size();
        }
        return sum;
    }));

    Row("std::deque", Measure([] {
        std::deque<Entry> window;
        std::uint64_t sum = 0;
        for (std::size_t i = 0; i < kOps; ++i) {
            window.push_back(MakeEntry(i));
            if (window.size() > kCapacity) {
                window.pop_front();
            }
            sum += window.size();
        }
        return sum;
    }));

    Row("std::vector + erase", Measure([] {
        std::vector<Entry> window;
        std::uint64_t sum = 0;
        for (std::size_t i = 0; i < kOps; ++i) {
            window.push_back(MakeEntry(i));
            if (window.size() > kCapacity) {
                window.erase(window.begin());
            }
            sum += window.size();
        }
        return sum;
    }));

    // --- доступ по индексу ---------------------------------------------------
    //
    // Агент печатает контекст сработки: два последних события окна. На списке
    // это проход от головы, на остальных — арифметика.
    //
    // Индекс меняется от обращения к обращению, и это не украшение замера.
    // С постоянным индексом компилятор выносит чтение из цикла целиком,
    // и в таблице получается 0,0 нс — число, которое означает «замер сломан»,
    // а не «быстро».
    constexpr std::size_t kReads = 2000000;
    {
        ListWindow list(kCapacity);
        RingWindow<Entry, kCapacity> ring;
        std::deque<Entry> deq;
        for (std::size_t i = 0; i < kCapacity; ++i) {
            list.PushBack(MakeEntry(i));
            ring.PushBack(MakeEntry(i));
            deq.push_back(MakeEntry(i));
        }

        std::uint64_t sink = 0;

        const auto t0 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < kReads; ++i) {
            sink += list[kCapacity - 2 - (i % 2)].ts.ms;
        }
        const auto t1 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < kReads; ++i) {
            sink += ring[kCapacity - 2 - (i % 2)].ts.ms;
        }
        const auto t2 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < kReads; ++i) {
            sink += deq[kCapacity - 2 - (i % 2)].ts.ms;
        }
        const auto t3 = std::chrono::steady_clock::now();

        std::printf("\nдоступ к предпоследнему элементу, %zu обращений\n\n",
                    kReads);
        std::printf("  %s %8s\n", Pad("контейнер", 24).c_str(), "нс");
        std::printf("  %s %8.1f\n", Pad("свой список", 24).c_str(),
                    std::chrono::duration<double, std::nano>(t1 - t0).count() /
                        kReads);
        std::printf("  %s %8.1f\n", Pad("свой кольцевой буфер", 24).c_str(),
                    std::chrono::duration<double, std::nano>(t2 - t1).count() /
                        kReads);
        std::printf("  %s %8.1f\n", Pad("std::deque", 24).c_str(),
                    std::chrono::duration<double, std::nano>(t3 - t2).count() /
                        kReads);
        std::printf("  (контрольная сумма %llu)\n",
                    static_cast<unsigned long long>(sink));
    }

    // --- поиск в модели -----------------------------------------------------

    std::printf("\nпоиск записи по номеру, 200000 обращений\n\n");
    std::printf("  %8s  %11s  %11s  %10s\n", "записей", "линейно нс",
                "словарь нс", "разница");
    std::uint64_t sink = 0;
    sink += SearchBench(100);
    sink += SearchBench(1000);
    sink += SearchBench(4000);
    sink += SearchBench(8000);
    std::printf("  (контрольная сумма %llu)\n",
                static_cast<unsigned long long>(sink));
    return 0;
}
