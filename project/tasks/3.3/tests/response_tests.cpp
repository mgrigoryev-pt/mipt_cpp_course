// Тесты плана реагирования. Занятие 3.3.
//
// ГЛАВНОЕ ПРО ЭТОТ ФАЙЛ: ЗДЕСЬ НЕТ СИМУЛЯТОРА
//
// Ни одного вызова os.h, ни одного сценария, ни одного файла на диске. Только
// модель, сработка и план. Это не экономия на тестах, а проверка дизайна:
// если план построить нельзя, не убив по дороге настоящий процесс, значит
// планирование и исполнение не разделены — и `--dry-run` в таком агенте будет
// работать «почти всегда».
//
// Тесты исполнителя выглядели бы иначе: там нужна граница, нужны коды отказа,
// нужен сценарий. Они есть в наборе решения; здесь проверяется то, что должно
// проверяться дёшево, — а дёшево должно проверяться всё, что не трогает мир.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "detection.h"
#include "doctest.h"
#include "entity_model.h"
#include "event.h"
#include "response.h"
#include "rules.h"

using nano_edr::Action;
using nano_edr::ActionKind;
using nano_edr::Detection;
using nano_edr::EntityModel;
using nano_edr::Event;
using nano_edr::EventParts;
using nano_edr::Field;
using nano_edr::ResponsePlan;
using nano_edr::ResponsePlanner;
using nano_edr::Severity;

namespace {

Event Make(const std::string& type, const std::string& pid,
           std::uint64_t ts_ms, const std::vector<Field>& fields) {
    EventParts parts;
    parts.ts = std::to_string(ts_ms);
    parts.type = type;
    parts.pid = pid;
    parts.fields = fields;
    return Event(std::move(parts));
}

Event Start(const std::string& pid, const std::string& ppid,
            const std::string& image, const std::string& cmdline,
            std::uint64_t ts_ms) {
    return Make("process_start", pid, ts_ms,
                {Field{"ppid", ppid}, Field{"image", image},
                 Field{"cmdline", cmdline}});
}

Detection MakeDetection(const std::string& rule, const std::string& pid,
                        const std::vector<std::string>& actions) {
    Detection detection;
    detection.rule = rule;
    detection.severity = Severity::kHigh;
    detection.pid = pid;
    detection.actions = actions;
    return detection;
}

// Сколько действий такого вида в плане.
std::size_t CountOf(const ResponsePlan& plan, ActionKind kind) {
    std::size_t n = 0;
    for (const Action& action : plan.actions()) {
        if (action.kind == kind) {
            ++n;
        }
    }
    return n;
}

const Action* FindTarget(const ResponsePlan& plan, const std::string& target) {
    for (const Action& action : plan.actions()) {
        if (action.target() == target) {
            return &action;
        }
    }
    return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
// Поддерево процессов
// ---------------------------------------------------------------------------

TEST_CASE("в план убийства попадает всё поддерево") {
    EntityModel model;
    model.Observe(Start("1042", "880", "C:\\Windows\\System32\\cmd.exe",
                        "cmd /c wscript a.js", 1000));
    model.Observe(Start("1101", "1042", "C:\\Windows\\System32\\wscript.exe",
                        "wscript.exe a.js", 2000));
    model.Observe(Start("1156", "1101",
                        "C:\\Windows\\System32\\powershell.exe",
                        "powershell -enc AAA", 3000));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "1042", {"kill_process"}));

    CHECK(plan.size() == 3);
    CHECK(plan.Has("1042"));
    CHECK(plan.Has("1101"));
    CHECK(plan.Has("1156"));
}

TEST_CASE("потомки убиваются раньше родителя") {
    EntityModel model;
    model.Observe(Start("1042", "880", "cmd.exe", "cmd", 1000));
    model.Observe(Start("1101", "1042", "wscript.exe", "wscript", 2000));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "1042", {"kill_process"}));

    REQUIRE(plan.size() == 2);
    // Живой родитель успеет породить нового потомка взамен убитого. На живой
    // машине это не теория, а то, как устроены сторожевые процессы.
    CHECK(plan.actions()[0].pid == "1101");
    CHECK(plan.actions()[1].pid == "1042");
}

TEST_CASE("время старта берётся из модели") {
    EntityModel model;
    model.Observe(Start("1042", "880", "cmd.exe", "cmd", 1730000001000));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "1042", {"kill_process"}));

    REQUIRE(plan.size() == 1);
    // Личность процесса — пара (номер, время старта). Без второй половины
    // убийство однажды достанется тому, кому этот номер перешёл.
    CHECK(plan.actions()[0].expected_start.ms == 1730000001000);
}

TEST_CASE("неизвестное время старта остаётся нулём") {
    EntityModel model;
    // Процесс известен только по номеру: событие про него было, а process_start
    // не было — он существовал до подписки.
    model.Observe(Make("file_write", "880", 1000,
                       {Field{"path", "C:\\Users\\max\\report.docx"}}));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "880", {"kill_process"}));

    REQUIRE(plan.size() == 1);
    // Ноль здесь значит «планировщик не знает», а не «не проверять».
    // Разбираться с этим — работа исполнителя: он спросит систему.
    CHECK(plan.actions()[0].expected_start.ms == 0);
}

TEST_CASE("неизвестный процесс плана не даёт") {
    EntityModel model;
    ResponsePlanner planner(&model, "C:\\quarantine");

    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "9999", {"kill_process"}));

    CHECK(plan.empty());
}

// ---------------------------------------------------------------------------
// Изоляция файлов
// ---------------------------------------------------------------------------

TEST_CASE("изолируется файл-источник, а не образ") {
    EntityModel model;
    model.Observe(Start("1042", "880", "C:\\Windows\\System32\\cmd.exe",
                        "cmd /c wscript", 1000));
    model.Observe(Make("file_create", "1042", 1400,
                       {Field{"path", "C:\\Users\\max\\Temp\\a.js"}}));
    model.Observe(Start("1101", "1042", "C:\\Windows\\System32\\wscript.exe",
                        "wscript.exe C:\\Users\\max\\Temp\\a.js", 2000));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "1101", {"quarantine_source"}));

    REQUIRE(plan.size() == 1);
    // Источник запуска — подброшенный скрипт. Образ у этого процесса
    // системный, и унести его в карантин значило бы сломать хост.
    CHECK(plan.actions()[0].path == "C:\\Users\\max\\Temp\\a.js");
    CHECK(plan.actions()[0].to == "C:\\quarantine\\1101-a.js");
}

TEST_CASE("системный файл не изолируется никогда") {
    EntityModel model;
    model.Observe(Start("1156", "880",
                        "C:\\Windows\\System32\\powershell.exe",
                        "powershell -w hidden -enc AAA", 1000));

    ResponsePlanner planner(&model, "C:\\quarantine");
    // Правило ошиблось: попросило изолировать образ, а образ у него системный.
    // Предохранитель в планировщике обязан не пустить — правил много,
    // а место исполнения одно.
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "1156", {"quarantine_image"}));

    CHECK(plan.empty());
}

TEST_CASE("образ из пользовательского каталога изолируется") {
    EntityModel model;
    model.Observe(Start("2210", "880",
                        "C:\\Users\\max\\AppData\\Local\\Temp\\svchost.exe",
                        "svchost.exe -e -q", 1000));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "2210", {"quarantine_image"}));

    REQUIRE(plan.size() == 1);
    CHECK(plan.actions()[0].path ==
          "C:\\Users\\max\\AppData\\Local\\Temp\\svchost.exe");
    CHECK(plan.actions()[0].to == "C:\\quarantine\\2210-svchost.exe");
}

TEST_CASE("без каталога карантина изоляции не будет") {
    EntityModel model;
    model.Observe(Start("2210", "880",
                        "C:\\Users\\max\\AppData\\Local\\Temp\\svchost.exe",
                        "svchost.exe", 1000));

    // Сервер не сказал, куда переносить. Перенести «куда-нибудь» хуже,
    // чем не переносить: файл исчезнет из известного места и не появится
    // в известном.
    ResponsePlanner planner(&model, "");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "2210", {"quarantine_image"}));

    CHECK(plan.empty());
}

TEST_CASE("номер процесса входит в имя в карантине") {
    EntityModel model;
    ResponsePlanner planner(&model, "C:\\quarantine");

    // Перенос на существующий путь заменяет его, как rename. Два файла
    // с одинаковым именем из разных каталогов затёрли бы друг друга,
    // и вторая улика исчезла бы молча.
    CHECK(planner.QuarantinePath("C:\\a\\payload.exe", "10") ==
          "C:\\quarantine\\10-payload.exe");
    CHECK(planner.QuarantinePath("C:\\b\\payload.exe", "20") ==
          "C:\\quarantine\\20-payload.exe");
}

// ---------------------------------------------------------------------------
// Удаление принесённого — и то, что удалять нельзя
// ---------------------------------------------------------------------------

TEST_CASE("переименованные файлы не удаляются") {
    EntityModel model;
    model.Observe(Start("2210", "880",
                        "C:\\Users\\max\\AppData\\Local\\Temp\\svchost.exe",
                        "svchost.exe", 1000));
    // Принёс с собой записку о выкупе...
    model.Observe(Make("file_create", "2210", 1100,
                       {Field{"path", "C:\\Users\\max\\Documents\\READ_ME.txt"}}));
    // ...и переименовал два документа жертвы.
    model.Observe(Make("file_move", "2210", 1200,
                       {Field{"from", "C:\\Users\\max\\Documents\\a.xlsx"},
                        Field{"to", "C:\\Users\\max\\Documents\\a.xlsx.locked"}}));
    model.Observe(Make("file_move", "2210", 1300,
                       {Field{"from", "C:\\Users\\max\\Documents\\b.docx"},
                        Field{"to", "C:\\Users\\max\\Documents\\b.docx.locked"}}));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "2210", {"delete_dropped"}));

    // Создателем всех трёх путей модель честно считает шифровальщика: этих
    // путей до него не было. Но два из них — документы жертвы под новыми
    // именами, и это единственная копия, которая у неё осталась.
    REQUIRE(plan.size() == 1);
    CHECK(plan.actions()[0].path == "C:\\Users\\max\\Documents\\READ_ME.txt");
    CHECK(!plan.Has("C:\\Users\\max\\Documents\\a.xlsx.locked"));
    CHECK(!plan.Has("C:\\Users\\max\\Documents\\b.docx.locked"));
}

TEST_CASE("удаляется принесённое всем поддеревом") {
    EntityModel model;
    model.Observe(Start("1042", "880", "cmd.exe", "cmd", 1000));
    model.Observe(Make("file_create", "1042", 1100,
                       {Field{"path", "C:\\Temp\\a.js"}}));
    model.Observe(Start("1101", "1042", "wscript.exe", "wscript", 2000));
    model.Observe(Make("file_create", "1101", 2100,
                       {Field{"path", "C:\\Temp\\stage2.bin"}}));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("test", "1042", {"delete_dropped"}));

    CHECK(plan.size() == 2);
    CHECK(plan.Has("C:\\Temp\\a.js"));
    CHECK(plan.Has("C:\\Temp\\stage2.bin"));
}

TEST_CASE("удаляется файл из самой сработки") {
    EntityModel model;
    model.Observe(Start("4410", "880", "powershell.exe", "powershell", 1000));

    Detection detection =
        MakeDetection("autostart", "4410", {"delete_evidence"});
    detection.evidence.push_back(std::make_unique<Event>(
        Make("file_create", "4410", 1600,
             {Field{"path", "C:\\Users\\max\\Startup\\Sync.lnk"}})));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan = planner.Plan(detection);

    // Ярлык в автозагрузке не назовёт ни модель, ни образ процесса:
    // powershell, положивший его, сам по себе не улика. Назвать его может
    // только то событие, на котором правило сработало.
    REQUIRE(plan.size() == 1);
    CHECK(plan.actions()[0].kind == ActionKind::kDeleteFile);
    CHECK(plan.actions()[0].path == "C:\\Users\\max\\Startup\\Sync.lnk");
}

// ---------------------------------------------------------------------------
// Дедупликация целей внутри плана
// ---------------------------------------------------------------------------

TEST_CASE("одна цель — одно действие") {
    ResponsePlan plan;

    Action kill;
    kill.kind = ActionKind::kKillProcess;
    kill.pid = "1042";
    CHECK(plan.Add(kill));

    Action again;
    again.kind = ActionKind::kKillProcess;
    again.pid = "1042";
    CHECK(!plan.Add(again));

    CHECK(plan.size() == 1);
}

TEST_CASE("изоляция побеждает удаление на одном файле") {
    EntityModel model;
    model.Observe(Start("1042", "880", "cmd.exe", "cmd", 1000));
    model.Observe(Make("file_create", "1042", 1100,
                       {Field{"path", "C:\\Temp\\a.js"}}));
    model.Observe(Start("1101", "1042", "wscript.exe",
                        "wscript.exe C:\\Temp\\a.js", 2000));

    ResponsePlanner planner(&model, "C:\\quarantine");
    // Файл-источник процесса 1101 создан процессом 1042, то есть попадает
    // и под изоляцию, и под удаление. Без дедупликации второе действие
    // отменило бы первое, и улика была бы уничтожена.
    const ResponsePlan plan = planner.Plan(MakeDetection(
        "test", "1101", {"quarantine_source", "delete_dropped"}));

    const Action* action = FindTarget(plan, "C:\\Temp\\a.js");
    REQUIRE(action != nullptr);
    CHECK(action->kind == ActionKind::kQuarantineFile);
    CHECK(CountOf(plan, ActionKind::kDeleteFile) == 0);
}

TEST_CASE("действие без цели в план не попадает") {
    ResponsePlan plan;
    Action empty;
    empty.kind = ActionKind::kDeleteFile;
    CHECK(!plan.Add(empty));
    CHECK(plan.empty());
}

// ---------------------------------------------------------------------------
// Причина у каждого действия
// ---------------------------------------------------------------------------

TEST_CASE("у действия есть причина") {
    EntityModel model;
    model.Observe(Start("1042", "880", "cmd.exe", "cmd", 1000));
    model.Observe(Start("1101", "1042", "wscript.exe", "wscript", 2000));

    ResponsePlanner planner(&model, "C:\\quarantine");
    const ResponsePlan plan =
        planner.Plan(MakeDetection("script_host_from_temp", "1042",
                                   {"kill_process"}));

    // Действие без причины невозможно ни объяснить, ни оспорить. В журнале
    // реагирования это первое, что спросят при разборе.
    const Action* parent = FindTarget(plan, "1042");
    const Action* child = FindTarget(plan, "1101");
    REQUIRE(parent != nullptr);
    REQUIRE(child != nullptr);
    CHECK(parent->reason == "script_host_from_temp");
    CHECK(child->reason.find("потомок 1042") != std::string::npos);
}
