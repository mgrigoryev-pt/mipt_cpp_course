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
#include <vector>
#include <unordered_map>

const std::vector<std::string> attributes = {
    "wscript.exe", 
    ".locked", 
    "certutil.exe", 
    "\\Startup\\",
};


int main(int argc, char** argv) {
    // Аргументы разбираются грубо: путь к журналу и ничего больше. Остальное,
    // включая --quiet, добавляется по заданию.
    

    bool is_quite = false;

    if (argc < 2) {
        std::print(stderr, "использование: nano-edr <журнал.log>\n");
        return 2;
    }

    std::ifstream log(argv[1]);
    if (!log) {
        std::print(stderr, "не удалось открыть журнал: {}\n", argv[1]);
        return 2;
    }

    //Проверка на флаги
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--quiet") {
            is_quite = true;
        }
    }

    long long lines = 0;
    long long comments = 0;
    long long events = 0;
    std::pmr::unordered_map<std::string, long long> event_types_count;
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
        } else if (!line.empty()) {
            ++events;

            //Проверка на известные признаки
            for (auto& attribute:attributes) {
                if (line.find(attribute) != std::string::npos) {
                    std::print("[DETECT] строка {}, признак {}: {}\n", lines, attribute, line);
                }
            }

            //Типизация события
            int type_start_index = line.find("type=")+5;
            int type_end_index = line.find(" ", type_start_index);
            int type_len = type_end_index - type_start_index;
            std::string type = line.substr(type_start_index, type_len);
            ++event_types_count[type];

        }

        // >>> Здесь начинается занятие 1.1.
        //
        // Проверка признаков и печать детекта. Номер строки, который нужен
        // в выводе, — это lines.
    }

    //Необязательный вывод
    if (!is_quite){
        std::println("строк {}, из них комментариев {}\n", lines, comments);
        std::println("событий всего : {}", events);
        for (auto& [type, count]:event_types_count){
            std::println("событий типа {} всего : {}", type, count);
        }

    }
    return 0;
}
