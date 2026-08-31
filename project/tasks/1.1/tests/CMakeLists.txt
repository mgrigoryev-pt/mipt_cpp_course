# Тесты, выдаваемые на занятии 1.1.
#
# Подключаются ключом -D NANO_EDR_LESSON при конфигурации — см. README.md
# рядом. Проверяют ровно то, что сказано в критерии «Готово, когда»:
# на трёх сценариях вывод детектов совпадает с эталоном.
#
# Правьте этот файл, только если понимаете, зачем: он же прогоняется при сдаче.

# Цель агента ищется по имени: nano-edr в комплекте студента, nano_edr —
# в решениях. Если ваша называется иначе, передайте своё имя до подключения:
#
#     set(NANO_EDR_AGENT my_agent)
if(NOT DEFINED NANO_EDR_AGENT)
    if(TARGET nano-edr)
        set(NANO_EDR_AGENT nano-edr)
    else()
        set(NANO_EDR_AGENT nano_edr)
    endif()
endif()

if(NOT TARGET ${NANO_EDR_AGENT})
    message(FATAL_ERROR
        "Не найдена цель '${NANO_EDR_AGENT}' — тесты не знают, что запускать.\n"
        "Назовите цель агента nano-edr либо передайте своё имя:\n"
        "  set(NANO_EDR_AGENT my_agent)")
endif()

# Журналы лежат в scenarios/ этого же репозитория, тремя уровнями выше.
get_filename_component(NANO_EDR_KIT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

set(NANO_EDR_TASK11_CASES phishing_macro ransomware clean_office)

foreach(case IN LISTS NANO_EDR_TASK11_CASES)
    add_test(NAME task1.1.${case}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:${NANO_EDR_AGENT}>
                     -DLOG=${NANO_EDR_KIT_ROOT}/scenarios/${case}.log
                     -DEXPECTED=${CMAKE_CURRENT_LIST_DIR}/expected/${case}.out
                     -P ${CMAKE_CURRENT_LIST_DIR}/run_case.cmake)
endforeach()
