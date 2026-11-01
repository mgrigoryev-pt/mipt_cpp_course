# Тесты, выдаваемые на занятии 3.1.
#
# Кольцевой буфер, пороговое правило, комбинаторы на шаблонах
# и типизированный доступ к полям. Подключение — см. постановку занятия.
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

add_executable(task3_1_tests
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/template_tests.cpp)

target_include_directories(task3_1_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

target_link_libraries(task3_1_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, предупреждения в нём не ваши.
if(NOT MSVC)
    target_compile_options(task3_1_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task3.1 COMMAND task3_1_tests)

# ---------------------------------------------------------------------------
# Запуски агента
# ---------------------------------------------------------------------------
#
# Эти случаи запускают nano-edr, а не библиотеку: окно, правило на шаблонах
# и флаги живут в вашем агенте. Действуют только на занятии 3.1 — дальше
# вывод меняют следующие занятия, и эталон перестал бы совпадать. Так же
# устроены случаи контекста 1.2 и эталоны 2.1.
if(NOT NANO_EDR_LESSON STREQUAL "3.1")
    return()
endif()

# Цель агента ищется по имени так же, как в наборах 1.1–2.1: nano-edr
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

# Тайм-аут на каждый запуск агента. Своего у CTest нет, и агент, который
# не выходит из цикла ожидания, повесил бы весь прогон тестов.
set(NANO_EDR_AGENT_RUN_TIMEOUT 60)

#   nano_edr_agent_case(<тест> <код> [ONLY_DETECTS] [EXPECTED <эталон>]
#                       [ARGS <до трёх аргументов агента>])
function(nano_edr_agent_case test code)
    cmake_parse_arguments(PARSE_ARGV 2 run "ONLY_DETECTS" "EXPECTED" "ARGS")
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
    add_test(NAME ${test}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:${NANO_EDR_AGENT}>
                     -DCODE=${code}
                     ${defines}
                     -P ${CMAKE_CURRENT_LIST_DIR}/run_agent.cmake)
    set_tests_properties(${test} PROPERTIES
                         TIMEOUT ${NANO_EDR_AGENT_RUN_TIMEOUT})
endfunction()

# Детекты и контекст на семи сценариях: четыре строки нового правила
# на ransomware, ни одного детекта на clean_office и clean_build, контекст
# перед детектами своего события.
foreach(scenario IN ITEMS clean_build clean_office lolbin_download persistence
                          phishing_macro pid_reuse ransomware)
    set(expected ${CMAKE_CURRENT_LIST_DIR}/expected/${scenario}.out)
    nano_edr_agent_case(task3.1.output.${scenario} 0 EXPECTED ${expected}
                        ARGS ${NANO_EDR_KIT_ROOT}/scenarios/${scenario}.log)
endforeach()

# Под --quiet в stdout только детекты: ни контекста, ни сводки.
nano_edr_agent_case(task3.1.quiet 0 ONLY_DETECTS
                    ARGS --quiet ${NANO_EDR_KIT_ROOT}/scenarios/ransomware.log)

# Флага --window-size больше нет: агент отвечает на него как на любой
# неизвестный флаг, кодом 2.
nano_edr_agent_case(task3.1.no_window_size 2
                    ARGS --window-size 8
                         ${NANO_EDR_KIT_ROOT}/scenarios/ransomware.log)
