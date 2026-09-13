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

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <print>
#include <string>
#include <vector>
#include <unordered_map>

int main(int argc, char** argv) {
    // Аргументы разбираются грубо: путь к журналу и ничего больше. Остальное,
    // включая --quiet, добавляется по заданию.
    bool quiet = 0;
    for (int i = 0; i < argc; i++) {
        if (!std::strcmp(argv[i], "--quiet")) quiet = 1;
    }

    std::vector<std::string> rules = {"wscript.exe", ".locked", "certutil.exe", "\\Startup\\"};
    std::unordered_map<int, int> dc;
    if (argc < 2) {
        std::print(stderr, "использование: nano-edr <журнал.log>\n");
        return 2;
    }

    std::ifstream log(argv[1]);
    if (!log) {
        std::print(stderr, "не удалось открыть журнал: {}\n", argv[1]);
        return 2;
    }

    int lines = 0, comments = 0;
    std::string line;

    while (std::getline(log, line)) {
        // Счётчик увеличивается до всех проверок: он считает строки файла,
        // а не события. Номер, посчитанный по событиям, бесполезен — по нему
        // нельзя открыть файл и посмотреть.
        ++lines;

        // Строки-комментарии в журнале начинаются с '#'. Они не события,
        // и детекта по ним быть не должно.
        if (!line.empty() && line[0] == '#') {
            ++comments;
            continue;
        }
        int rule_cnt = 0;
        for (std::string rule : rules) {
            if ((int)line.find(rule) != -1) {
                std::printf("[DETECT] строка %d, признак %s: %s\n", lines, rule.c_str(), line.c_str());
                dc[rule_cnt]++;
            }
            rule_cnt++;
        }
    }
    
    if (!quiet){
        std::printf("Строк %d, из них комментариев %d\n", lines, comments);
        for (int i = 0; (size_t)i < rules.size(); i++) {
            std::printf("Признак %s встретился %d раз\n", rules[i].c_str(), dc[i]);
        }
    }
    return 0;
}
