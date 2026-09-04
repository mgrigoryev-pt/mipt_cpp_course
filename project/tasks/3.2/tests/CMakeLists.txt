# Тесты, выдаваемые на занятии 3.2.
#
# Свой Function, четыре получателя детектов, подавление дубликатов, модель
# сущностей, связка child_of_prev и правило из предиката. Подключение —
# см. README.md рядом.
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

add_executable(task3_2_tests
    ${NANO_EDR_KIT_ROOT}/tests/doctest_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/function_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/entity_tests.cpp)

target_include_directories(task3_2_tests PRIVATE
    ${NANO_EDR_KIT_ROOT}/tests/third_party)

target_link_libraries(task3_2_tests PRIVATE ${NANO_EDR_CORE})

# doctest — сторонний заголовок, предупреждения в нём не ваши.
if(NOT MSVC)
    target_compile_options(task3_2_tests PRIVATE -Wno-old-style-cast)
endif()

add_test(NAME task3.2 COMMAND task3_2_tests)
