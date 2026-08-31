cmake_minimum_required(VERSION 3.21)

project(nano_edr_agent
        VERSION 0.1.0
        DESCRIPTION "Агент EDR — практикум курса C++ ВШПИ МФТИ"
        LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Стандарт и компилятор
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NANO_EDR_HINT "Курс идёт на C++23. Нужен g++ 14+, clang 19+ или MSVC 19.40+ \
(Visual Studio 2022 17.10+).")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    message(FATAL_ERROR "g++ ${CMAKE_CXX_COMPILER_VERSION} слишком старый. ${NANO_EDR_HINT}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
    message(FATAL_ERROR "clang ${CMAKE_CXX_COMPILER_VERSION} слишком старый. ${NANO_EDR_HINT}")
elseif(MSVC AND MSVC_VERSION LESS 1940)
    message(FATAL_ERROR "MSVC ${MSVC_VERSION} слишком старый. ${NANO_EDR_HINT}")
endif()

# ---------------------------------------------------------------------------
# Настройки занятия
# ---------------------------------------------------------------------------
#
# Ключ один — номер занятия:
#
#     cmake --preset windows -D NANO_EDR_LESSON=1.3
#
# Из него выводится всё остальное: какой комплект заготовок попадает
# на include-путь и какие наборы выданных тестов собираются. Таблица лежит
# в tasks/lessons.cmake; там же написано, почему набор прошлого занятия
# иногда уходит, а не копится дальше.
#
# Значение запоминается в кэше сборки, поэтому набирается один раз за занятие,
# а не при каждой конфигурации.

set(NANO_EDR_LESSON "" CACHE STRING "Занятие: 1.1, 1.2, 1.3, 2.1, ..., F3")

# Переопределения для преподавателя. Заданные руками, они выигрывают
# у выведенных из занятия: собрать комплект против чужого набора тестов бывает
# нужно, а придумывать для этого несуществующий номер занятия — плохой способ.
set(NANO_EDR_KIT   "" CACHE STRING "Комплект заготовок вручную: l1.2, l2.1, ...")
set(NANO_EDR_TASKS "" CACHE STRING "Наборы тестов вручную, через ';': 1.2;1.3")

# Все исполняемые файлы — в один каталог сборки, включая программы выданных
# тестов. Иначе каждая из них лежала бы в своём подкаталоге, а os.dll и edr.dll
# копируются рядом с агентом: на Windows тест, который трогает границу,
# не нашёл бы библиотеку и не запустился. Многоконфигурационный генератор
# (Visual Studio) дописывает сюда профиль сам, поэтому путь остаётся
# build/windows/Release/, как и был.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

option(NANO_EDR_WERROR   "Считать предупреждения ошибками" OFF)
option(NANO_EDR_SANITIZE "Собрать с ASan и UBSan"          OFF)

# ---------------------------------------------------------------------------
# Выключатели устаревших проверок
# ---------------------------------------------------------------------------
#
# Наборы прошлых занятий подключаются вместе с нынешним: рефакторинг, задевший
# чужое, обязан провалить их. Но два файла в наборе 2.1 проверяют контракт,
# который позднейшие занятия заменяют по заданию, и после замены перестают
# компилироваться.
#
# Набирать эти ключи не нужно — их включает номер занятия. Открыты они потому,
# что преподавателю бывает нужно собрать набор в необычном сочетании.

option(NANO_EDR_NO_EVENT_LIST
       "Не собирать тесты односвязного списка (с занятия Ф1)" OFF)
option(NANO_EDR_NO_THROWING_OS_HANDLE
       "Не собирать тесты бросающего конструктора OsHandle (с занятия Ф2)" OFF)

# ---------------------------------------------------------------------------
# Разрешение номера занятия
# ---------------------------------------------------------------------------

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tasks/lessons.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/tasks/lessons.cmake")
    if(NANO_EDR_LESSON)
        nano_edr_resolve_lesson("${NANO_EDR_LESSON}")
    endif()
endif()

if(NOT NANO_EDR_LESSON AND NOT NANO_EDR_KIT AND NOT NANO_EDR_TASKS)
    message(STATUS
        "занятие не указано: собирается только агент, без заготовок и тестов. "
        "Номер занятия — ключом -D NANO_EDR_LESSON=1.1")
endif()

# ---------------------------------------------------------------------------
# Санитайзеры
# ---------------------------------------------------------------------------
#
# Флаги ставятся на весь каталог, а не на отдельные цели: ASan обязан
# покрывать и ваш код, и выданные тесты. Инструментированный код, слинкованный
# с неинструментированным, собирается — и молча теряет часть проверок.
#
# Готовые os.dll и edr.dll собраны без ASan, и это не мешает: перехват
# выделений памяти в ASan работает на весь процесс, поэтому обращение к строкам
# события после возврата из колбэка он поймает и там. А если не поймает —
# библиотека всё равно затирает буфер мусором 0xDD, так что неправильный код
# не будет выглядеть работающим.

if(NANO_EDR_SANITIZE)
    if(MSVC)
        # У MSVC есть ASan, но нет UBSan: часть проверок доступна только
        # на GCC и clang. Инкрементальная компоновка с ASan несовместима,
        # поэтому выключается здесь же.
        add_compile_options(/fsanitize=address)
        add_link_options(/INCREMENTAL:NO)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# ---------------------------------------------------------------------------
# Предупреждения
# ---------------------------------------------------------------------------
#
# -Wall -Wextra включены всегда: приёмка занятий требует чистой сборки под ними.
# Ошибками они не считаются по умолчанию — на середине задания это мешает
# отлаживать. Перед сдачей включите NANO_EDR_WERROR и убедитесь, что чисто:
# это дешевле, чем узнать о предупреждении на проверке.

add_library(nano_edr_warnings INTERFACE)
if(MSVC)
    target_compile_options(nano_edr_warnings INTERFACE /W4 /permissive- /utf-8)
    if(NANO_EDR_WERROR)
        target_compile_options(nano_edr_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(nano_edr_warnings INTERFACE -Wall -Wextra)
    if(NANO_EDR_WERROR)
        target_compile_options(nano_edr_warnings INTERFACE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# Границы: готовые библиотеки os и edr
# ---------------------------------------------------------------------------

include(cmake/nano_edr_libs.cmake)

# ---------------------------------------------------------------------------
# Ваш код
# ---------------------------------------------------------------------------
#
# Файлы подбираются маской: новый .cpp в src/ не требует правки этого файла,
# достаточно пересобрать. CONFIGURE_DEPENDS заставляет CMake замечать
# появление файлов самому.

file(GLOB_RECURSE NANO_EDR_CORE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM NANO_EDR_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Имя nano_edr_core не произвольное: выданные тесты линкуются именно с ним.
#
# Пока в src/ нет ничего, кроме main.cpp — а на занятии 1.1 так и есть, —
# компилировать в библиотеку нечего, и она объявляется INTERFACE: цель
# существует и раздаёт пути и границы, но своих объектных файлов не имеет.
# Первый же ваш .cpp рядом с main.cpp делает её обычной статической.
if(NANO_EDR_CORE_SOURCES)
    add_library(nano_edr_core STATIC ${NANO_EDR_CORE_SOURCES})
    set(NANO_EDR_CORE_SCOPE PUBLIC)
    target_link_libraries(nano_edr_core PRIVATE nano_edr_warnings)
else()
    add_library(nano_edr_core INTERFACE)
    set(NANO_EDR_CORE_SCOPE INTERFACE)
endif()

target_include_directories(nano_edr_core ${NANO_EDR_CORE_SCOPE}
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(NANO_EDR_KIT)
    if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/kit/include/${NANO_EDR_KIT}")
        message(FATAL_ERROR
            "Нет комплекта '${NANO_EDR_KIT}'. Доступные — каталоги в kit/include/.")
    endif()
    target_include_directories(nano_edr_core ${NANO_EDR_CORE_SCOPE}
        "${CMAKE_CURRENT_SOURCE_DIR}/kit/include/${NANO_EDR_KIT}")
endif()

# Границы раздаются наружу, а не держатся внутри: os.h и edr.h нужны и вашему
# коду, и тестам, которые с ним линкуются.
target_link_libraries(nano_edr_core ${NANO_EDR_CORE_SCOPE} os edr)

add_executable(nano-edr "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
target_link_libraries(nano-edr PRIVATE nano_edr_core nano_edr_warnings)
nano_edr_copy_runtime(nano-edr)

# Пример из examples/: полный цикл работы с os.h, тридцать семь строк.
# Собирается отдельной целью, чтобы его можно было не только прочитать,
# но и запустить. Понадобится на занятии 2.1.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal_agent.cpp")
    add_executable(minimal_agent
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal_agent.cpp")
    target_link_libraries(minimal_agent PRIVATE os nano_edr_warnings)
    nano_edr_copy_runtime(minimal_agent)
endif()

# ---------------------------------------------------------------------------
# Выданные тесты
# ---------------------------------------------------------------------------

if(NANO_EDR_TASKS)
    enable_testing()
    foreach(task IN LISTS NANO_EDR_TASKS)
        if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tasks/${task}/tests")
            message(FATAL_ERROR
                "Нет набора тестов '${task}'. Доступные — каталоги в tasks/.")
        endif()
        string(REPLACE "." "_" task_dir "task${task}")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tasks/${task}/tests" "${task_dir}")
    endforeach()
    message(STATUS "выданные тесты: ${NANO_EDR_TASKS}")
endif()

if(NANO_EDR_LESSON)
    message(STATUS "занятие: ${NANO_EDR_LESSON}")
endif()
message(STATUS "комплект занятия: ${NANO_EDR_KIT}")
message(STATUS "границы: ${NANO_EDR_PLATFORM}, готовые библиотеки из lib/${NANO_EDR_PLATFORM}")
