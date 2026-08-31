// Событие: теперь тип с инвариантом, а не набор полей.
//
// Что изменилось с занятия 1.3 и почему. До лекции 4 Event был агрегатом:
// открытые поля, никаких обещаний. Следить за тем, что в `ts` лежит время,
// а в `type` непустая строка, приходилось коду вокруг — то есть никому.
// Теперь у типа есть инвариант, и держит его сам тип:
//
//   ts и type обязательны и непусты — иначе конструктор бросает;
//   после конструирования Event корректен, и проверять это больше негде.
//
// Отсюда исчезает `is_valid()`. У конструктора нет канала для кода возврата
// (лекция 4), поэтому исход ровно один из двух: корректный объект или
// исключение. Полуживых объектов, которые надо проверять после создания,
// в этом коде не бывает.
//
// EventParts остаётся агрегатом, и это не забытый рудимент. Разбор строки —
// операция, которая законно не удаётся: журнал пишет чужой код. Собирать
// из обрывка объект с инвариантом нельзя, поэтому у разбора свой тип-перевозчик
// без обещаний, а Event строится из него уже целиком. Разделение «структура
// для переноса данных» и «тип с инвариантом» — то, ради чего занятие.

#ifndef NANO_EDR_KIT_EVENT_H
#define NANO_EDR_KIT_EVENT_H

#include <compare>
#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <vector>

namespace nano_edr {

// Пара «ключ — значение» из строки журнала.
struct Field {
    std::string key;
    std::string value;
};

// Модельное время в миллисекундах.
//
// Отдельный тип, а не uint64_t, по одной причине: время и размер файла —
// разные вещи, и складывать их друг с другом не должно компилироваться.
// Сравнения даёт `= default`: их шесть, писать руками нечего, а <=> из лекции 5
// делает это одной строкой.
struct Timestamp {
    uint64_t ms = 0;

    auto operator<=>(const Timestamp&) const = default;
    bool operator==(const Timestamp&) const = default;
};

// Части события, как их дал разбор строки. Агрегат без обещаний.
struct EventParts {
    std::string ts;
    std::string type;
    std::string pid;
    std::vector<Field> fields;
};

// Событие с инвариантом.
class Event {
 public:
    // Бросает std::invalid_argument, если ts или type пусты, либо ts
    // не разбирается как число. Иначе объект корректен — и остаётся таким,
    // потому что менять его снаружи нечем.
    explicit Event(const EventParts& parts);

    Timestamp ts() const { return ts_; }
    const std::string& raw_ts() const { return raw_ts_; }
    const std::string& type() const { return type_; }
    const std::string& pid() const { return pid_; }
    const std::vector<Field>& fields() const { return fields_; }

 private:
    Timestamp ts_;
    std::string raw_ts_;   // как было в журнале: для вывода и диагностики
    std::string type_;
    std::string pid_;
    std::vector<Field> fields_;
};

// Печать события. Нужна для `std::print("{}\n", event)` — форматтер ниже
// сводится к ней, чтобы формат был в одном месте.
std::string ToString(const Event& event);

// Вывод в поток — упражнение лекции 5. Возвращает поток, чтобы вызовы
// цеплялись друг за друга. Формат тот же, что у ToString: один формат должен
// быть в одном месте, иначе они разойдутся на первой правке.
std::ostream& operator<<(std::ostream& out, const Event& event);

}  // namespace nano_edr

// Форматтер для std::format и std::print — ВЫДАН ГОТОВЫМ.
//
// Это специализация шаблона, а шаблоны начинаются с лекции 8. Писать её сейчас
// не нужно и не просят: она здесь ровно затем, чтобы печать события через
// std::print работала, как требует критерий занятия. Всё, что от вас нужно, —
// написать ToString.
//
// Заодно видно, зачем классу приватные поля: печать идёт через публичный
// интерфейс, и внутренности можно менять, не трогая ни вывод, ни эту
// специализацию.
template <>
struct std::formatter<nano_edr::Event> : std::formatter<std::string> {
    auto format(const nano_edr::Event& event, std::format_context& ctx) const {
        return std::formatter<std::string>::format(nano_edr::ToString(event), ctx);
    }
};

#endif  // NANO_EDR_KIT_EVENT_H
