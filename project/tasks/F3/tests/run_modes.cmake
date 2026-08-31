# Один сценарий в двух режимах доставки, и сравнение результата с самим собой.
#
# Это и есть критерий занятия Ф3 дословно: «результат на всех сценариях
# в режиме thread совпадает с режимом inline». Эталона здесь нет и не нужно —
# сравнивается ваш агент с вашим же агентом, и вопрос ставится не «правильно
# ли он работает», а «одинаково ли».
#
# ПОЧЕМУ СРАВНЕНИЕ С СОБОЙ ЛУЧШЕ СРАВНЕНИЯ С ЭТАЛОНОМ
#
# Потому что оно ловит ровно то, ради чего занятие существует, и не ловит
# ничего лишнего. Агент, у которого правила настроены не так, как у автора
# курса, всё равно обязан давать один и тот же ответ в обоих режимах.
#
# ПРОГОНОВ НЕСКОЛЬКО, И ЭТО СУЩЕСТВЕННО
#
# Гонка — не свойство программы, а свойство прогона. Однократное совпадение
# не значит ничего: сломанный агент совпадает в девяти случаях из десяти,
# и именно поэтому его так трудно чинить. Режим thread прогоняется RUNS раз,
# и расхождение хотя бы в одном — провал.
#
# Ожидает: EXE, INLINE_CFG, THREAD_CFG. Необязательно: RUNS (по умолчанию 5).

if(NOT DEFINED RUNS OR RUNS STREQUAL "")
    set(RUNS 5)
endif()

# ENCODING UTF8 обязателен на Windows, см. run_case.cmake.
execute_process(COMMAND "${EXE}" "${INLINE_CFG}" --quiet
                OUTPUT_VARIABLE inline_out
                ERROR_VARIABLE inline_err
                RESULT_VARIABLE inline_rc
                ENCODING UTF8)

if(NOT "${inline_rc}" STREQUAL "0")
    message(FATAL_ERROR
        "агент завершился с кодом ${inline_rc} на сценарии:\n  ${INLINE_CFG}\n"
        "поток ошибок:\n${inline_err}")
endif()

string(REPLACE "\r\n" "\n" inline_out "${inline_out}")

math(EXPR last "${RUNS} - 1")
foreach(run RANGE 0 ${last})
    execute_process(COMMAND "${EXE}" "${THREAD_CFG}" --quiet
                    OUTPUT_VARIABLE thread_out
                    ERROR_VARIABLE thread_err
                    RESULT_VARIABLE thread_rc
                    ENCODING UTF8)

    if(NOT "${thread_rc}" STREQUAL "0")
        message(FATAL_ERROR
            "в режиме thread агент завершился с кодом ${thread_rc}\n"
            "сценарий: ${THREAD_CFG}\n"
            "прогон:   ${run}\n"
            "поток ошибок:\n${thread_err}")
    endif()

    string(REPLACE "\r\n" "\n" thread_out "${thread_out}")

    if(NOT thread_out STREQUAL inline_out)
        # Первая расхождение — на него и стоит смотреть.
        string(REPLACE "\n" ";" thread_lines "${thread_out}")
        string(REPLACE "\n" ";" inline_lines "${inline_out}")
        list(LENGTH thread_lines thread_count)
        list(LENGTH inline_lines inline_count)

        set(limit ${thread_count})
        if(inline_count LESS limit)
            set(limit ${inline_count})
        endif()

        set(first_diff "различие в длине вывода")
        if(limit GREATER 0)
            math(EXPR last_line "${limit} - 1")
            foreach(i RANGE 0 ${last_line})
                list(GET thread_lines ${i} got)
                list(GET inline_lines ${i} want)
                if(NOT got STREQUAL want)
                    math(EXPR human "${i} + 1")
                    set(first_diff
                        "строка ${human}:\n  inline: ${want}\n  thread: ${got}")
                    break()
                endif()
            endforeach()
        endif()

        message(FATAL_ERROR
            "режимы разошлись на прогоне ${run} из ${RUNS}.\n"
            "сценарий: ${INLINE_CFG}\n"
            "строк в thread ${thread_count}, в inline ${inline_count}\n"
            "${first_diff}\n"
            "\n"
            "Если расходятся строки [ACT] с кодами OS_NO_SUCH_PROCESS, "
            "OS_NO_SUCH_FILE или OS_PID_REUSED — вы реагируете слишком поздно. "
            "Симулятор в режиме thread доставляет с опережением, и всё, что "
            "отложено в очередь, разбирается уже после того, как мир изменился.")
    endif()
endforeach()
