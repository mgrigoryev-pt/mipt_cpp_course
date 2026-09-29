# Запуск агента с заданными аргументами и проверка кода завершения.
#
# Аргументы передаются по одному — ARG1, ARG2, ARG3, — а не списком: точка
# с запятой внутри аргумента add_test теряется по дороге до скрипта.
#
# Ожидает: EXE, CODE; ARG1..ARG3 — необязательно.

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

if("${result}" STREQUAL "${CODE}")
    return()
endif()

string(REPLACE ";" " " shown "${args}")
message(FATAL_ERROR
    "код завершения ${result}, ожидался ${CODE}.\n"
    "аргументы: ${shown}\n"
    "поток ошибок:\n${errors}")
