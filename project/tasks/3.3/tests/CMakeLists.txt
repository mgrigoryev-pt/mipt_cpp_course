# Тесты, выдаваемые на занятии 3.3.
#
# Move-семантика с замером и план реагирования. Половина тестов — static_assert
# про правило пяти и правило нуля, половина — числа из счётчика копий.
# Тесты плана не трогают ни границу, ни диск.
# Подключение — см. постановку занятия.
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

add_executable(task3_3_tests
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/move_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/response_tests.cpp)

target_include_directories(task3_3_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

target_link_libraries(task3_3_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, предупреждения в нём не ваши.
if(NOT MSVC)
    target_compile_options(task3_3_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task3.3 COMMAND task3_3_tests)

# ---------------------------------------------------------------------------
# Запуски агента
# ---------------------------------------------------------------------------
#
# Тесты выше проверяют план без симулятора и проходят у того, кто не подключил
# реагирование к агенту. Эти случаи запускают nano-edr на конфигурации
# сценария, сверяют строки [DETECT], [ACT] и [PLAN] с эталоном и спрашивают
# sim_verify, изменился ли мир так, как ждёт сценарий. Действуют только
# на занятии 3.3: дальше вывод меняют следующие занятия.
if(NOT NANO_EDR_LESSON STREQUAL "3.3")
    return()
endif()

# Цель агента ищется по имени так же, как в наборе 3.2: nano-edr в комплекте
# студента, nano_edr — в решениях. Если ваша называется иначе, передайте имя
# до подключения: set(NANO_EDR_AGENT my_agent)
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

if(WIN32)
    set(NANO_EDR_SIM_VERIFY ${NANO_EDR_KIT_ROOT}/tools/win-x64/sim_verify.exe)
else()
    set(NANO_EDR_SIM_VERIFY ${NANO_EDR_KIT_ROOT}/tools/linux-x64/sim_verify)
endif()

set(NANO_EDR_AGENT_RUN_TIMEOUT 60)

#   nano_edr_act_case(<тест> <сценарий> [VERIFY] [EXPECTED <эталон>]
#                     [LINE <строка>] ARGS <до двух флагов агента>)
#
# Агент запускается на scenarios/<сценарий>.cfg: только тогда у него есть
# граница os.h, через которую он действует. Отчёт симулятора пишется рядом
# со сценарием, поэтому два случая одного сценария не идут параллельно.
function(nano_edr_act_case test scenario)
    cmake_parse_arguments(PARSE_ARGV 2 run "VERIFY" "EXPECTED;LINE" "ARGS")
    set(config ${NANO_EDR_KIT_ROOT}/scenarios/${scenario}.cfg)
    set(defines "")
    set(i 0)
    foreach(arg IN LISTS run_ARGS ITEMS ${config})
        math(EXPR i "${i} + 1")
        list(APPEND defines -DARG${i}=${arg})
    endforeach()
    if(run_EXPECTED)
        list(APPEND defines -DEXPECTED=${run_EXPECTED})
    endif()
    if(run_LINE)
        list(APPEND defines "-DLINE=${run_LINE}")
    endif()
    if(run_VERIFY)
        list(APPEND defines -DVERIFY=${NANO_EDR_SIM_VERIFY} -DCONFIG=${config})
    endif()
    add_test(NAME ${test}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:${NANO_EDR_AGENT}>
                     -DCODE=0
                     ${defines}
                     -P ${CMAKE_CURRENT_LIST_DIR}/run_agent.cmake)
    set_tests_properties(${test} PROPERTIES
                         TIMEOUT ${NANO_EDR_AGENT_RUN_TIMEOUT}
                         RESOURCE_LOCK nano_edr_scenario_${scenario})
endfunction()

# Детекты и действия по порядку, и мир изменился как надо: на phishing_macro
# завершены 1101 и 1156 и изолирован a.js, на pid_reuse завершён только первый
# владелец номера 1042. На чистых сценариях ни детектов, ни действий.
foreach(scenario IN ITEMS phishing_macro pid_reuse clean_office clean_build)
    nano_edr_act_case(task3.3.act.${scenario} ${scenario} VERIFY
                      EXPECTED ${CMAKE_CURRENT_LIST_DIR}/expected/${scenario}.out
                      ARGS --quiet)
endforeach()

# На ransomware — только мир: шифровальщик завершён, его образ изолирован.
# Строки не сверяются: шифровальщика вправе поймать и правило сверх
# постановки, и тогда строк будет больше.
nano_edr_act_case(task3.3.act.ransomware ransomware VERIFY ARGS --quiet)

# --dry-run печатает план строками [PLAN] и не исполняет ничего.
nano_edr_act_case(task3.3.dry_run phishing_macro
                  EXPECTED ${CMAKE_CURRENT_LIST_DIR}/expected/dry_run.out
                  ARGS --dry-run --quiet)

# Агент узнаёт события, порождённые его же действиями: на phishing_macro
# их три, по одному на действие, — завершения двух процессов и перенос a.js
# в карантин.
nano_edr_act_case(task3.3.own_events phishing_macro
                  LINE "[STAT] своих событий 3")

# Счётчик копий: к концу занятия копий одна на детект.
nano_edr_act_case(task3.3.copies phishing_macro
                  LINE "[COPY] копий всего 5"
                  ARGS --quiet --copies)
