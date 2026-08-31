# Тесты, выдаваемые на занятии 2.1.
#
# Четыре группы: событие с инвариантом, разбор строки, окно событий, условия —
# и отдельно обёртка над границей os.h. Подключение — см. README.md рядом.
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
    ${CMAKE_CURRENT_LIST_DIR}/conditions_tests.cpp)

# Тесты обёртки над границей — отдельной строкой и под условием.
#
# На занятии Ф2 конструктор OsHandle уезжает в приватные, а на его место
# встаёт фабрика Open с std::expected. Тесты этого файла проверяют ровно
# то, чего после этого нет: что конструктор бросает на несуществующем файле
# и что сообщение исключения называет путь.
#
# Это не «тесты устарели». Это цена смены канала отказа, и она честная:
# проверялся контракт, контракт заменён, значит проверка заменяется тоже —
# на тесты фабрики из набора Ф2. Всё остальное в наборе 2.1 (событие, разбор,
# условия) на Ф2 нужно ровно так же, как на 2.1.
if(NOT NANO_EDR_NO_THROWING_OS_HANDLE)
    list(APPEND task2_1_sources ${CMAKE_CURRENT_LIST_DIR}/os_handle_tests.cpp)
endif()

# Тесты окна на списке — отдельной строкой и под условием.
#
# Занятие Ф1 заменяет свои структуры данных стандартными, и односвязный список
# при этом удаляется: с занятия 3.1 окно живёт на кольцевом буфере, а список
# остался в проекте только потому, что его никто не выбросил. Удалённый тип
# нельзя проверять тестами — значит тесты уходят вместе с ним.
#
# Это и есть настоящая цена своего примитива, и она видна именно здесь: пока
# он был, его надо было проверять; когда его не стало, из набора пришлось
# вынимать файл. Стандартный контейнер такого следа не оставляет — у него
# свои тесты, и они не ваши.
#
# Остальные четыре файла набора проверяют то, что никуда не делось (событие,
# разбор, условия, обёртка над границей), и на Ф1 нужны ровно так же, как
# на 2.1: они и есть страховка от того, что рефакторинг задел чужое.
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
