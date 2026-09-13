# Тесты, выдаваемые на занятии 1.2.
#
# Подключаются ключом -D NANO_EDR_LESSON при конфигурации — см. постановку
# занятия. Проверяют список, разбор строки и контекст детекта: то, что задание
# просит довести до состояния «проходит выданные тесты».
#
# Групп две по способу запуска. Список и разбор линкуются с вашим кодом
# и зовут функции напрямую. Контекст запускает агента целиком: ни один тест
# на заголовках не отличает написанный примитив от применённого, а занятие
# половиной своей работы лежит как раз в main.cpp.
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

add_executable(task1_2_tests
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/list_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/parse_tests.cpp)

target_include_directories(task1_2_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

target_link_libraries(task1_2_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, и предупреждения в нём не ваши. Гасятся ровно
# те, которые он вызывает под флагами курса; остальные остаются включёнными.
if(NOT MSVC)
    target_compile_options(task1_2_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task1.2 COMMAND task1_2_tests)

# ---------------------------------------------------------------------------
# Контекст детекта: агент запускается целиком
# ---------------------------------------------------------------------------
#
# С занятия 1.3 эти случаи выпадают: эталон снят с детектов-подстрок, а там
# детект — сработавшее правило, и контекст встаёт в других местах журнала.
# Почему именно так — в tasks/lessons.cmake. Дальше файла ничего нет, поэтому
# выход отсюда — это выход из набора; add_subdirectory делает его безопасным.
if(NANO_EDR_NO_CONTEXT_CASES)
    return()
endif()

# Цель агента ищется по имени так же, как в наборе 1.1: nano-edr в комплекте
# студента, nano_edr — в решениях. Если ваша называется иначе, передайте своё
# имя до подключения: set(NANO_EDR_AGENT my_agent)
if(NOT DEFINED NANO_EDR_AGENT)
    if(TARGET nano-edr)
        set(NANO_EDR_AGENT nano-edr)
    else()
        set(NANO_EDR_AGENT nano_edr)
    endif()
endif()

if(NOT TARGET ${NANO_EDR_AGENT})
    message(FATAL_ERROR
        "Не найдена цель '${NANO_EDR_AGENT}' — случаям контекста нечего "
        "запускать.\nНазовите цель агента nano-edr либо передайте своё имя:\n"
        "  set(NANO_EDR_AGENT my_agent)")
endif()

# Журналы лежат в scenarios/ комплекта, тремя уровнями выше.
function(nano_edr_context_case name log)
    add_test(NAME task1.2.context.${name}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:${NANO_EDR_AGENT}>
                     -DLOG=${NANO_EDR_KIT_ROOT}/scenarios/${log}.log
                     -DEXPECTED=${CMAKE_CURRENT_LIST_DIR}/expected/${name}.ctx
                     ${ARGN}
                     -P ${CMAKE_CURRENT_LIST_DIR}/run_context.cmake)
endfunction()

# Два сценария с настройками по умолчанию: окно ведётся, контекст печатается.
nano_edr_context_case(phishing_macro phishing_macro)
nano_edr_context_case(clean_office clean_office)

# Окно на одно место: контекст обязан укоротиться до одной строки. Проверяет
# заодно разбор --window-size и вытеснение на живом прогоне, а не в тесте
# списка.
nano_edr_context_case(window_one phishing_macro -DWINDOW=1)

# Под --quiet строк [CTX] не бывает вовсе: stdout принадлежит детектам.
nano_edr_context_case(quiet phishing_macro -DQUIET=ON)
