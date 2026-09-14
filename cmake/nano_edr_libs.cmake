# Подключение готовых библиотек границ: os и edr.
#
# Библиотеки поставляются собранными; всё, что о них нужно знать, — в
# include/os.h, include/edr.h и SPEC.md.

if(WIN32)
    set(NANO_EDR_PLATFORM win-x64)
elseif(UNIX AND NOT APPLE)
    set(NANO_EDR_PLATFORM linux-x64)
else()
    message(FATAL_ERROR
        "Готовых библиотек для этой системы (${CMAKE_SYSTEM_NAME}) в комплекте нет: "
        "собраны win-x64 и linux-x64. Напишите преподавателю — сборка под вашу "
        "платформу это один прогон скрипта, а не переделка задания.")
endif()

set(NANO_EDR_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(NANO_EDR_LIB_DIR "${NANO_EDR_ROOT}/lib/${NANO_EDR_PLATFORM}")

if(WIN32)
    set(NANO_EDR_OS_BIN  "${NANO_EDR_LIB_DIR}/os.dll")
    set(NANO_EDR_OS_LINK "${NANO_EDR_LIB_DIR}/os.lib")
    set(NANO_EDR_EDR_BIN  "${NANO_EDR_LIB_DIR}/edr.dll")
    set(NANO_EDR_EDR_LINK "${NANO_EDR_LIB_DIR}/edr.lib")
else()
    set(NANO_EDR_OS_BIN  "${NANO_EDR_LIB_DIR}/libos.so")
    set(NANO_EDR_EDR_BIN "${NANO_EDR_LIB_DIR}/libedr.so")
endif()

foreach(f "${NANO_EDR_OS_BIN}" "${NANO_EDR_EDR_BIN}")
    if(NOT EXISTS "${f}")
        message(FATAL_ERROR "Не найдена библиотека: ${f}\nКомплект неполный — перекачайте репозиторий.")
    endif()
endforeach()

# IMPORTED-цели, а не голые пути в target_link_libraries: тогда заголовки,
# определение OS_USE_SHARED и сама библиотека приезжают одним требованием,
# и забыть половину невозможно.

add_library(os SHARED IMPORTED GLOBAL)
set_target_properties(os PROPERTIES IMPORTED_LOCATION "${NANO_EDR_OS_BIN}")
target_include_directories(os INTERFACE "${NANO_EDR_ROOT}/include")
target_compile_definitions(os INTERFACE OS_USE_SHARED)

add_library(edr SHARED IMPORTED GLOBAL)
set_target_properties(edr PROPERTIES IMPORTED_LOCATION "${NANO_EDR_EDR_BIN}")
target_include_directories(edr INTERFACE "${NANO_EDR_ROOT}/include")
target_compile_definitions(edr INTERFACE EDR_USE_SHARED)

if(WIN32)
    set_target_properties(os  PROPERTIES IMPORTED_IMPLIB "${NANO_EDR_OS_LINK}")
    set_target_properties(edr PROPERTIES IMPORTED_IMPLIB "${NANO_EDR_EDR_LINK}")
endif()

# Библиотека ASan из состава MSVC. Без неё собранный с /fsanitize=address
# исполняемый файл не стартует вовсе: загрузчик не находит DLL и возвращает
# 0xC0000135, а сообщения нет никакого. Visual Studio добавляет её в PATH
# при запуске из среды, но из обычной оболочки этого не происходит, поэтому
# библиотека кладётся рядом с exe так же, как os и edr.
if(NANO_EDR_SANITIZE AND MSVC)
    get_filename_component(NANO_EDR_MSVC_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    file(GLOB NANO_EDR_ASAN_RUNTIME
         "${NANO_EDR_MSVC_BIN}/clang_rt.asan_dynamic-*.dll")
    if(NOT NANO_EDR_ASAN_RUNTIME)
        message(FATAL_ERROR
            "Не найдена библиотека ASan рядом с cl.exe (${NANO_EDR_MSVC_BIN}).\n"
            "В установщике Visual Studio нужен компонент «C++ AddressSanitizer».")
    endif()
endif()

# Чтобы агент запускался из любого каталога, а не только из каталога сборки.
#
# На Windows DLL ищется рядом с exe, поэтому её туда надо положить: этим
# занимается nano_edr_copy_runtime() ниже. На ELF путь к библиотеке зашивается
# в сам исполняемый файл через RPATH, и копировать ничего не нужно.
#
# Достаточно вызвать функцию для одной цели: все исполняемые файлы проекта,
# включая программы выданных тестов, собираются в один каталог.
function(nano_edr_copy_runtime target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:os>" "$<TARGET_FILE:edr>"
                    ${NANO_EDR_ASAN_RUNTIME}
                    "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Кладу os.dll и edr.dll рядом с ${target}")
    else()
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "${NANO_EDR_LIB_DIR}")
    endif()
endfunction()
