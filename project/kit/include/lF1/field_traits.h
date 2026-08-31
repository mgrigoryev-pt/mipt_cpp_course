// Типизированный доступ к полям. Занятие 3.1.
//
// Задача мелкая, а показывает много. Поля события — строки, и до сих пор
// каждый вызывающий сам решал, что с ними делать: `FindField` отдавал строку,
// `GetIntField` — число, и на каждый новый тип понадобилась бы новая функция
// с новым именем.
//
// Специализация шаблона убирает эту очередь имён. Тип задаётся вызывающим:
//
//     std::string image;
//     GetField<std::string>(event, "image", &image);
//
//     uint64_t pid = 0;
//     GetField<uint64_t>(event, "pid", &pid);
//
// Одно имя, разные типы, и добавление нового типа — новая специализация,
// не новая функция.
//
// Канал отказа не изменился и меняться не должен: `false`, если поля нет или
// оно не разбирается. Битые внешние данные — обычное дело, а не исключение.
//
// Обратите внимание: у `FieldTraits` нет общего определения, только
// специализации. Это осознанно. Попытка взять поле как тип, для которого
// разбора не написано, должна не собираться, а не молча делать что-то похожее
// на правду.

#ifndef NANO_EDR_KIT_FIELD_TRAITS_H
#define NANO_EDR_KIT_FIELD_TRAITS_H

#include <cstdint>
#include <string>

#include "event.h"
#include "fields.h"

namespace nano_edr {

// Объявлен, но не определён: только специализации ниже.
template <typename T>
struct FieldTraits;

template <>
struct FieldTraits<std::string> {
    // Строка берётся как есть — разбирать нечего.
    static bool Parse(const std::string& text, std::string* out);
};

template <>
struct FieldTraits<uint64_t> {
    // Только десятичные цифры, без знака, с проверкой переполнения.
    static bool Parse(const std::string& text, uint64_t* out);
};

template <>
struct FieldTraits<bool> {
    // Телеметрия пишет флаги по-разному: 1/0, true/false, yes/no.
    // Разбирать это в одном месте лучше, чем в каждом вызывающем.
    static bool Parse(const std::string& text, bool* out);
};

// Значение поля нужного типа. false — поля нет либо оно не разбирается.
template <typename T>
bool GetField(const Event& event, const std::string& key, T* out) {
    const std::string* raw = FindField(event, key);
    if (raw == nullptr) {
        return false;
    }
    return FieldTraits<T>::Parse(*raw, out);
}

// Со значением по умолчанию — для полей, отсутствие которых осмысленно.
template <typename T>
T GetFieldOr(const Event& event, const std::string& key, T fallback) {
    T value = fallback;
    if (GetField<T>(event, key, &value)) {
        return value;
    }
    return fallback;
}

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_FIELD_TRAITS_H
