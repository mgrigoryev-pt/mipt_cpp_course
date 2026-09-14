// Каркас агента: читает журнал событий построчно и считает строки.
//
// Это заготовка занятия 1.1, а не решение. Детектов она не ищет — их вы
// добавите здесь же, в отмеченном месте ниже. Формат строки детекта, список
// признаков и правило про их порядок заданы в постановке занятия: по ним
// сравниваются эталоны.
//
// Весь код лежит в main, и на этом занятии так и надо: функции появятся
// на занятии 1.2, ссылки — на 1.3. Разбор аргументов, коды возврата и флаг
// --quiet — часть задания.
//
// Запуск:
//   nano-edr <журнал.log>
#include <cstdio>
#include <fstream>
#include <print>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char** argv) {
    // Аргументы разбираются грубо: путь к журналу и ничего больше. Остальное,
    // включая --quiet, добавляется по заданию.
    bool quiet = false;
    const char* log_path = nullptr;
    for (int argument = 1; argument < argc; ++argument) {
        if (std::string(argv[argument]) == "--quiet") {
            quiet = true;
        } else if (log_path == nullptr) {
            log_path = argv[argument];
        } else {
            std::print(stderr, "неизвестный аргумент: {}\n", argv[argument]);
            return 2;
        }
    }

    if (log_path == nullptr) {
        std::print(stderr, "использование: nano-edr <журнал.log>\n");
        return 2;
    }

    std::ifstream log(log_path);
    if (!log) {
        std::print(stderr, "не удалось открыть журнал: {}\n", log_path);
        return 2;
    }

    long long lines = 0;
    long long comments = 0;
    long long events = 0;
    std::vector<std::pair<std::string, long long>> event_types;
    std::string line;

    while (std::getline(log, line)) {
        // Счётчик увеличивается до всех проверок: он считает строки файла,
        // а не события. Номер, посчитанный по событиям, бесполезен — по нему
        // нельзя открыть файл и посмотреть.
        ++lines;

        // Строки-комментарии в журнале начинаются с '#'. Они не события,
        // и детекта по ним быть не должно.
        std::size_t first = 0;
        while (first < line.size() && (line[first] == ' ' || line[first] == '\t')) {
            ++first;
        }
        if (first == line.size() || line[first] == '#' || line[first] == ';') {
            ++comments;
            continue;
        }

        // >>> Здесь начинается занятие 1.1.
        //
        // Проверка признаков и печать детекта. Номер строки, который нужен
        // в выводе, — это lines.
        ++events;

        std::size_t type_start = line.find("type=");
        if (type_start != std::string::npos) {
            type_start += 5;
            std::size_t type_end = line.find(' ', type_start);
            std::string type = line.substr(type_start, type_end - type_start);
            bool known_type = false;
            for (auto& entry : event_types) {
                if (entry.first == type) {
                    ++entry.second;
                    known_type = true;
                    break;
                }
            }
            if (!known_type) {
                event_types.emplace_back(type, 1);
            }
        }

        const std::vector<std::string> indicators = {
            "wscript.exe", ".locked", "certutil.exe", "\\Startup\\"};
        for (const auto& indicator : indicators) {
            if (line.find(indicator) != std::string::npos) {
                std::print("[DETECT] строка {}, признак {}: {}\n",
                           lines, indicator, line);
            }
        }
    }

    if (!quiet) {
        std::print("событий {}, строк {}, из них комментариев {}\n",
                   events, lines, comments);
        for (const auto& [type, count] : event_types) {
            std::print("{}: {}\n", type, count);
        }
    }
    return 0;
}
