# Прогон одного случая: запустить агента на журнале и сравнить вывод с эталоном.
#
# Скрипт, а не встроенный в CMake механизм, по двум причинам. Первая: сравнение
# нужно с нормализацией переводов строк — на Windows stdout в текстовом режиме
# отдаёт CRLF, и без нормализации тест падал бы на ровном месте. Вторая: когда
# вывод не совпал, полезно увидеть первую разошедшуюся строку, а не «test failed».
#
# Ожидает: EXE, LOG, EXPECTED.

# ENCODING UTF8 обязателен, и без него тест падает только на Windows: там CMake
# по умолчанию считает, что потомок пишет в кодировке консоли, перекодирует
# вывод из неё — и сравнение с эталоном не проходит на любой не-ASCII строке.
# На Linux параметр не делает ничего.
execute_process(COMMAND "${EXE}" "${LOG}" --quiet
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

if(actual STREQUAL expected)
    return()
endif()

# Первая расхождение — на него и стоит смотреть; остальные обычно следствия.
string(REPLACE "\n" ";" actual_lines "${actual}")
string(REPLACE "\n" ";" expected_lines "${expected}")
list(LENGTH actual_lines actual_count)
list(LENGTH expected_lines expected_count)

set(first_diff "")
set(limit ${actual_count})
if(expected_count LESS limit)
    set(limit ${expected_count})
endif()

math(EXPR last "${limit} - 1")
foreach(i RANGE 0 ${last})
    list(GET actual_lines ${i} got)
    list(GET expected_lines ${i} want)
    if(NOT got STREQUAL want)
        math(EXPR human "${i} + 1")
        set(first_diff "строка вывода ${human}:\n  ожидалось: ${want}\n  получено:  ${got}")
        break()
    endif()
endforeach()

if(first_diff STREQUAL "")
    set(first_diff "строки совпадают до конца более короткого вывода — различие в длине")
endif()

message(FATAL_ERROR
    "вывод не совпал с эталоном.\n"
    "журнал:  ${LOG}\n"
    "эталон:  ${EXPECTED}\n"
    "строк получено ${actual_count}, ожидалось ${expected_count}\n"
    "${first_diff}")
