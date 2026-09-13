cmake_minimum_required(VERSION 3.21)
project(nano_edr_macos LANGUAGES CXX)

# Native adapter for the first part of the course. The distributed project/
# build and tests remain the source of truth for the full simulator project.
if(NOT APPLE OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    message(FATAL_ERROR "Этот адаптер предназначен для macOS ARM64. Для Windows/Linux используйте project/.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(NANO_EDR_LESSON "1.1" CACHE STRING "Занятие: 1.1, 1.2 или 1.3")
set_property(CACHE NANO_EDR_LESSON PROPERTY STRINGS 1.1 1.2 1.3)
option(NANO_EDR_WERROR "Считать предупреждения ошибками" OFF)
option(NANO_EDR_SANITIZE "Включить ASan и UBSan" OFF)

if(NOT NANO_EDR_LESSON MATCHES "^1\\.[123]$")
    message(FATAL_ERROR
        "Нативная сборка доступна для занятий 1.1–1.3. Начиная с 2.1 нужны "
        "библиотеки os/edr для macOS ARM64; комплект содержит только Windows/Linux x64.")
endif()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#include <print>\nint main() { std::print(\"{}\", 23); }"
    NANO_EDR_HAS_STD_PRINT)
if(NOT NANO_EDR_HAS_STD_PRINT)
    message(FATAL_ERROR "Нужны компилятор и стандартная библиотека с C++23 std::print. Проверено с Apple Clang 21.")
endif()

add_compile_options(-Wall -Wextra)
if(NANO_EDR_WERROR)
    add_compile_options(-Werror)
endif()
if(NANO_EDR_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

set(course "${CMAKE_CURRENT_SOURCE_DIR}/project")
include("${course}/tasks/lessons.cmake")
nano_edr_resolve_lesson("${NANO_EDR_LESSON}")

file(GLOB_RECURSE core_sources CONFIGURE_DEPENDS "${course}/src/*.cpp")
list(REMOVE_ITEM core_sources "${course}/src/main.cpp")
if(core_sources)
    add_library(nano_edr_core STATIC ${core_sources})
    set(core_scope PUBLIC)
else()
    add_library(nano_edr_core INTERFACE)
    set(core_scope INTERFACE)
endif()
target_include_directories(nano_edr_core ${core_scope} "${course}/src")
if(NANO_EDR_KIT)
    target_include_directories(nano_edr_core ${core_scope}
        "${course}/kit/include/${NANO_EDR_KIT}")
endif()

add_executable(nano-edr "${course}/src/main.cpp")
target_link_libraries(nano-edr PRIVATE nano_edr_core)

enable_testing()
foreach(task IN LISTS NANO_EDR_TASKS)
    add_subdirectory("${course}/tasks/${task}/tests" "task${task}")
endforeach()

add_custom_target(toolchain ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${course}/compile_commands.json"
    COMMENT "macOS ARM64: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}, занятие ${NANO_EDR_LESSON}")
add_dependencies(nano-edr toolchain)
