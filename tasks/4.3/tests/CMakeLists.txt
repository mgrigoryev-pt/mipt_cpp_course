# Тесты, выдаваемые на занятии 4.3.
#
# Две части, и они разного рода.
#
# Первая — обычные тесты на doctest: поток отправки сработок, атомарный
# счётчик, слабая ссылка на родителя, копия сработки без улик.
#
# Вторая — прогон каждого сценария в двух режимах доставки и сравнение
# результата с самим собой. Это критерий занятия дословно, и эталон для него
# не нужен: сравнивается ваш агент с вашим же агентом. Конфигурации режима
# thread делаются здесь же из открытых сценариев — тем, что в них меняется
# ровно одна строка.
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

add_executable(task4_3_tests
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/reporter_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shared_tests.cpp)

target_include_directories(task4_3_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

target_link_libraries(task4_3_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, предупреждения в нём не ваши.
if(NOT MSVC)
    target_compile_options(task4_3_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task4_3 COMMAND task4_3_tests)

# ---------------------------------------------------------------------------
# Два режима доставки на одном сценарии
# ---------------------------------------------------------------------------
#
# Цель агента ищется по имени. Если ваш исполняемый файл называется иначе,
# передайте своё имя перед add_subdirectory:
#
#     set(NANO_EDR_AGENT my_agent)
#
# Имён по умолчанию два, потому что и в скелете студента, и в решениях цель
# называется по-своему: nano-edr там, nano_edr здесь.
if(NOT DEFINED NANO_EDR_AGENT)
    if(TARGET nano-edr)
        set(NANO_EDR_AGENT nano-edr)
    else()
        set(NANO_EDR_AGENT nano_edr)
    endif()
endif()

if(TARGET ${NANO_EDR_AGENT})
    set(NANO_EDR_SCENARIO_DIR ${NANO_EDR_KIT_ROOT}/scenarios)

    foreach(case IN ITEMS phishing_macro ransomware lolbin_download persistence
                         pid_reuse clean_office clean_build)
        # Конфигурация режима thread делается из открытой копированием
        # и заменой одной строки. Пути к журналу и к отчётам разворачиваются
        # в абсолютные: файл лежит уже не рядом со сценарием.
        set(source_cfg ${NANO_EDR_SCENARIO_DIR}/${case}.cfg)
        file(READ ${source_cfg} cfg_text)
        string(REPLACE "delivery = inline" "delivery = thread" cfg_text "${cfg_text}")
        string(REPLACE "events   = ${case}.log"
                       "events   = ${NANO_EDR_SCENARIO_DIR}/${case}.log"
                       cfg_text "${cfg_text}")
        string(REPLACE "report   = ${case}.report.txt"
                       "report   = ${CMAKE_CURRENT_BINARY_DIR}/${case}.thread.report.txt"
                       cfg_text "${cfg_text}")
        string(REPLACE "out = ${case}.detections.txt"
                       "out = ${CMAKE_CURRENT_BINARY_DIR}/${case}.thread.detections.txt"
                       cfg_text "${cfg_text}")
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${case}.thread.cfg "${cfg_text}")

        add_test(NAME modes.${case}
                 COMMAND ${CMAKE_COMMAND}
                         -DEXE=$<TARGET_FILE:${NANO_EDR_AGENT}>
                         -DINLINE_CFG=${source_cfg}
                         -DTHREAD_CFG=${CMAKE_CURRENT_BINARY_DIR}/${case}.thread.cfg
                         -P ${CMAKE_CURRENT_LIST_DIR}/run_modes.cmake)
    endforeach()
else()
    message(STATUS
        "task4_3: цель '${NANO_EDR_AGENT}' не найдена, сравнение режимов "
        "доставки пропущено. Имя цели задаётся через NANO_EDR_AGENT.")
endif()
