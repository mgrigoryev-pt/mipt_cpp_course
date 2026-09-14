// Разбор текста правил. Занятие 4.2, выдано готовым.
//
// Копируется в src/ и дальше живёт как ваш файл. Интерфейс — rule_text.h,
// там же сказано, почему этот код не входит в задание.
//
// Всё, что здесь возвращается как string_view, смотрит внутрь текста,
// переданного снаружи, и живёт ровно столько же. Наружу из загрузчика
// ни один такой взгляд не выходит: правила владеют своими строками.

#include "rule_text.h"

#include <charconv>
#include <cstddef>
#include <utility>
#include <vector>

#include "event.h"

namespace nano_edr {
namespace {

// Найти разделитель альтернатив — слово `or`, окружённое пробелами и лежащее
// вне кавычек. Возвращает позицию начала слова либо npos.
//
// Вне кавычек — существенно: значение `cmdline contains "a or b"` содержит
// это слово внутри, и резать по нему значило бы получить два бессмысленных
// куска. Разбор, который не знает про кавычки, ломается на первом же пути
// с пробелом.
std::size_t FindOr(std::string_view text) {
    bool quoted = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '"') {
            quoted = !quoted;
            continue;
        }
        if (quoted || i + 4 > text.size()) {
            continue;
        }
        if (text.substr(i, 4) == " or ") {
            return i;
        }
    }
    return std::string_view::npos;
}

// Разобранный атом условия: «поле оператор значение».
struct Atom {
    std::string_view field;
    std::string_view op;
    std::string_view value;
};

bool SplitAtom(std::string_view text, Atom* out) {
    text = Trim(text);
    const std::size_t field_end = text.find(' ');
    if (field_end == std::string_view::npos) {
        return false;
    }
    out->field = text.substr(0, field_end);

    std::string_view rest = Trim(text.substr(field_end));
    const std::size_t op_end = rest.find(' ');
    if (op_end == std::string_view::npos) {
        return false;
    }
    out->op = rest.substr(0, op_end);

    std::string_view value = Trim(rest.substr(op_end));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        // Кавычки снимаются здесь, а не в вызывающем: иначе каждый вызывающий
        // снимал бы их по-своему, и однажды один забыл бы.
        value = value.substr(1, value.size() - 2);
    }
    out->value = value;
    return !out->field.empty() && !out->value.empty();
}

// Условие из одного атома. nullptr означает «оператор неизвестен» —
// диагностику формулирует вызывающий, у него есть номер строки.
std::unique_ptr<ICondition> MakeAtom(const Atom& atom) {
    const std::string field(atom.field);
    const std::string value(atom.value);

    if (atom.op == "equals") {
        return std::make_unique<FieldEquals>(field, value);
    }
    if (atom.op == "contains") {
        return std::make_unique<FieldContains>(field, value);
    }
    if (atom.op == "ends_with") {
        return std::make_unique<FieldEndsWith>(field, value);
    }
    return nullptr;
}

}  // namespace

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' ||
                             text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

bool ParseUint(std::string_view text, uint64_t* out) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    // from_chars, а не stoull: не бросает, не аллоцирует и не смотрит
    // на локаль. Ровно тот инструмент, который нужен разбору конфигурации.
    const std::from_chars_result result = std::from_chars(begin, end, *out);
    return result.ec == std::errc() && result.ptr == end;
}

bool ParseSeverity(std::string_view text, Severity* out) {
    if (text == "low") {
        *out = Severity::kLow;
    } else if (text == "medium") {
        *out = Severity::kMedium;
    } else if (text == "high") {
        *out = Severity::kHigh;
    } else if (text == "critical") {
        *out = Severity::kCritical;
    } else {
        return false;
    }
    return true;
}

// Собрать одно условие из строки: альтернативы через `or`, объединённые ИЛИ.
//
// Особый случай — `type is a or b or c`: из него собирается одно EventTypeIs
// со списком, а не AnyOf из трёх. Результат тот же, устройство ближе
// к правилам из кода, и на горячем пути на два виртуальных вызова меньше.
std::unique_ptr<ICondition> BuildCondition(std::string_view text,
                                           std::string* error) {
    std::vector<Atom> atoms;
    std::string_view rest = text;
    for (;;) {
        const std::size_t at = FindOr(rest);
        std::string_view piece =
            at == std::string_view::npos ? rest : rest.substr(0, at);
        Atom atom;
        if (!SplitAtom(piece, &atom)) {
            *error = "условие не разбирается: " + std::string(Trim(piece));
            return nullptr;
        }
        atoms.push_back(atom);
        if (at == std::string_view::npos) {
            break;
        }
        rest = rest.substr(at + 4);
    }

    // Все альтернативы про тип события — одно условие со списком.
    bool all_types = true;
    for (std::size_t i = 0; i < atoms.size(); ++i) {
        if (atoms[i].field != "type" || atoms[i].op != "is") {
            all_types = false;
            break;
        }
    }
    if (all_types) {
        std::vector<EventType> types;
        for (std::size_t i = 0; i < atoms.size(); ++i) {
            const EventType type =
                EventTypeFromString(atoms[i].value);
            if (type == EventType::kOther && atoms[i].value != "other") {
                *error = "неизвестный тип события: " +
                         std::string(atoms[i].value);
                return nullptr;
            }
            types.push_back(type);
        }
        return std::make_unique<EventTypeIs>(types);
    }

    // Смешивать `type is` с остальными операторами внутри одной строки
    // запрещено, и не из вредности: `type is process_start or image ends_with
    // x` читается двусмысленно, а двусмысленную конфигурацию лучше отвергнуть,
    // чем угадать.
    for (std::size_t i = 0; i < atoms.size(); ++i) {
        if (atoms[i].op == "is") {
            *error = "оператор is применим только к полю type";
            return nullptr;
        }
    }

    std::vector<std::unique_ptr<ICondition>> parts;
    for (std::size_t i = 0; i < atoms.size(); ++i) {
        std::unique_ptr<ICondition> part = MakeAtom(atoms[i]);
        if (part == nullptr) {
            *error = "неизвестный оператор: " + std::string(atoms[i].op);
            return nullptr;
        }
        parts.push_back(std::move(part));
    }

    if (parts.size() == 1) {
        return std::move(parts[0]);
    }
    auto any = std::make_unique<AnyOf>();
    for (std::size_t i = 0; i < parts.size(); ++i) {
        any->Add(std::move(parts[i]));
    }
    return any;
}

}  // namespace nano_edr
