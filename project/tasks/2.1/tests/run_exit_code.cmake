# Запуск агента с заданными аргументами и проверка кода завершения.
#
# Аргументы передаются по одному — ARG1, ARG2, ARG3, — а не списком: точка
# с запятой внутри аргумента add_test теряется по дороге до скрипта.
#
# Ожидает: EXE, CODE; ARG1..ARG3 — необязательно.
# ONLY_DETECTS — в stdout не должно быть ничего, кроме строк [DETECT].
# EXPECTED — stdout должен совпасть с эталоном.

set(args "")
foreach(i RANGE 1 3)
    if(DEFINED ARG${i})
        list(APPEND args "${ARG${i}}")
    endif()
endforeach()

execute_process(COMMAND "${EXE}" ${args}
                OUTPUT_VARIABLE output
                ERROR_VARIABLE errors
                RESULT_VARIABLE result
                ENCODING UTF8)

string(REPLACE ";" " " shown "${args}")

if(NOT "${result}" STREQUAL "${CODE}")
    message(FATAL_ERROR
        "код завершения ${result}, ожидался ${CODE}.\n"
        "аргументы: ${shown}\n"
        "поток ошибок:\n${errors}")
endif()

string(REPLACE "\r\n" "\n" output "${output}")

if(ONLY_DETECTS)
    string(REPLACE "\n" ";" lines "${output}")
    foreach(line IN LISTS lines)
        if(NOT line STREQUAL "" AND NOT line MATCHES "^\\[DETECT\\] ")
            message(FATAL_ERROR
                "под --quiet в stdout попала строка, которая не детект:\n"
                "  ${line}\n"
                "аргументы: ${shown}")
        endif()
    endforeach()
endif()

if(DEFINED EXPECTED)
    file(READ "${EXPECTED}" expected)
    string(REPLACE "\r\n" "\n" expected "${expected}")
    if(NOT output STREQUAL expected)
        message(FATAL_ERROR
            "вывод не совпал с эталоном.\n"
            "аргументы: ${shown}\n"
            "эталон: ${EXPECTED}\n"
            "получено:\n${output}\n"
            "ожидалось:\n${expected}")
    endif()
endif()
