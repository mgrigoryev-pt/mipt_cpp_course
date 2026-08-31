# Тесты, выдаваемые на занятии 3.3.
#
# Move-семантика с замером и план реагирования. Половина тестов — static_assert
# про правило пяти и правило нуля, половина — числа из счётчика копий.
# Тесты плана не трогают ни границу, ни диск. Подключение — см. README.md рядом.
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
