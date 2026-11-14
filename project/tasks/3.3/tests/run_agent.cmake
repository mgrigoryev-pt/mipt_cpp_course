# Запуск агента на занятии 3.3: код завершения, строки детектов и действий,
# при необходимости — сверка изменившегося мира через sim_verify.
#
# Сравниваются строки [DETECT], [ACT] и [PLAN], целиком и по порядку: так
# видно, что действие идёт за своим детектом, что повтор не исполнен и что
# под --dry-run не исполнено ничего. Сводка пропускается: её формулировки
# ваши, кроме строки, которую случай проверяет через LINE.
#
# Аргументы передаются по одному — ARG1, ARG2, ARG3, — а не списком: точка
# с запятой внутри аргумента add_test теряется по дороге до скрипта.
#
# Ожидает: EXE, CODE. Необязательно: ARG1..ARG3; EXPECTED — эталон строк
# [DETECT], [ACT] и [PLAN]; LINE — строка, которая обязана найтись в stdout
# целиком; VERIFY и CONFIG — программа
# sim_verify и конфигурация сценария: после агента sim_verify обязан принять
# сценарий.

# Пустые строки вывода — тоже элементы списка: иначе list(FIND) их теряет
# и предупреждает.
cmake_policy(SET CMP0007 NEW)

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
# сами точки с запятой снимаются заранее: в сравниваемых строках их не бывает.
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

if(DEFINED LINE)
    string(REPLACE ";" "," text "${output}")
    string(REPLACE "\n" ";" all_lines "${text}")
    list(FIND all_lines "${LINE}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "в stdout нет строки «${LINE}».\n"
            "аргументы: ${shown}")
    endif()
endif()

if(DEFINED EXPECTED)
    file(READ "${EXPECTED}" expected)
    string(REPLACE "\r\n" "\n" expected "${expected}")
    set(pattern "^\\[(DETECT|ACT|PLAN)\\] ")
    output_lines(actual_lines "${output}" "${pattern}")
    output_lines(expected_lines "${expected}" "${pattern}")
    if(NOT actual_lines STREQUAL expected_lines)
        list(LENGTH actual_lines actual_count)
        list(LENGTH expected_lines expected_count)
        set(limit ${actual_count})
        if(expected_count LESS limit)
            set(limit ${expected_count})
        endif()
        # Первое расхождение — на него и стоит смотреть; остальные обычно
        # следствия.
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
            "строки [DETECT], [ACT] и [PLAN] не совпали с эталоном.\n"
            "аргументы: ${shown}\n"
            "эталон:    ${EXPECTED}\n"
            "строк получено ${actual_count}, ожидалось ${expected_count}\n"
            "${first_diff}")
    endif()
endif()

if(DEFINED VERIFY)
    execute_process(COMMAND "${VERIFY}" "${CONFIG}"
                    OUTPUT_VARIABLE report
                    ERROR_VARIABLE report_errors
                    RESULT_VARIABLE verdict
                    ENCODING UTF8)
    if(NOT verdict EQUAL 0)
        message(FATAL_ERROR
            "sim_verify не принял сценарий.\n"
            "аргументы агента: ${shown}\n"
            "отчёт sim_verify:\n${report}${report_errors}")
    endif()
endif()
