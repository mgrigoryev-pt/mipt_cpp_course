// Тесты модели сущностей и связки child_of_prev. Занятие 3.2.
//
// Модель — это память агента о связях, а не о событиях. Проверяется именно
// это: не «запомнила ли она процесс», а «отвечает ли она на вопросы,
// на которые окно событий ответить не может».

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "conditions.h"
#include "doctest.h"
#include "entity_model.h"
#include "event.h"
#include "rule.h"
#include "rules.h"

using nano_edr::EntityModel;
using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::EventType;
using nano_edr::EventTypeIs;
using nano_edr::Field;
using nano_edr::FieldEndsWith;
using nano_edr::FileRecord;
using nano_edr::ProcessRecord;
using nano_edr::SequenceRule;
using nano_edr::Severity;

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
            const std::string& image, const std::string& cmdline,
            std::uint64_t ts_ms) {
    return Make("process_start", pid, ts_ms,
                {Field{"ppid", ppid}, Field{"image", image},
                 Field{"cmdline", cmdline}});
}

Event Write(const std::string& pid, const std::string& path,
            std::uint64_t ts_ms) {
    return Make("file_write", pid, ts_ms,
                {Field{"path", path}, Field{"size", "1024"}});
}

}  // namespace

// ---------------------------------------------------------------------------
// Что модель запоминает
// ---------------------------------------------------------------------------

TEST_CASE("пустая модель ничего не знает") {
    const EntityModel model;

    CHECK(model.process_count() == 0);
    CHECK(model.file_count() == 0);
    CHECK(model.FindProcess("1042") == nullptr);
    CHECK(model.FindFile("C:\\a.txt") == nullptr);
}

TEST_CASE("процесс из process_start") {
    EntityModel model;
    model.Observe(Start("1042", "880", "C:\\Windows\\System32\\cmd.exe",
                        "cmd /c wscript a.js", 1000));

    const ProcessRecord* process = model.FindProcess("1042");
    REQUIRE(process != nullptr);
    CHECK(process->ppid == "880");
    CHECK(process->image == "C:\\Windows\\System32\\cmd.exe");
    CHECK(process->cmdline == "cmd /c wscript a.js");
    CHECK(process->start.ms == 1000);
    CHECK(process->alive);
}

TEST_CASE("процесс становится известен по любому событию") {
    // Процессы, существовавшие до подписки, в поток через process_start
    // не попадают. Знать про них хоть номер всё равно нужно: на них
    // обрывается цепочка предков, и обрыв должен быть виден.
    EntityModel model;
    model.Observe(Write("880", "C:\\Users\\max\\report.docx", 1000));

    const ProcessRecord* process = model.FindProcess("880");
    REQUIRE(process != nullptr);
    CHECK(process->pid == "880");
    CHECK(process->image.empty());
    CHECK(process->ppid.empty());
}

TEST_CASE("process_end помечает процесс завершённым") {
    EntityModel model;
    model.Observe(Start("1042", "880", "cmd.exe", "cmd", 1000));
    model.Observe(Make("process_end", "1042", 2000, {Field{"exit", "0"}}));

    const ProcessRecord* process = model.FindProcess("1042");
    REQUIRE(process != nullptr);
    CHECK_FALSE(process->alive);
    // Запись остаётся: реагирование на занятии 3.3 обязано отличать
    // «процесса не было» от «процесс уже завершился».
    //
    // И одна, а не две: 880 упомянут только полем ppid, а модель запоминает
    // процессы по pid события. Родитель станет известен, когда сделает
    // что-нибудь сам.
    CHECK(model.process_count() == 1);
}

TEST_CASE("файл из file_write") {
    EntityModel model;
    model.Observe(Write("1042", "C:\\Temp\\a.js", 1000));

    const FileRecord* file = model.FindFile("C:\\Temp\\a.js");
    REQUIRE(file != nullptr);
    CHECK(file->writer_pid == "1042");
    CHECK(file->last_write.ms == 1000);
    CHECK_FALSE(file->deleted);
}

TEST_CASE("переименование переносит файл") {
    EntityModel model;
    model.Observe(Write("2210", "C:\\Users\\max\\report.docx", 1000));
    model.Observe(Make("file_move", "2210", 1100,
                       {Field{"from", "C:\\Users\\max\\report.docx"},
                        Field{"to", "C:\\Users\\max\\report.docx.locked"}}));

    const FileRecord* from = model.FindFile("C:\\Users\\max\\report.docx");
    REQUIRE(from != nullptr);
    CHECK(from->deleted);

    const FileRecord* to = model.FindFile("C:\\Users\\max\\report.docx.locked");
    REQUIRE(to != nullptr);
    CHECK(to->creator_pid == "2210");
}

// ---------------------------------------------------------------------------
// Цепочка предков
// ---------------------------------------------------------------------------

TEST_CASE("цепочка предков от процесса к корню") {
    EntityModel model;
    model.Observe(Write("880", "C:\\Users\\max\\report.docx", 900));
    model.Observe(Start("1042", "880", "C:\\Windows\\System32\\cmd.exe",
                        "cmd /c wscript", 1000));
    model.Observe(Start("1101", "1042", "C:\\Windows\\System32\\wscript.exe",
                        "wscript a.js", 2000));

    const std::vector<std::string> chain = model.GetAncestorChain("1101");

    REQUIRE(chain.size() == 3);
    CHECK(chain[0] == "1101");
    CHECK(chain[1] == "1042");
    CHECK(chain[2] == "880");
}

TEST_CASE("цепочка обрывается там, где кончается знание") {
    EntityModel model;
    model.Observe(Start("1101", "1042", "wscript.exe", "wscript", 2000));

    // Про 1042 не было ни одного события — цепочка кончается на 1101.
    const std::vector<std::string> chain = model.GetAncestorChain("1101");

    REQUIRE(chain.size() == 1);
    CHECK(chain[0] == "1101");
}

TEST_CASE("цепочка неизвестного процесса пуста") {
    const EntityModel model;

    CHECK(model.GetAncestorChain("1042").empty());
    CHECK(model.DescribeChain("1042").empty());
}

TEST_CASE("цепочка строкой читается от старшего") {
    EntityModel model;
    model.Observe(Write("880", "C:\\Users\\max\\report.docx", 900));
    model.Observe(Start("1042", "880", "C:\\Windows\\System32\\cmd.exe",
                        "cmd", 1000));

    // Имя образа, а не полный путь: полный путь в строку сработки
    // не влезает. У 880 образ неизвестен — печатается один номер.
    CHECK(model.DescribeChain("1042") == "880>1042(cmd.exe)");
}

TEST_CASE("цикл в цепочке не зацикливает обход") {
    // Переиспользование pid может дать `a -> b -> a`: ключ записи — pid,
    // и старый процесс с тем же номером затирается. В реальной системе
    // такого предка не бывает, в модели — бывает.
    EntityModel model;
    model.Observe(Start("1042", "1101", "cmd.exe", "cmd", 1000));
    model.Observe(Start("1101", "1042", "wscript.exe", "wscript", 2000));

    const std::vector<std::string> chain = model.GetAncestorChain("1101");

    CHECK(chain.size() == 2);
}

// ---------------------------------------------------------------------------
// Поддерево и файлы
// ---------------------------------------------------------------------------

TEST_CASE("поддерево процессов") {
    // То, что придётся убивать на занятии 3.3: не один процесс, а всё,
    // что он породил.
    EntityModel model;
    model.Observe(Start("1042", "880", "cmd.exe", "cmd", 1000));
    model.Observe(Start("1101", "1042", "wscript.exe", "wscript", 2000));
    model.Observe(Start("1156", "1101", "powershell.exe", "powershell", 3000));
    model.Observe(Start("1300", "880", "chrome.exe", "chrome", 3500));

    const std::vector<std::string> tree = model.GetProcessTree("1042");

    REQUIRE(tree.size() == 3);
    CHECK(tree[0] == "1042");
    CHECK(tree[1] == "1101");
    CHECK(tree[2] == "1156");
}

TEST_CASE("поддерево неизвестного процесса пусто") {
    const EntityModel model;

    CHECK(model.GetProcessTree("1042").empty());
}

TEST_CASE("файлы, созданные процессом") {
    EntityModel model;
    model.Observe(Make("file_create", "1042", 1000,
                       {Field{"path", "C:\\Temp\\a.js"}}));
    model.Observe(Make("file_create", "1042", 1100,
                       {Field{"path", "C:\\Temp\\b.js"}}));
    model.Observe(Make("file_create", "1300", 1200,
                       {Field{"path", "C:\\Temp\\c.js"}}));
    // Запись в чужой файл создателя не меняет.
    model.Observe(Write("1042", "C:\\Temp\\c.js", 1300));

    const std::vector<std::string> files = model.GetFilesCreatedBy("1042");

    REQUIRE(files.size() == 2);
    CHECK(files[0] == "C:\\Temp\\a.js");
    CHECK(files[1] == "C:\\Temp\\b.js");
}

// ---------------------------------------------------------------------------
// FindSpawnSource — связка path_in_cmdline
// ---------------------------------------------------------------------------

TEST_CASE("процесс запустился из сброшенного файла") {
    EntityModel model;
    model.Observe(Make("file_create", "1042", 1000,
                       {Field{"path", "C:\\Temp\\a.js"}}));
    model.Observe(Write("1042", "C:\\Temp\\a.js", 1100));
    model.Observe(Start("1101", "1042", "C:\\Windows\\System32\\wscript.exe",
                        "wscript.exe C:\\Temp\\a.js", 2000));

    const FileRecord* source = model.FindSpawnSource("1101");

    REQUIRE(source != nullptr);
    CHECK(source->path == "C:\\Temp\\a.js");
    CHECK(source->writer_pid == "1042");
}

TEST_CASE("имя без пути связку не даёт") {
    // Ловушка сценария clean_build. Сборка пишет nano-edr.exe и тут же его
    // запускает — но в командной строке стоит имя без пути, и связка
    // path_in_cmdline не находит ничего. Признак не в том, что файл записан,
    // а в том, что путь к нему передан.
    EntityModel model;
    model.Observe(Write("5990", "C:\\work\\build\\nano-edr.exe", 1000));
    model.Observe(Start("6100", "2500", "C:\\work\\build\\nano-edr.exe",
                        "nano-edr.exe --selftest", 2000));

    CHECK(model.FindSpawnSource("6100") == nullptr);
}

TEST_CASE("неизвестный файл в командной строке связку не даёт") {
    // clean_office: корпоративный скрипт лежит в C:\corp\tools, и модель
    // про него не знает — событий по нему не было.
    EntityModel model;
    model.Observe(Start("1850", "880", "C:\\Windows\\System32\\wscript.exe",
                        "wscript.exe C:\\corp\\tools\\map_drives.vbs", 1000));

    CHECK(model.FindSpawnSource("1850") == nullptr);
}

TEST_CASE("процесс без командной строки связку не даёт") {
    EntityModel model;
    model.Observe(Write("1042", "C:\\Temp\\a.js", 1000));
    model.Observe(Make("process_start", "1101", 2000,
                       {Field{"ppid", "1042"}}));

    CHECK(model.FindSpawnSource("1101") == nullptr);
}

// ---------------------------------------------------------------------------
// SequenceRule: связка child_of_prev
// ---------------------------------------------------------------------------

namespace {

// Правило «оболочка запустила скриптовый хост». Два шага, разные процессы,
// связь — родство.
std::unique_ptr<SequenceRule> MakeShellSpawnsScriptHost(
        const EntityModel* model) {
    auto rule = std::make_unique<SequenceRule>("shell_spawns_script_host",
                                               Severity::kHigh, 30000);
    auto first = std::make_unique<nano_edr::AllOf>();
    first->Add(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    first->Add(std::make_unique<FieldEndsWith>("image", "cmd.exe"));
    rule->SetFirst(std::move(first));

    auto second = std::make_unique<nano_edr::AllOf>();
    second->Add(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    second->Add(std::make_unique<FieldEndsWith>("image", "wscript.exe"));
    rule->SetSecond(std::move(second));

    rule->set_child_of_prev(true);
    rule->SetModel(model);
    return rule;
}

}  // namespace

TEST_CASE("child_of_prev срабатывает на настоящем родстве") {
    EntityModel model;
    std::unique_ptr<SequenceRule> rule = MakeShellSpawnsScriptHost(&model);

    const Event shell = Start("1042", "880", "C:\\Windows\\System32\\cmd.exe",
                              "cmd /c wscript", 1000);
    model.Observe(shell);
    CHECK_FALSE(rule->Check(shell));

    const Event host = Start("1101", "1042",
                             "C:\\Windows\\System32\\wscript.exe",
                             "wscript a.js", 2000);
    // Порядок существенный: модель обязана узнать про 1101 ДО проверки,
    // иначе связке неоткуда взять его родителя.
    model.Observe(host);
    CHECK(rule->Check(host));
    CHECK(rule->hits() == 1);
}

TEST_CASE("child_of_prev молчит на общем родителе") {
    // clean_office дословно: пользователь запустил cmd.exe, потом отдельно
    // корпоративный скрипт. Оба шага есть, окно выдержано, родство — нет.
    EntityModel model;
    std::unique_ptr<SequenceRule> rule = MakeShellSpawnsScriptHost(&model);

    const Event shell = Start("1820", "880", "C:\\Windows\\System32\\cmd.exe",
                              "cmd /c dir", 1000);
    model.Observe(shell);
    rule->Check(shell);

    const Event host = Start("1850", "880",
                             "C:\\Windows\\System32\\wscript.exe",
                             "wscript.exe map_drives.vbs", 2000);
    model.Observe(host);

    CHECK_FALSE(rule->Check(host));
    CHECK(rule->hits() == 0);
}

TEST_CASE("child_of_prev без модели молчит") {
    // Правило, которое не может проверить свою связь, обязано молчать,
    // а не срабатывать наугад.
    std::unique_ptr<SequenceRule> rule = MakeShellSpawnsScriptHost(nullptr);

    rule->Check(Start("1042", "880", "cmd.exe", "cmd", 1000));

    CHECK_FALSE(rule->Check(Start("1101", "1042", "wscript.exe", "w", 2000)));
    CHECK(rule->hits() == 0);
}

TEST_CASE("child_of_prev не срабатывает на одном и том же процессе") {
    // Связка «его потомок» отменяет same_pid, и это не косметика: событие
    // в том же процессе потомком не является.
    EntityModel model;
    auto rule = std::make_unique<SequenceRule>("self", Severity::kLow, 30000);
    rule->SetFirst(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kProcessStart}));
    rule->SetSecond(std::make_unique<EventTypeIs>(
        std::vector<EventType>{EventType::kFileWrite}));
    rule->set_child_of_prev(true);
    rule->SetModel(&model);

    const Event start = Start("1042", "880", "cmd.exe", "cmd", 1000);
    model.Observe(start);
    rule->Check(start);

    const Event write = Write("1042", "C:\\Temp\\a.js", 1100);
    model.Observe(write);

    CHECK_FALSE(rule->Check(write));
}
