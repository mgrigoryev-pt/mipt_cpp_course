# Тесты, выдаваемые на занятии 3.2.
#
# Четыре получателя детектов, подавление дубликатов, модель сущностей,
# связка child_of_prev и правило из предиката. Подключение — см. постановку
# занятия.
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

set(task3_2_sources
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/entity_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sink_tests.cpp)

add_executable(task3_2_tests ${task3_2_sources})

target_include_directories(task3_2_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

target_link_libraries(task3_2_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, предупреждения в нём не ваши.
if(NOT MSVC)
    target_compile_options(task3_2_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task3.2 COMMAND task3_2_tests)

# ---------------------------------------------------------------------------
# Запуски агента
# ---------------------------------------------------------------------------
#
# Тесты выше проверяют получатели, модель и правила по отдельности и проходят
# у того, кто не подключил их к агенту. Эти случаи запускают nano-edr
# и сверяют то, что видно в выводе. Действуют только на занятии 3.2: дальше
# вывод меняют следующие занятия, и эталон перестал бы совпадать.
if(NOT NANO_EDR_LESSON STREQUAL "3.2")
    return()
endif()

# Цель агента ищется по имени так же, как в наборе 3.1: nano-edr в комплекте
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

# Тайм-аут на каждый запуск: агент, который не выходит из цикла ожидания,
# повесил бы весь прогон тестов.
set(NANO_EDR_AGENT_RUN_TIMEOUT 60)

#   nano_edr_agent_case(<тест> <код> [ONLY_DETECTS] [EXPECTED <эталон>]
#                       [OUT_FILE <файл>] [ARGS <до трёх аргументов агента>])
function(nano_edr_agent_case test code)
    cmake_parse_arguments(PARSE_ARGV 2 run "ONLY_DETECTS" "EXPECTED;OUT_FILE"
                          "ARGS")
    set(defines "")
    set(i 0)
    foreach(arg IN LISTS run_ARGS)
        math(EXPR i "${i} + 1")
        list(APPEND defines -DARG${i}=${arg})
    endforeach()
    if(run_ONLY_DETECTS)
        list(APPEND defines -DONLY_DETECTS=ON)
    endif()
    if(run_EXPECTED)
        list(APPEND defines -DEXPECTED=${run_EXPECTED})
    endif()
    if(run_OUT_FILE)
        list(APPEND defines -DOUT_FILE=${run_OUT_FILE})
    endif()
    add_test(NAME ${test}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:${NANO_EDR_AGENT}>
                     -DCODE=${code}
                     ${defines}
                     -P ${CMAKE_CURRENT_LIST_DIR}/run_agent.cmake)
    set_tests_properties(${test} PROPERTIES
                         TIMEOUT ${NANO_EDR_AGENT_RUN_TIMEOUT})
endfunction()

# Контекст и детекты на семи сценариях: цепочка у каждого детекта, два новых
# правила на phishing_macro, подавленные дубликаты на ransomware
# и persistence, ни одного детекта на clean_office и clean_build.
foreach(scenario IN ITEMS clean_build clean_office lolbin_download persistence
                          phishing_macro pid_reuse ransomware)
    set(expected ${CMAKE_CURRENT_LIST_DIR}/expected/${scenario}.out)
    nano_edr_agent_case(task3.2.output.${scenario} 0 EXPECTED ${expected}
                        ARGS ${NANO_EDR_KIT_ROOT}/scenarios/${scenario}.log)
endforeach()

# Под --quiet в stdout только детекты: контекст печатает агент, и тихий
# режим глушит его, а не консоль.
nano_edr_agent_case(task3.2.quiet 0 ONLY_DETECTS
                    ARGS --quiet ${NANO_EDR_KIT_ROOT}/scenarios/phishing_macro.log)

# --out пишет в файл те же строки [DETECT], что и консоль.
nano_edr_agent_case(task3.2.out_file 0
                    OUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/task3_2_out.txt
                    ARGS --out ${CMAKE_CURRENT_BINARY_DIR}/task3_2_out.txt
                         ${NANO_EDR_KIT_ROOT}/scenarios/phishing_macro.log)
