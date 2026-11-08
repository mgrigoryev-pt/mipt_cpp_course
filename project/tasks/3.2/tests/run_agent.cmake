# Запуск агента: код завершения и, если задан эталон, строки [CTX] и [DETECT].
# Тот же скрипт, что в наборе 3.1, и ещё один режим — сверка файла --out.
#
# Сравниваются только строки [CTX] и [DETECT], целиком и по порядку: так
# видны цепочка в детекте, подавленные дубликаты и то, что контекст стоит
# перед детектами своего события. Сводка пропускается: её формулировки ваши.
#
# Аргументы передаются по одному — ARG1, ARG2, ARG3, — а не списком: точка
# с запятой внутри аргумента add_test теряется по дороге до скрипта.
#
# Ожидает: EXE, CODE. Необязательно: ARG1..ARG3; EXPECTED — эталон строк
# [CTX] и [DETECT]; ONLY_DETECTS — в stdout нет ничего, кроме строк [DETECT];
# OUT_FILE — файл, который агент пишет по --out: в нём ровно строки [DETECT]
# из stdout, в том же порядке.

if(DEFINED OUT_FILE)
    file(REMOVE "${OUT_FILE}")
endif()

set(args "")
foreach(i RANGE 1 3)
    if(DEFINED ARG${i})
        list(APPEND args "${ARG${i}}")
    endif()
endforeach()

# ENCODING UTF8 обязателен, и без него случай падает только на Windows —
# причина та же, что в run_case.cmake набора 1.1.
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

# Текст в список строк — заменой перевода строки на точку с запятой. Поэтому
# сами точки с запятой снимаются заранее: в строках [CTX] и [DETECT] их
# не бывает, а в отброшенных строках они делили бы список не там, где надо.
function(output_lines out text pattern)
    string(REPLACE ";" "," text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    set(kept "")
    foreach(line IN LISTS lines)
        if(line MATCHES "${pattern}")
            list(APPEND kept "${line}")
        endif()
    endforeach()
    set(${out} "${kept}" PARENT_SCOPE)
endfunction()

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

if(DEFINED OUT_FILE)
    if(NOT EXISTS "${OUT_FILE}")
        message(FATAL_ERROR
            "агент не создал файл ${OUT_FILE}.\n"
            "аргументы: ${shown}")
    endif()
    file(READ "${OUT_FILE}" written)
    string(REPLACE "\r\n" "\n" written "${written}")
    output_lines(file_lines "${written}" "^\\[DETECT\\] ")
    output_lines(console_lines "${output}" "^\\[DETECT\\] ")
    list(LENGTH file_lines file_count)
    list(LENGTH console_lines console_count)
    if(console_count EQUAL 0)
        message(FATAL_ERROR
            "в stdout нет ни одной строки [DETECT] — сверять файл не с чем.\n"
            "аргументы: ${shown}")
    endif()
    if(NOT file_lines STREQUAL console_lines)
        message(FATAL_ERROR
            "в файле --out не те же строки [DETECT], что в stdout.\n"
            "аргументы: ${shown}\n"
            "в файле строк ${file_count}, в stdout ${console_count}")
    endif()
endif()

if(NOT DEFINED EXPECTED)
    return()
endif()

file(READ "${EXPECTED}" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")

set(pattern "^\\[(CTX|DETECT)\\] ")
output_lines(actual_lines "${output}" "${pattern}")
output_lines(expected_lines "${expected}" "${pattern}")

if(actual_lines STREQUAL expected_lines)
    return()
endif()

list(LENGTH actual_lines actual_count)
list(LENGTH expected_lines expected_count)

set(limit ${actual_count})
if(expected_count LESS limit)
    set(limit ${expected_count})
endif()

# Первое расхождение — на него и стоит смотреть; остальные обычно следствия.
set(first_diff "строки совпадают до конца более короткого списка")
if(limit GREATER 0)
    math(EXPR last "${limit} - 1")
    foreach(i RANGE 0 ${last})
        list(GET actual_lines ${i} got)
        list(GET expected_lines ${i} want)
        if(NOT got STREQUAL want)
            math(EXPR human "${i} + 1")
            set(first_diff "строка ${human}:")
            set(first_diff "${first_diff}\n  ожидалось: ${want}")
            set(first_diff "${first_diff}\n  получено:  ${got}")
            break()
        endif()
    endforeach()
endif()

message(FATAL_ERROR
    "строки [CTX] и [DETECT] не совпали с эталоном.\n"
    "аргументы: ${shown}\n"
    "эталон:    ${EXPECTED}\n"
    "строк получено ${actual_count}, ожидалось ${expected_count}\n"
    "${first_diff}")
