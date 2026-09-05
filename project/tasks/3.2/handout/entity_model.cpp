// Модель сущностей. Занятие 3.2. См. entity_model.h.
//
// ЭТОТ ФАЙЛ ВЫДАН ГОТОВЫМ. Скопируйте его в `src/` — и дальше он ваш:
// на занятии 3.3 вы правите его вслед за заголовком, на 4.1 переписываете
// хранилище по замеру.
//
// Почему выдан. Два вектора, линейный поиск и обходы в лоб — ни одной
// хитрости, и учить тут нечему: занятие 3.2 про стирание типа, а не про
// перебор векторов. Ценно здесь другое — у агента впервые появилась память
// о связях, а не только о последних событиях, — и видно это по тому, как
// модель подключается, а не по тому, как она написана.
//
// Прочитать всё равно стоит, и в первую очередь `Observe`: планировщик
// реагирования на 3.3 не читает ничего, кроме этой модели, а на 4.1 вы её
// переписываете.

#include "entity_model.h"

#include <cstddef>
#include <format>
#include <string>
#include <vector>

#include "fields.h"

namespace nano_edr {
namespace {

// Значение поля или пустая строка. Отсутствие поля здесь не отказ: журнал
// пишет чужой код, и `process_start` без cmdline — нормальное событие.
std::string FieldOrEmpty(const Event& event, const std::string& key) {
    const std::string* value = FindField(event, key);
    return value != nullptr ? *value : std::string();
}

// Предел глубины обхода. Нужен не от кривых данных, а от переиспользования
// pid: `1042 -> 880 -> 1042` — цикл, которого в реальной системе быть
// не может, а в модели с ключом-pid может. Явная проверка на повтор ниже
// ловит его точнее, предел — последняя страховка.
constexpr std::size_t kMaxDepth = 64;

// Насколько давно мог быть записан файл, из которого запустился процесс.
//
// Окно про смысл, а не про скорость: дроппер запускает сброшенный файл
// сразу — секунды, не часы. Файл, записанный вчера и упомянутый в командной
// строке сегодня, это не сброс, а обычная работа с документом.
//
// Ускорения от него ждать не стоит, и это проверено: на нагрузочном сценарии
// окно не отсекает почти ничего, потому что путей в модели немного и все они
// перезаписываются постоянно. Дорого в модели совсем другое — линейный поиск
// в Observe, см. заметку у MutableProcess.
constexpr uint64_t kSpawnSourceWindowMs = 60000;

// Две мелочи ниже в проекте уже есть — в вашем `text.h`. Здесь они
// повторены намеренно: этот файл выдан готовым и обязан собираться у любого,
// а как называются функции в `text.h`, курс не фиксирует — его пишете вы.
// Десять строк ради независимости выдачи. После копирования можете заменить
// их своими, файл ваш.

char LowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Упомянут ли путь в командной строке. Без учёта регистра: в командной строке
// путь пишут как придётся, а файловая система Windows регистр не различает.
bool PathMentioned(const std::string& cmdline, const std::string& path) {
    if (path.empty() || path.size() > cmdline.size()) {
        return false;
    }
    for (std::size_t at = 0; at + path.size() <= cmdline.size(); ++at) {
        std::size_t i = 0;
        while (i < path.size() &&
               LowerAscii(cmdline[at + i]) == LowerAscii(path[i])) {
            ++i;
        }
        if (i == path.size()) {
            return true;
        }
    }
    return false;
}

// Имя файла из пути: всё после последнего разделителя. Разделителями
// считаются оба, и обратный, и прямой: журнал пишет чужой код, и полагаться
// на один из них не стоит.
std::string FileName(const std::string& path) {
    const std::size_t at = path.find_last_of("\\/");
    return at == std::string::npos ? path : path.substr(at + 1);
}

bool Contains(const std::vector<std::string>& list, const std::string& value) {
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i] == value) {
            return true;
        }
    }
    return false;
}

}  // namespace

// --- запись и вытеснение ---------------------------------------------------

ProcessRecord* EntityModel::MutableProcess(const std::string& pid) {
    for (std::size_t i = 0; i < processes_.size(); ++i) {
        if (processes_[i].pid == pid) {
            return &processes_[i];
        }
    }

    // ЧТО ЗДЕСЬ СТОИТ ДОРОГО, И НАСКОЛЬКО
    //
    // Цикл выше. Он выполняется на КАЖДОМ событии, и длина его — число
    // известных процессов. То же у MutableFile с файлами. Пока их десятки,
    // это ничто; на потоке это главная статья расхода агента.
    //
    // Замерено на нагрузочном сценарии: два миллиона событий, к концу
    // в модели 3868 процессов и 7975 файлов.
    //
    //   без модели                 22,7 с процессорного времени
    //   с моделью                  85,1 с
    //
    // То есть модель стоит вчетверо больше всей остальной работы агента,
    // и стоит она вот этим циклом. Отдельный замер обращения: линейный поиск
    // по восьми тысячам путей — 8,5 мкс, словарь — 56 нс, разница
    // в сто пятьдесят раз.
    //
    // Чинится это на занятии 4.1, и именно этот замер там и есть постановка
    // задачи: не «словарь быстрее, всем известно», а «модель съедает
    // восемьдесят процентов времени, вот на чём».
    if (processes_.size() >= kMaxProcesses) {
        // Вытесняется четверть самых старых — одним проходом, а не по одной
        // записи на вставку: сдвиг вектора стоит линейно, и делать его
        // на каждом новом процессе значит платить квадратично.
        //
        // Вытеснение по возрасту записи, а не по времени последнего обращения,
        // и предел один на всех — это грубо. Настоящее вытеснение появится
        // на занятии 4.1; здесь важно, что предел есть.
        const std::size_t drop = kMaxProcesses / 4;
        processes_.erase(processes_.begin(),
                         processes_.begin() + static_cast<std::ptrdiff_t>(drop));
    }

    ProcessRecord record;
    record.pid = pid;
    processes_.push_back(record);
    return &processes_.back();
}

FileRecord* EntityModel::MutableFile(const std::string& path) {
    for (std::size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].path == path) {
            return &files_[i];
        }
    }

    if (files_.size() >= kMaxFiles) {
        const std::size_t drop = kMaxFiles / 4;
        files_.erase(files_.begin(),
                     files_.begin() + static_cast<std::ptrdiff_t>(drop));
    }

    FileRecord record;
    record.path = path;
    files_.push_back(record);
    return &files_.back();
}

void EntityModel::NoteFileWrite(const Event& event, const std::string& path,
                                bool created) {
    if (path.empty()) {
        return;
    }
    FileRecord* file = MutableFile(path);
    file->writer_pid = event.pid();
    file->last_write = event.ts();
    file->deleted = false;
    if (created || file->creator_pid.empty()) {
        // Создатель запоминается один раз. Если файл существовал до подписки
        // и первым событием была запись — создателем считается тот, кто
        // записал первым, и это ровно то, что модель может знать.
        file->creator_pid = event.pid();
    }
}

void EntityModel::Observe(const Event& event) {
    // Любое событие с pid делает процесс известным — хотя бы по номеру.
    // Именно поэтому цепочка предков в фишинговом сценарии доходит до 880:
    // сам winword.exe не запускался при агенте, но записал файл.
    if (!event.pid().empty()) {
        MutableProcess(event.pid());
    }

    switch (event.event_type()) {
        case EventType::kProcessStart: {
            if (event.pid().empty()) {
                return;
            }
            ProcessRecord* process = MutableProcess(event.pid());
            // Перезапись, а не новая запись: ключ — pid, и старый процесс
            // с тем же номером из модели исчезает. Ограничение сознательное,
            // см. заголовок.
            process->ppid = FieldOrEmpty(event, "ppid");
            process->image = FieldOrEmpty(event, "image");
            process->cmdline = FieldOrEmpty(event, "cmdline");
            process->start = event.ts();
            process->alive = true;
            break;
        }
        case EventType::kProcessEnd: {
            if (!event.pid().empty()) {
                MutableProcess(event.pid())->alive = false;
            }
            break;
        }
        case EventType::kFileCreate:
            NoteFileWrite(event, FieldOrEmpty(event, "path"), true);
            break;
        case EventType::kFileWrite:
            NoteFileWrite(event, FieldOrEmpty(event, "path"), false);
            break;
        case EventType::kFileDelete: {
            const std::string path = FieldOrEmpty(event, "path");
            if (!path.empty()) {
                MutableFile(path)->deleted = true;
            }
            break;
        }
        case EventType::kFileMove: {
            // Переименование — два действия над двумя путями. Источник
            // исчезает, цель появляется, и появляется от того, кто переименовал.
            // Шифровальщик виден именно так, и именно поэтому цель считается
            // созданной, а не просто записанной.
            const std::string from = FieldOrEmpty(event, "from");
            if (!from.empty()) {
                MutableFile(from)->deleted = true;
            }
            NoteFileWrite(event, FieldOrEmpty(event, "to"), true);
            break;
        }
        case EventType::kNetConnect:
        case EventType::kOther:
            break;
    }
}

// --- точечный доступ -------------------------------------------------------

const ProcessRecord* EntityModel::FindProcess(const std::string& pid) const {
    for (std::size_t i = 0; i < processes_.size(); ++i) {
        if (processes_[i].pid == pid) {
            return &processes_[i];
        }
    }
    return nullptr;
}

const FileRecord* EntityModel::FindFile(const std::string& path) const {
    for (std::size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].path == path) {
            return &files_[i];
        }
    }
    return nullptr;
}

// --- запросы по связям -----------------------------------------------------

std::vector<std::string> EntityModel::GetAncestorChain(
        const std::string& pid) const {
    std::vector<std::string> chain;

    std::string current = pid;
    while (!current.empty() && chain.size() < kMaxDepth) {
        const ProcessRecord* process = FindProcess(current);
        if (process == nullptr) {
            break;  // знание потока кончилось — цепочка обрывается здесь
        }
        if (Contains(chain, current)) {
            break;  // цикл: pid переиспользован, дальше идти некуда
        }
        chain.push_back(current);
        current = process->ppid;
    }

    return chain;
}

std::vector<std::string> EntityModel::GetProcessTree(
        const std::string& pid) const {
    std::vector<std::string> tree;
    if (FindProcess(pid) == nullptr) {
        return tree;
    }
    tree.push_back(pid);

    // Обход в ширину: индекс идёт по уже найденным, и на каждом шаге ищутся
    // те, чей родитель — текущий. Квадратично по числу процессов; на десятках
    // это ничто, а словарь «родитель -> дети» появится на 4.1.
    for (std::size_t i = 0; i < tree.size() && tree.size() < kMaxDepth; ++i) {
        for (std::size_t j = 0; j < processes_.size(); ++j) {
            if (processes_[j].ppid != tree[i]) {
                continue;
            }
            if (!Contains(tree, processes_[j].pid)) {
                tree.push_back(processes_[j].pid);
            }
        }
    }

    return tree;
}

std::vector<std::string> EntityModel::GetFilesCreatedBy(
        const std::string& pid) const {
    std::vector<std::string> paths;
    if (pid.empty()) {
        return paths;
    }
    for (std::size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].creator_pid == pid) {
            paths.push_back(files_[i].path);
        }
    }
    return paths;
}

const FileRecord* EntityModel::FindSpawnSource(const std::string& pid) const {
    const ProcessRecord* process = FindProcess(pid);
    if (process == nullptr || process->cmdline.empty()) {
        return nullptr;
    }

    // Самый свежий из подходящих. Свежесть важна: путь может встретиться
    // в командной строке дважды за прогон, и интересен последний, кто его
    // записал.
    const FileRecord* best = nullptr;
    for (std::size_t i = 0; i < files_.size(); ++i) {
        const FileRecord& file = files_[i];
        if (file.path.empty() || file.path.size() > process->cmdline.size()) {
            // Путь длиннее командной строки подстрокой в ней быть не может.
            // Проверка на одно сравнение вместо поиска подстроки.
            continue;
        }
        // Слишком давно — не сброс. И заодно самая дешёвая из проверок,
        // поэтому стоит раньше поиска подстроки.
        if (process->start.ms < file.last_write.ms ||
            process->start.ms - file.last_write.ms > kSpawnSourceWindowMs) {
            continue;
        }
        // Без учёта регистра: в командной строке путь пишут как придётся,
        // а файловая система Windows регистр не различает.
        if (!PathMentioned(process->cmdline, file.path)) {
            continue;
        }
        if (best == nullptr || best->last_write <= file.last_write) {
            best = &file;
        }
    }
    return best;
}

std::string EntityModel::DescribeChain(const std::string& pid) const {
    const std::vector<std::string> chain = GetAncestorChain(pid);
    if (chain.empty()) {
        return std::string();
    }

    // Цепочка приходит от процесса к корню, а читается наоборот: сначала кто
    // всё начал. Обход с конца — весь разворот.
    std::string text;
    for (std::size_t i = chain.size(); i > 0; --i) {
        if (!text.empty()) {
            text += '>';
        }
        const std::string& current = chain[i - 1];
        const ProcessRecord* process = FindProcess(current);
        text += current;
        if (process != nullptr && !process->image.empty()) {
            // Только имя файла: полный путь к образу в одну строку детекта
            // не влезает и не читается.
            text += std::format("({})", FileName(process->image));
        }
    }
    return text;
}

}  // namespace nano_edr
