# Прогон одного журнала через оба источника: через os.h и с --file.
#
# Сначала два вывода под --quiet сравниваются друг с другом — это главный
# критерий занятия, и эталон для него не нужен. Затем, если передан EXPECTED,
# вывод сравнивается с эталоном: так проверяются и пять правил.
#
# Ожидает: EXE, LOG; EXPECTED — необязательно.

# Первая разошедшаяся строка двух текстов — на неё и стоит смотреть;
# остальные расхождения обычно следствия.
function(nano_edr_first_diff out left_name left right_name right)
    string(REPLACE "\n" ";" left_lines "${left}")
    string(REPLACE "\n" ";" right_lines "${right}")
    list(LENGTH left_lines left_count)
    list(LENGTH right_lines right_count)

    set(limit ${left_count})
    if(right_count LESS limit)
        set(limit ${right_count})
    endif()

    set(result "")
    if(limit GREATER 0)
        math(EXPR last "${limit} - 1")
        foreach(i RANGE 0 ${last})
            list(GET left_lines ${i} l)
            list(GET right_lines ${i} r)
            if(NOT l STREQUAL r)
                math(EXPR human "${i} + 1")
                string(CONCAT result "строка вывода ${human}:\n"
                                     "  ${left_name}: ${l}\n"
                                     "  ${right_name}: ${r}")
                break()
            endif()
        endforeach()
    endif()

    if(result STREQUAL "")
        string(CONCAT result
               "строки совпадают до конца более короткого вывода — "
               "различие в длине")
    endif()
    string(CONCAT summary
           "строк ${left_name}: ${left_count}, ${right_name}: ${right_count}\n"
           "${result}")
    set(${out} "${summary}" PARENT_SCOPE)
endfunction()

# ENCODING UTF8 обязателен по той же причине, что в наборе 1.1: без него
# на Windows CMake перекодирует вывод из кодировки консоли.
execute_process(COMMAND "${EXE}" "${LOG}" --quiet
                OUTPUT_VARIABLE via_os
                ERROR_VARIABLE via_os_errors
                RESULT_VARIABLE via_os_result
                ENCODING UTF8)

if(NOT "${via_os_result}" STREQUAL "0")
    message(FATAL_ERROR
        "источник os.h: агент завершился с кодом ${via_os_result} на журнале:\n"
        "  ${LOG}\n"
        "поток ошибок:\n${via_os_errors}")
endif()

execute_process(COMMAND "${EXE}" --quiet --file "${LOG}"
                OUTPUT_VARIABLE via_file
                ERROR_VARIABLE via_file_errors
                RESULT_VARIABLE via_file_result
                ENCODING UTF8)

if(NOT "${via_file_result}" STREQUAL "0")
    message(FATAL_ERROR
        "источник --file: агент завершился с кодом ${via_file_result}\n"
        "на журнале:\n"
        "  ${LOG}\n"
        "поток ошибок:\n${via_file_errors}")
endif()

string(REPLACE "\r\n" "\n" via_os "${via_os}")
string(REPLACE "\r\n" "\n" via_file "${via_file}")

if(NOT via_os STREQUAL via_file)
    nano_edr_first_diff(details "os.h" "${via_os}" "--file" "${via_file}")
    message(FATAL_ERROR
        "два источника дали разный вывод под --quiet.\n"
        "журнал: ${LOG}\n"
        "${details}")
endif()

if(NOT DEFINED EXPECTED)
    return()
endif()

file(READ "${EXPECTED}" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")

if(NOT via_os STREQUAL expected)
    nano_edr_first_diff(details
                        "получено" "${via_os}" "ожидалось" "${expected}")
    message(FATAL_ERROR
        "вывод не совпал с эталоном; оба источника при этом совпадают друг\n"
        "с другом, так что дело в правилах или в строке детекта.\n"
        "журнал: ${LOG}\n"
        "эталон: ${EXPECTED}\n"
        "${details}")
endif()
