# Тесты, выдаваемые на занятии 2.1.
#
# Пять групп: событие с инвариантом, разбор строки, окно событий, условия,
# поиск полей в шапке события — и отдельно обёртка над границей os.h. В конце
# файла — случаи, которые запускают агента целиком. Подключение — см.
# постановку занятия.
#
# Правьте этот файл, только если понимаете, зачем: он же прогоняется при сдаче.

# Библиотека с вашим кодом. Цель nano_edr_core создаётся сама, как только
# в src/ появляется хоть один .cpp кроме main.cpp. Если ваша называется иначе,
# передайте имя до подключения: set(NANO_EDR_CORE my_core)
if(NOT DEFINED NANO_EDR_CORE)
    set(NANO_EDR_CORE nano_edr_core)
endif()

if(NOT TARGET ${NANO_EDR_CORE})
    message(FATAL_ERROR
        "Не найдена цель '${NANO_EDR_CORE}' — тестам не с чем линковаться.")
endif()

get_filename_component(NANO_EDR_KIT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

set(task2_1_sources
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/event_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/parse_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/conditions_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/fields_tests.cpp)

# Тесты обёртки над границей — отдельной строкой и под условием.
#
# На занятии 4.2 конструктор OsHandle уезжает в приватные, а на его место
# встаёт фабрика Open с std::expected. Тесты этого файла проверяют ровно
# то, чего после этого нет: что конструктор бросает на несуществующем файле
# и что сообщение исключения называет путь.
#
# Это не «тесты устарели». Это цена смены способа сообщить об ошибке, и она
# честная: проверялся контракт, контракт заменён, значит проверка заменяется
# тоже — на тесты фабрики из набора 4.2. Всё остальное в наборе 2.1 (событие,
# разбор, условия, поля) на 4.2 нужно ровно так же, как на 2.1.
if(NOT NANO_EDR_NO_THROWING_OS_HANDLE)
    list(APPEND task2_1_sources ${CMAKE_CURRENT_LIST_DIR}/os_handle_tests.cpp)
endif()

# Тесты окна на списке — отдельной строкой и под условием.
#
# Занятие 4.1 заменяет свои структуры данных стандартными, и односвязный список
# при этом удаляется: с занятия 3.1 окно живёт на кольцевом буфере, а список
# остался в проекте только потому, что его никто не выбросил. Удалённый тип
# нельзя проверять тестами — значит тесты уходят вместе с ним.
#
# Это и есть настоящая цена своего примитива, и она видна именно здесь: пока
# он был, его надо было проверять; когда его не стало, из набора пришлось
# вынимать файл. Стандартный контейнер такого следа не оставляет — у него
# свои тесты, и они не ваши.
#
# Остальные пять файлов набора проверяют то, что никуда не делось (событие,
# разбор, условия, поля, обёртка над границей), и на 4.1 нужны ровно так же,
# как на 2.1: они и есть страховка от того, что рефакторинг задел чужое.
if(NOT NANO_EDR_NO_EVENT_LIST)
    list(APPEND task2_1_sources ${CMAKE_CURRENT_LIST_DIR}/list_tests.cpp)
endif()

add_executable(task2_1_tests ${task2_1_sources})

target_include_directories(task2_1_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

# Тестам обёртки нужен настоящий журнал: проверяется, что конструктор бросает
# на несуществующем файле и не бросает на исправном. Путь передаётся сборкой —
# тест не должен знать, где лежит репозиторий.
target_compile_definitions(task2_1_tests PRIVATE
    NANO_EDR_SCENARIOS="${NANO_EDR_KIT_ROOT}/scenarios")

target_link_libraries(task2_1_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, предупреждения в нём не ваши.
if(NOT MSVC)
    target_compile_options(task2_1_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task2.1 COMMAND task2_1_tests)

# ---------------------------------------------------------------------------
# Агент целиком: два источника, эталоны детектов, коды завершения
# ---------------------------------------------------------------------------
#
# Эти случаи запускают nano-edr, а не библиотеку. Трамплин, перевод os_event
# в Event, источник --file и коды завершения живут в вашем агенте, и из
# библиотеки их не видно: имена там ваши.
#
# С занятия 3.1 случаи выпадают целиком, эталоны — уже с 2.3. Почему именно
# так — в tasks/lessons.cmake. Дальше файла ничего нет, поэтому выход
# отсюда — это выход из набора.
if(NANO_EDR_NO_AGENT_RUNS)
    return()
endif()

# Цель агента ищется по имени так же, как в наборах 1.1 и 1.2: nano-edr
# в комплекте студента, nano_edr — в решениях. Если ваша называется иначе,
# передайте имя до подключения: set(NANO_EDR_AGENT my_agent)
if(NOT DEFINED NANO_EDR_AGENT)
    if(TARGET nano-edr)
        set(NANO_EDR_AGENT nano-edr)
    else()
        set(NANO_EDR_AGENT nano_edr)
    endif()
endif()

if(NOT TARGET ${NANO_EDR_AGENT})
    message(FATAL_ERROR
        "Не найдена цель '${NANO_EDR_AGENT}' — случаям агента нечего "
        "запускать.\nНазовите цель агента nano-edr либо передайте своё имя:\n"
        "  set(NANO_EDR_AGENT my_agent)")
endif()

# Два источника на одном журнале дают одинаковый вывод под --quiet. Пока
# эталоны действуют, вывод сверяется и с ними: семь сценариев и свой журнал
# с границами пяти правил.
function(nano_edr_sources_case name log)
    set(expected "")
    if(NOT NANO_EDR_NO_AGENT_GOLDEN)
        set(expected -DEXPECTED=${CMAKE_CURRENT_LIST_DIR}/expected/${name}.out)
    endif()
    add_test(NAME task2.1.sources.${name}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:${NANO_EDR_AGENT}>
                     -DLOG=${log}
                     ${expected}
                     -P ${CMAKE_CURRENT_LIST_DIR}/run_sources.cmake)
endfunction()

foreach(scenario IN ITEMS clean_build clean_office lolbin_download persistence
                          phishing_macro pid_reuse ransomware)
    nano_edr_sources_case(${scenario}
                          ${NANO_EDR_KIT_ROOT}/scenarios/${scenario}.log)
endforeach()

nano_edr_sources_case(rules_edges
                      ${CMAKE_CURRENT_LIST_DIR}/logs/rules_edges.log)

# Коды завершения: 2 — main не смог разобрать командную строку, 1 — ошибка
# во время работы. Несуществующий файл — это уже работа, поэтому 1, и у обоих
# источников одинаково.
function(nano_edr_exit_case name code)
    set(args "")
    set(i 0)
    foreach(arg IN LISTS ARGN)
        math(EXPR i "${i} + 1")
        list(APPEND args -DARG${i}=${arg})
    endforeach()
    add_test(NAME task2.1.exit.${name}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:${NANO_EDR_AGENT}>
                     -DCODE=${code}
                     ${args}
                     -P ${CMAKE_CURRENT_LIST_DIR}/run_exit_code.cmake)
endfunction()

set(clean_log ${NANO_EDR_KIT_ROOT}/scenarios/clean_office.log)
set(missing_log ${NANO_EDR_KIT_ROOT}/scenarios/no-such-file.log)

nano_edr_exit_case(no_arguments 2)
nano_edr_exit_case(unknown_option 2 --no-such-option ${clean_log})
nano_edr_exit_case(file_without_value 2 --file)
nano_edr_exit_case(path_twice 2 ${clean_log} --file ${clean_log})
nano_edr_exit_case(missing_file_os 1 ${missing_log})
nano_edr_exit_case(missing_file_file 1 --file ${missing_log})
