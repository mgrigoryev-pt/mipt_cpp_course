# Контекст детекта: запустить агента и сверить строки [CTX] с эталоном.
#
# Зачем этот случай есть. Список и разбор проверяются каждый по своему
# заголовку, и оба набора проходят у того, кто написал оба примитива и не
# подключил к агенту ни один. Окно, которое ведётся правильно и никуда не
# присоединено, — ровно то, что здесь ловится.
#
# Сравниваются только строки [CTX]. Всё остальное — сводка, предупреждения,
# сами детекты — пропускается: формулировки сводки остаются свободными, как
# обещано на занятии 1.1, а детекты сверяет набор 1.1 своим эталоном.
#
# Ожидает: EXE, LOG, EXPECTED. Необязательно: WINDOW (число для
# --window-size), QUIET (любое значение — добавить --quiet).

set(args "${LOG}")
if(DEFINED WINDOW AND NOT WINDOW STREQUAL "")
    list(APPEND args --window-size "${WINDOW}")
endif()
if(DEFINED QUIET AND NOT QUIET STREQUAL "")
    list(APPEND args --quiet)
endif()

# ENCODING UTF8 обязателен, и без него случай падает только на Windows —
# причина та же, что в run_case.cmake набора 1.1.
execute_process(COMMAND "${EXE}" ${args}
                OUTPUT_VARIABLE actual
                ERROR_VARIABLE diagnostics
                RESULT_VARIABLE result
                ENCODING UTF8)

if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR
        "агент завершился с кодом ${result} на журнале:\n  ${LOG}\n"
        "поток ошибок:\n${diagnostics}")
endif()

file(READ "${EXPECTED}" expected)

string(REPLACE "\r\n" "\n" actual "${actual}")
string(REPLACE "\r\n" "\n" expected "${expected}")

# Текст в список строк — заменой перевода строки на точку с запятой. Поэтому
# сами точки с запятой снимаются заранее: в строках [CTX] их не бывает,
# а в отброшенных строках вывода они делили бы список не там, где надо.
# Обе стороны проходят одно и то же преобразование, так что сравнение честное.
function(ctx_lines out text)
    string(REPLACE ";" "," text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    set(kept "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^\\[CTX\\]")
            list(APPEND kept "${line}")
        endif()
    endforeach()
    set(${out} "${kept}" PARENT_SCOPE)
endfunction()

ctx_lines(actual_lines "${actual}")
ctx_lines(expected_lines "${expected}")

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
            set(first_diff "строка контекста ${human}:")
            set(first_diff "${first_diff}\n  ожидалось: ${want}")
            set(first_diff "${first_diff}\n  получено:  ${got}")
            break()
        endif()
    endforeach()
endif()

message(FATAL_ERROR
    "строки [CTX] не совпали с эталоном.\n"
    "журнал:  ${LOG}\n"
    "эталон:  ${EXPECTED}\n"
    "строк получено ${actual_count}, ожидалось ${expected_count}\n"
    "${first_diff}")
