// Тесты личности процесса и вытеснения. Занятие Ф1.
//
// Два предмета проверки, и они про разное.
//
// Первый — ключ записи. До Ф1 модель помнила процесс по номеру, и `process_start`
// с уже известным номером затирал прежнюю запись вместе с её цепочкой предков.
// Сценарий `pid_reuse` существует именно затем, чтобы это было видно; здесь
// то же самое проверяется точечно, потому что на сценарии дефект проявляется
// только при удачном совпадении времён, а в тесте — всегда.
//
// Второй — вытеснение. Проверять его надо не «работает ли», а «что именно
// выбрасывается»: вытеснение, которое выбрасывает не то, хуже отсутствующего,
// потому что выглядит как работающее.

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "doctest.h"
#include "entity_model.h"
#include "event.h"

using nano_edr::EntityModel;
using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::EvictionLimits;
using nano_edr::Field;
using nano_edr::FileRecord;
using nano_edr::ProcessKey;
using nano_edr::ProcessRecord;
using nano_edr::Timestamp;

namespace {

Event Make(const std::string& type, const std::string& pid,
           std::uint64_t ts_ms, const std::vector<Field>& fields) {
    EventParts parts;
    parts.ts = std::to_string(ts_ms);
    parts.type = type;
    parts.pid = pid;
    parts.fields = fields;
    return Event(parts);
}

Event Start(const std::string& pid, const std::string& ppid,
            const std::string& image, std::uint64_t ts_ms) {
    return Make("process_start", pid, ts_ms,
                {Field{"ppid", ppid}, Field{"image", image}});
}

Event Write(const std::string& pid, const std::string& path,
            std::uint64_t ts_ms) {
    return Make("file_write", pid, ts_ms,
                {Field{"path", path}, Field{"size", "1024"}});
}

// Пределы, при которых не вытесняется ничего: так проверяется всё, что
// про вытеснение не про него.
EvictionLimits NoEviction() {
    EvictionLimits limits;
    limits.max_processes = 100000;
    limits.max_files = 100000;
    limits.max_idle_ms = 0;
    return limits;
}

}  // namespace

// ---------------------------------------------------------------------------
// Ключ и его хеш
// ---------------------------------------------------------------------------

TEST_CASE("хеш ключа не бросает") {
    // Требование стандарта, а не пожелание: unordered_map зовёт хеш в местах,
    // где раскрутка стека оставила бы таблицу в половинчатом состоянии.
    // Проверяется компилятором, а не запуском, — потому и static_assert.
    static_assert(std::is_nothrow_invocable_v<std::hash<ProcessKey>,
                                             const ProcessKey&>);
    CHECK(true);
}

TEST_CASE("ключ различает воплощения одного номера") {
    const ProcessKey first{"1042", Timestamp{1000}};
    const ProcessKey second{"1042", Timestamp{2000}};
    const ProcessKey same{"1042", Timestamp{1000}};

    CHECK(first == same);
    CHECK_FALSE(first == second);

    const std::hash<ProcessKey> hash;
    // Равные ключи обязаны давать равный хеш — это контракт. Обратное
    // не обязано выполняться, и проверять «разные ключи дают разный хеш»
    // было бы проверкой отсутствия коллизий, то есть неверной проверкой.
    CHECK(hash(first) == hash(same));
}

// ---------------------------------------------------------------------------
// Переиспользование номера
// ---------------------------------------------------------------------------

TEST_CASE("прежнее воплощение остаётся в модели") {
    EntityModel model;
    model.SetLimits(NoEviction());

    model.Observe(Start("1042", "880", "wscript.exe", 1000));
    model.Observe(Make("process_end", "1042", 1800, {Field{"exit", "0"}}));
    model.Observe(Start("1042", "880", "notepad.exe", 2000));

    // Две записи, а не одна: номер один, процессов два.
    CHECK(model.process_count() == 2);

    const ProcessKey first_key{"1042", Timestamp{1000}};
    const ProcessKey second_key{"1042", Timestamp{2000}};

    const ProcessRecord* old_one = model.FindProcess(first_key);
    REQUIRE(old_one != nullptr);
    CHECK(old_one->image == "wscript.exe");
    CHECK_FALSE(old_one->alive);

    const ProcessRecord* new_one = model.FindProcess(second_key);
    REQUIRE(new_one != nullptr);
    CHECK(new_one->image == "notepad.exe");
    CHECK(new_one->alive);
}

TEST_CASE("поиск по номеру отдаёт нынешнего носителя") {
    EntityModel model;
    model.SetLimits(NoEviction());

    model.Observe(Start("1042", "880", "wscript.exe", 1000));
    model.Observe(Start("1042", "880", "notepad.exe", 2000));

    // Событие приносит номер, а не пару, и относится оно к тому, кто держит
    // номер сейчас. Иначе агент реагировал бы на прошлое.
    const ProcessRecord* current = model.FindProcess("1042");
    REQUIRE(current != nullptr);
    CHECK(current->image == "notepad.exe");
    CHECK(current->start.ms == 2000);

    const ProcessKey expected{"1042", Timestamp{2000}};
    CHECK(model.CurrentKey("1042") == expected);
}

TEST_CASE("цепочка предков не подменяется переиспользованным номером") {
    // Тот самый дефект, который занятие починило. 1180 запущен процессом
    // 1042 в его первом воплощении; потом номер 1042 достаётся блокноту.
    // Модель, помнящая родителя номером, скажет, что 1180 запущен блокнотом.
    EntityModel model;
    model.SetLimits(NoEviction());

    model.Observe(Start("1042", "880", "wscript.exe", 1000));
    model.Observe(Start("1180", "1042", "powershell.exe", 1200));
    model.Observe(Make("process_end", "1042", 1800, {Field{"exit", "0"}}));
    model.Observe(Start("1042", "880", "notepad.exe", 2000));

    const std::vector<std::string> chain = model.GetAncestorChain("1180");
    REQUIRE(chain.size() == 2);
    CHECK(chain[0] == "1180");
    CHECK(chain[1] == "1042");

    // А вот здесь видна разница: имя образа берётся у того воплощения,
    // которое действительно было родителем.
    CHECK(model.DescribeChain("1180") == "1042(wscript.exe)>1180(powershell.exe)");
}

TEST_CASE("поддерево не забирает чужих детей с тем же номером") {
    EntityModel model;
    model.SetLimits(NoEviction());

    model.Observe(Start("1042", "880", "wscript.exe", 1000));
    model.Observe(Start("1180", "1042", "powershell.exe", 1200));
    model.Observe(Make("process_end", "1042", 1800, {Field{"exit", "0"}}));
    model.Observe(Start("1042", "880", "notepad.exe", 2000));
    model.Observe(Start("1300", "1042", "cmd.exe", 2200));

    // У первого воплощения ребёнок 1180, у второго — 1300. Поддерево
    // спрашивается по номеру, то есть у нынешнего носителя.
    const std::vector<std::string> tree = model.GetProcessTree("1042");
    REQUIRE(tree.size() == 2);
    CHECK(tree[0] == "1042");
    CHECK(tree[1] == "1300");
}

// ---------------------------------------------------------------------------
// Порядок, которого у словаря нет
// ---------------------------------------------------------------------------

TEST_CASE("поддерево упорядочено по времени старта") {
    // Порядок обхода хеш-таблицы стандарт не определяет, а порядок действий
    // в плане реагирования сравнивается с эталоном побайтово. Значит порядок
    // здесь обязан быть задан, а не унаследован от реализации.
    EntityModel model;
    model.SetLimits(NoEviction());

    model.Observe(Start("500", "4", "svchost.exe", 900));
    model.Observe(Start("9000", "500", "third.exe", 3000));
    model.Observe(Start("120", "500", "first.exe", 1000));
    model.Observe(Start("7000", "500", "second.exe", 2000));

    const std::vector<std::string> tree = model.GetProcessTree("500");
    REQUIRE(tree.size() == 4);
    CHECK(tree[0] == "500");
    CHECK(tree[1] == "120");
    CHECK(tree[2] == "7000");
    CHECK(tree[3] == "9000");
}

TEST_CASE("файлы процесса упорядочены по пути") {
    EntityModel model;
    model.SetLimits(NoEviction());

    model.Observe(Make("file_create", "1042", 1000,
                       {Field{"path", "C:\\Temp\\z.js"}}));
    model.Observe(Make("file_create", "1042", 1100,
                       {Field{"path", "C:\\Temp\\a.js"}}));
    model.Observe(Make("file_create", "1042", 1200,
                       {Field{"path", "C:\\Temp\\m.js"}}));

    const std::vector<std::string> files = model.GetFilesCreatedBy("1042");
    REQUIRE(files.size() == 3);
    CHECK(files[0] == "C:\\Temp\\a.js");
    CHECK(files[1] == "C:\\Temp\\m.js");
    CHECK(files[2] == "C:\\Temp\\z.js");
}

TEST_CASE("при равном времени записи связка выбирает меньший путь") {
    // Ничья по времени. До Ф1 её разрешал порядок появления в векторе,
    // то есть порядок событий; теперь порядка обхода нет вообще, и правило
    // приходится назвать.
    EntityModel model;
    model.SetLimits(NoEviction());

    model.Observe(Write("1042", "C:\\Temp\\b.js", 1000));
    model.Observe(Write("1042", "C:\\Temp\\a.js", 1000));
    model.Observe(Make("process_start", "1101", 1500,
                       {Field{"ppid", "1042"},
                        Field{"image", "C:\\Windows\\wscript.exe"},
                        Field{"cmdline",
                              "wscript.exe C:\\Temp\\a.js C:\\Temp\\b.js"}}));

    const FileRecord* source = model.FindSpawnSource("1101");
    REQUIRE(source != nullptr);
    CHECK(source->path == "C:\\Temp\\a.js");
}

// ---------------------------------------------------------------------------
// Вытеснение
// ---------------------------------------------------------------------------

TEST_CASE("без вытеснения модель не теряет ничего") {
    EntityModel model;
    model.SetLimits(NoEviction());

    model.Observe(Start("1042", "880", "cmd.exe", 1000));
    model.Observe(Write("1042", "C:\\Temp\\a.js", 2000));
    model.Observe(Make("process_start", "1101", 9000000,
                       {Field{"ppid", "1042"}, Field{"image", "x.exe"}}));
    model.Evict();

    CHECK(model.eviction().total() == 0);
    CHECK(model.FindProcess("1042") != nullptr);
    CHECK(model.FindFile("C:\\Temp\\a.js") != nullptr);
}

TEST_CASE("вытеснение по возрасту считает от последнего события") {
    EntityModel model;
    EvictionLimits limits = NoEviction();
    limits.max_idle_ms = 5000;
    model.SetLimits(limits);

    model.Observe(Start("1042", "880", "cmd.exe", 1000));
    model.Observe(Write("1042", "C:\\Temp\\old.js", 1000));
    model.Observe(Start("1101", "880", "wscript.exe", 20000));
    model.Observe(Write("1101", "C:\\Temp\\fresh.js", 20000));

    CHECK(model.last_seen().ms == 20000);
    model.Evict();

    // 1042 не упоминался пятнадцать секунд, 1101 упомянут только что.
    CHECK(model.FindProcess("1042") == nullptr);
    CHECK(model.FindProcess("1101") != nullptr);
    CHECK(model.FindFile("C:\\Temp\\old.js") == nullptr);
    CHECK(model.FindFile("C:\\Temp\\fresh.js") != nullptr);

    CHECK(model.eviction().processes_by_age == 1);
    CHECK(model.eviction().files_by_age == 1);
    CHECK(model.eviction().processes_by_size == 0);
}

TEST_CASE("любое событие обновляет возраст записи") {
    // Вытеснение идёт по последнему упоминанию, а не по времени старта.
    // Процесс, запущенный час назад и работающий сейчас, свежий.
    EntityModel model;
    EvictionLimits limits = NoEviction();
    limits.max_idle_ms = 5000;
    model.SetLimits(limits);

    model.Observe(Start("1042", "880", "cmd.exe", 1000));
    model.Observe(Write("1042", "C:\\Temp\\a.js", 19000));
    model.Observe(Start("1101", "880", "wscript.exe", 20000));
    model.Evict();

    CHECK(model.FindProcess("1042") != nullptr);
    CHECK(model.eviction().processes_by_age == 0);
}

TEST_CASE("вытеснение по объёму выбрасывает самые старые") {
    EntityModel model;
    EvictionLimits limits = NoEviction();
    limits.max_processes = 2;
    model.SetLimits(limits);

    model.Observe(Start("100", "4", "a.exe", 1000));
    model.Observe(Start("200", "4", "b.exe", 2000));
    model.Observe(Start("300", "4", "c.exe", 3000));
    model.Observe(Start("400", "4", "d.exe", 4000));
    model.Evict();

    CHECK(model.process_count() == 2);
    CHECK(model.FindProcess("100") == nullptr);
    CHECK(model.FindProcess("200") == nullptr);
    CHECK(model.FindProcess("300") != nullptr);
    CHECK(model.FindProcess("400") != nullptr);
    CHECK(model.eviction().processes_by_size == 2);
}

TEST_CASE("потолок соблюдается и при выключенном возрасте") {
    // Возраст и объём решают разные задачи, и вторая обязана работать сама.
    // Лекция 13 про это прямым текстом: гарантию даёт только жёсткий потолок.
    EntityModel model;
    EvictionLimits limits;
    limits.max_files = 3;
    limits.max_idle_ms = 0;
    model.SetLimits(limits);

    for (std::uint64_t i = 0; i < 10; ++i) {
        model.Observe(Write("1042", "C:\\Temp\\f" + std::to_string(i) + ".dat",
                            1000 + i));
    }
    model.Evict();

    CHECK(model.file_count() == 3);
    CHECK(model.FindFile("C:\\Temp\\f9.dat") != nullptr);
    CHECK(model.FindFile("C:\\Temp\\f0.dat") == nullptr);
}

TEST_CASE("вытесненный процесс перестаёт находиться и по номеру") {
    // Ловушка второго словаря: индекс «номер -> ключ» живёт ровно столько,
    // сколько то, на что он указывает. Индекс, который переживает запись,
    // — это и висячая ссылка, и утечка одновременно.
    EntityModel model;
    EvictionLimits limits = NoEviction();
    limits.max_idle_ms = 1000;
    model.SetLimits(limits);

    model.Observe(Start("1042", "880", "cmd.exe", 1000));
    model.Observe(Start("1101", "880", "wscript.exe", 50000));
    model.Evict();

    CHECK(model.FindProcess("1042") == nullptr);
    CHECK(model.CurrentKey("1042").pid.empty());
    CHECK(model.GetAncestorChain("1042").empty());
    CHECK(model.GetProcessTree("1042").empty());
    CHECK(model.DescribeChain("1042").empty());
}

TEST_CASE("цепочка обрывается на вытесненном предке") {
    // Настоящая цена вытеснения, и её надо знать: агент с ограниченной
    // памятью отвечает на вопросы про связи хуже, чем агент без предела.
    // Выбор здесь не между «точно» и «неточно», а между «неточно»
    // и «съел память хоста».
    EntityModel model;
    EvictionLimits limits = NoEviction();
    limits.max_processes = 1;
    model.SetLimits(limits);

    model.Observe(Start("1042", "880", "cmd.exe", 1000));
    model.Observe(Start("1101", "1042", "wscript.exe", 2000));
    model.Evict();

    const std::vector<std::string> chain = model.GetAncestorChain("1101");
    REQUIRE(chain.size() == 1);
    CHECK(chain[0] == "1101");
}
