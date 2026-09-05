# Тесты, выдаваемые на занятии 4.2.
#
# Три группы: фабрика границы с std::expected, разбор на string_view
# и загрузчик правил с номерами строк. Первая трогает границу — ей нужен
# настоящий сценарий, путь передаётся сборкой. Подключение — см. README.md.
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

add_executable(task4_2_tests
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/os_handle_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/parse_view_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/rule_loader_tests.cpp)

target_include_directories(task4_2_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

# Тестам фабрики нужен настоящий журнал: проверяется, что Open отказывает
# на несуществующем файле и не отказывает на исправном.
target_compile_definitions(task4_2_tests PRIVATE
    NANO_EDR_SCENARIOS="${NANO_EDR_KIT_ROOT}/scenarios")

# Два случая загрузчика — про виды правил sequence и threshold, то есть
# про продвинутую часть занятия. Базовая часть читает из файла только match,
# поэтому по умолчанию они не собираются: тест, требующий несделанного,
# сообщает о нём как о сломанном, и студент чинит не то.
if(NANO_EDR_RULE_KINDS)
    target_compile_definitions(task4_2_tests PRIVATE NANO_EDR_RULE_KINDS)
endif()

target_link_libraries(task4_2_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, предупреждения в нём не ваши.
if(NOT MSVC)
    target_compile_options(task4_2_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task4_2 COMMAND task4_2_tests)
