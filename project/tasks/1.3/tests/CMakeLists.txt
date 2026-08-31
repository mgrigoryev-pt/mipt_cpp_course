# Тесты, выдаваемые на занятии 1.3.
#
# Проверяют слой доступа к полям и механику таблицы правил: «таблица тестов
# на каждую функцию» из домашнего задания. Подключение — см. README.md рядом.
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

add_executable(task1_3_tests
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/fields_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/rules_tests.cpp)

target_include_directories(task1_3_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

target_link_libraries(task1_3_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, и предупреждения в нём не ваши. Гасятся ровно
# те, которые он вызывает под флагами курса; остальные остаются включёнными.
if(NOT MSVC)
    target_compile_options(task1_3_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task1.3 COMMAND task1_3_tests)
