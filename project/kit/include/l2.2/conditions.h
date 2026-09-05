// Условия как функторы. Занятие 2.1.
//
// Условие — объект с operator(), отвечающий «да» или «нет» об одном событии.
// Четыре штуки, и этого набора хватит, чтобы на занятии 2.2 собрать из них
// правила, не написав ни одного нового условия.
//
// Почему функтор, а не функция. У функции нет состояния: «поле path
// заканчивается на .locked» пришлось бы писать отдельной функцией для каждого
// суффикса. У функтора состояние есть — суффикс лежит в поле, — и одно и то же
// условие настраивается под любой суффикс. Это ровно тот шаг от функции
// к объекту, ради которого читалась лекция 5, и на занятии 3.2 он же приведёт
// к лямбдам: лямбда и есть автогенерируемый функтор.
//
// Все условия сравнивают без учёта регистра. В файловой системе Windows
// «A.JS» и «a.js» — один файл, и условие, которое этого не знает, обходится
// переименованием. Это не теория: так делают.
//
// Строки в конструкторы приходят по const-ссылке и копируются в поля. Приём
// «взять по значению и переместить» здесь был бы уместнее, но move-семантика —
// лекция 11: на занятии 3.3 эти четыре конструктора и станут первым примером
// того, что она даёт, и сравнить будет с чем.
//
// Ключ шапки — "ts", "type" или "pid" — тоже работает: условие смотрит
// и в шапку, и в fields. Иначе про тип события условие написать было бы нечем,
// а именно оно нужно чаще всего.

#ifndef NANO_EDR_KIT_CONDITIONS_H
#define NANO_EDR_KIT_CONDITIONS_H

#include <string>
#include <vector>

#include "event.h"

namespace nano_edr {

// Поле равно значению целиком.
class FieldEquals {
 public:
    FieldEquals(const std::string& key, const std::string& value)
        : key_(key), value_(value) {}

    bool operator()(const Event& event) const;

 private:
    std::string key_;
    std::string value_;
};

// Поле содержит подстроку.
class FieldContains {
 public:
    FieldContains(const std::string& key, const std::string& needle)
        : key_(key), needle_(needle) {}

    bool operator()(const Event& event) const;

 private:
    std::string key_;
    std::string needle_;
};

// Поле заканчивается на суффикс.
class FieldEndsWith {
 public:
    FieldEndsWith(const std::string& key, const std::string& suffix)
        : key_(key), suffix_(suffix) {}

    bool operator()(const Event& event) const;

 private:
    std::string key_;
    std::string suffix_;
};

// Поле равно одному из перечисленных значений.
//
// Нужно чаще, чем кажется: «скриптовый хост» — это wscript.exe или cscript.exe,
// «оболочка» — cmd.exe или powershell.exe. Через FieldEquals это выражается
// только дублированием правила.
class FieldInList {
 public:
    FieldInList(const std::string& key, const std::vector<std::string>& values)
        : key_(key), values_(values) {}

    bool operator()(const Event& event) const;

 private:
    std::string key_;
    std::vector<std::string> values_;
};


// ---------------------------------------------------------------------------
// Условие, которое можно положить в вектор
// ---------------------------------------------------------------------------
//
// Четыре класса выше — разные типы. Значит `std::vector` из них не собрать:
// вектор хранит объекты одного типа, а объединить четыре в один способ пока
// нечем. Виртуальные функции — лекция 7, шаблоны — лекция 8, std::function —
// лекция 10. К занятию 2.2 не пройдено ни одно из трёх.
//
// А вектор условий нужен: правило — это «все условия выполнены».
//
// Отсюда Condition: один класс, внутри переключатель вида сравнения. Решение
// временное и заявляется таковым — на занятии 2.3 `virtual` уберёт switch,
// и четыре класса вернутся, но уже наследниками общего интерфейса. Сравнить
// две формы будет полезно: это ровно тот случай, ради которого существует
// динамическая диспетчеризация.
//
// Заодно выясняется, что видов сравнения не четыре, а три. FieldInList —
// это FieldEquals со списком значений, и ничего больше; отдельным видом
// он выглядел только потому, что был отдельным классом.

class Condition {
 public:
    enum class Match {
        kEquals,     // поле равно одному из значений
        kContains,   // поле содержит одно из значений
        kEndsWith,   // поле заканчивается одним из значений
    };

    // Фабрики вместо публичного конструктора: `Condition::EndsWith("image",
    // {"wscript.exe"})` читается, а `Condition(Match::kEndsWith, "image", ...)`
    // — нет. Приём тот же, что появится на 4.2 у типов-результатов.
    static Condition Equals(const std::string& key,
                            const std::vector<std::string>& values);
    static Condition Contains(const std::string& key,
                              const std::vector<std::string>& values);
    static Condition EndsWith(const std::string& key,
                              const std::vector<std::string>& values);

    // «Любое из значений» — то есть ИЛИ внутри одного поля. ИЛИ между разными
    // полями так не выражается, и это настоящее ограничение: правило, которому
    // нужно «path в автозапуске ИЛИ to в автозапуске», приходится записывать
    // двумя правилами с одним идентификатором. Комбинаторы появятся на 3.1.
    bool operator()(const Event& event) const;

    Match match() const { return match_; }
    const std::string& key() const { return key_; }
    const std::vector<std::string>& values() const { return values_; }

 private:
    Condition(Match match, const std::string& key,
              const std::vector<std::string>& values)
        : match_(match), key_(key), values_(values) {}

    Match match_;
    std::string key_;
    std::vector<std::string> values_;
};

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_CONDITIONS_H
