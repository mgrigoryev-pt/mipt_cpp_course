// Правила как иерархия. Занятие 3.2.
//
// Что добавилось на 3.2. Во-первых, LambdaRule — правило из произвольного
// предиката; появление лямбд означает, что не под каждое правило нужен класс.
// Во-вторых, SequenceRule научился связке `child_of_prev`, и связка эта
// живёт не на окне событий, а на модели сущностей: «второе событие произошло
// в процессе, чей родитель — процесс первого события».
//
// Здесь два похожих класса, и вопрос «зачем оба» — один из главных на К2.
// Ответ короткий: они отвечают на разные вопросы.
//
//   IRule    — что правило умеет. Только чистые виртуальные функции, ни одного
//              поля. Это контракт: движок разговаривает с правилами через него
//              и больше ничего о них не знает.
//   RuleBase — что у всех правил одинаково. Идентификатор, важность, счётчик,
//              список действий. Это переиспользование, а не контракт.
//
// Разделять их стоит потому, что они меняются по разным причинам. Появится
// правило, которому не нужен счётчик, — оно унаследует IRule напрямую.
// Понадобится общим правилам ещё одно поле — оно добавится в RuleBase,
// и движок этого не заметит.
//
// Вектор правил теперь `std::vector<std::unique_ptr<IRule>>`, и это прямой
// ответ на вопрос занятия 2.2. Тогда правила хранились вектором конкретного
// типа, потому что вектор базового класса срезал бы наследника до базовой
// части. Указатель срезать нечего: он одного размера всегда.

#ifndef NANO_EDR_KIT_RULE_H
#define NANO_EDR_KIT_RULE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <string>
#include <vector>

#include "conditions.h"
#include "entity_model.h"
#include "event.h"
#include "fields.h"
// Свой `Function` студент пишет сам на занятии 3.2 и кладёт в src/ —
// этот каталог стоит на include-пути раньше комплекта.
#include "function.h"
// Кольцевой буфер студент пишет сам на занятии 3.1 и кладёт в src/ —
// этот каталог стоит на include-пути раньше комплекта.
#include "ring_buffer.h"
#include "rules.h"

namespace nano_edr {

// Контракт правила.
class IRule {
 public:
    // Виртуальный деструктор. Без него `delete` по указателю на IRule
    // не вызовет деструктор наследника: условия внутри MatchRule утекут,
    // и утечка будет тем более неприятной, что код при этом выглядит
    // безупречно. Уберите virtual и прогоните под санитайзером —
    // это домашнее задание занятия, и делать его стоит.
    virtual ~IRule() = default;

    virtual const std::string& id() const = 0;
    virtual Severity severity() const = 0;

    // Состояние допустимо: SequenceRule помнит незавершённые совпадения.
    // Отсюда неконстантный метод — и это честнее, чем mutable-поля
    // под словом const.
    virtual bool Check(const Event& event) = 0;

    // ЗАНЯТИЕ Ф1. Тик: событий сейчас нет, есть время убрать за собой.
    //
    // Правило с состоянием — это контейнер, который живёт столько же, сколько
    // процесс, и наполняется из потока. По правилу лекции 13 удаление старого
    // обязано быть частью его дизайна. У SequenceRule незавершённые
    // совпадения снимаются и в Check, но только когда правило вообще
    // проверяется; у ThresholdRule счётчики по ключам не снимаются нигде,
    // и на нагрузочном журнале их набирается по одному на каждый номер
    // процесса, который когда-либо встретился.
    //
    // НЕ чистая виртуальная, и это осознанно: правилу без состояния убирать
    // нечего, и заставлять его писать пустое тело значило бы платить за чужую
    // проблему. Первый метод в этом интерфейсе с телом — и первый случай,
    // когда «интерфейс = только чистые виртуальные» стоит нарушить.
    virtual void OnTick(Timestamp /*now*/) {}

    virtual std::size_t hits() const = 0;
    virtual const std::vector<std::string>& actions() const = 0;
};

// Общая часть правил: то, что одинаково у всех.
class RuleBase : public IRule {
 public:
    RuleBase(const std::string& id, Severity severity);

    const std::string& id() const override { return id_; }
    Severity severity() const override { return severity_; }
    std::size_t hits() const override { return hits_; }
    const std::vector<std::string>& actions() const override { return actions_; }

    void AddAction(const std::string& action);

 protected:
    void CountHit() { ++hits_; }

 private:
    std::string id_;
    Severity severity_;
    std::vector<std::string> actions_;
    std::size_t hits_ = 0;
};

// Все условия выполнены на одном событии.
class MatchRule : public RuleBase {
 public:
    MatchRule(const std::string& id, Severity severity);

    // Владение условием переходит правилу. Отсюда unique_ptr в подписи:
    // он говорит об этом сам, и забыть освободить условие нечем.
    void AddCondition(std::unique_ptr<ICondition> condition);

    bool Check(const Event& event) override;

    std::size_t condition_count() const { return conditions_.size(); }

 private:
    std::vector<std::unique_ptr<ICondition>> conditions_;
};

// Два шага в пределах окна, связанные одним процессом.
//
// Первое правило с состоянием, и оно же — первое, которое нельзя выразить
// проверкой одного события. «Скриптовый хост, запущенный офисным приложением»
// — это два события, между ними время, и связь между ними по pid.
//
// Состояние здесь маленькое намеренно: список незавершённых совпадений
// с временем и pid. Полноценное окно событий, три шага и связки вроде
// child_of_prev — занятие 3.1; здесь двух шагов и same_pid достаточно, чтобы
// увидеть, чем правило с состоянием отличается от правила без него.
class SequenceRule : public RuleBase {
 public:
    // window_ms — сколько модельного времени второй шаг ждёт первого.
    SequenceRule(const std::string& id, Severity severity, uint64_t window_ms);

    void SetFirst(std::unique_ptr<ICondition> condition);
    void SetSecond(std::unique_ptr<ICondition> condition);

    // Требовать, чтобы у обоих шагов совпадал pid. По умолчанию да: без этого
    // правило связывает случайные события, и ложные срабатывания неизбежны.
    void set_same_pid(bool value) { same_pid_ = value; }

    // Связка `child_of_prev`: второй шаг произошёл в процессе, чей родитель —
    // процесс первого шага. Занятие 3.2, домашняя часть.
    //
    // Окно событий этого не умеет и уметь не может: в событии `process_start`
    // родитель есть, а в остальных нет, и «кто родитель pid» — вопрос
    // к модели, а не к последним 256 событиям. Отсюда SetModel.
    //
    // Включённая связка отменяет same_pid: требовать одновременно «тот же
    // процесс» и «его потомок» бессмысленно.
    //
    // Без модели связка не выполняется никогда. Это выбор в пользу тишины:
    // правило, которое не может проверить свою связь и срабатывает всё равно,
    // хуже правила, которое молчит.
    void set_child_of_prev(bool value) { child_of_prev_ = value; }

    // Модель сущностей для связки child_of_prev. Не владеет.
    void SetModel(const EntityModel* model) { model_ = model; }

    bool Check(const Event& event) override;

    // ЗАНЯТИЕ Ф1. Снять просроченные совпадения, не дожидаясь события.
    //
    // Check делает то же самое, но только когда его зовут. Правило, которое
    // набрало сотню первых шагов и перестало проверяться, держит их до
    // конца прогона.
    void OnTick(Timestamp now) override;

    // Сколько незавершённых совпадений держится сейчас. Нужно тестам
    // и полезно на паре: у правила с состоянием память растёт, и увидеть,
    // что она не растёт бесконечно, стоит своими глазами.
    std::size_t pending() const { return pending_.size(); }

 private:
    struct Pending {
        uint64_t ts_ms;
        std::string pid;
    };

    // Связаны ли шаги так, как требует правило.
    bool Linked(const Pending& first, const Event& second) const;

    // ЗАНЯТИЕ Ф1. Выбросить совпадения, у которых истекло окно. Одно место
    // на два вызова — из Check и из OnTick.
    void DropExpired(uint64_t now_ms);

    std::unique_ptr<ICondition> first_;
    std::unique_ptr<ICondition> second_;
    uint64_t window_ms_;
    bool same_pid_ = true;
    bool child_of_prev_ = false;
    const EntityModel* model_ = nullptr;
    std::vector<Pending> pending_;
};


// Правило из произвольного предиката.
//
// Зачем оно, если есть MatchRule с условиями-объектами: условие-объект умеет
// смотреть только на событие. А есть вопросы, на которые по событию
// не ответить — «этот процесс запустился из файла, который записал кто-то
// другой». Ответ на такой вопрос знает модель сущностей, и заводить под каждый
// такой вопрос новый класс ICondition с полем-указателем на модель — работа
// на пустом месте.
//
// Лямбда захватывает модель и остаётся выражением в одну строку. Ровно то,
// ради чего в языке есть замыкания: поведение, которому не нужно имя.
//
// ОГРАНИЧЕНИЕ ФАБРИКИ, КОТОРОГО БОЛЬШЕ НЕТ
//
// На занятии 3.2 фабрика передавала свои аргументы дальше копией, и на
// move-only аргументе это не собиралось:
//
//   Predicate p = лямбда;
//   MakeRule<LambdaRule>(id, sev, std::move(p));   // use of deleted function
//
// Обратите внимание, где именно проходила граница. С самой лямбдой фабрика
// работала и тогда — лямбда, захватившая указатель, копируется, и получалось
// на одну копию больше нужного. Ломался не вызов с лямбдой, а вызов с уже
// собранным Function: внутри него unique_ptr, а его не копирует никто.
//
// Занятие 3.3 починило это одним словом в теле фабрики: `std::move(args)...` —
// перемещением того, что фабрика уже держит по значению. Цена: один лишний
// move на создание правила, то есть перенос указателя. Выгода: фабрика начала
// принимать то, что копировать нельзя вообще.
//
// Идеальная передача (лекция 11) решила бы то же иначе: `Args&&...` плюс
// `std::forward<Args>(args)...`, и без лишнего move. На вызовах агента разницы
// нет — аргументы там временные, а на них обе версии совпадают. Разница
// появляется на именованной переменной; замер обеих — в лекции 11.
//
// Смотреть на это стоит именно в таком порядке. Сначала код не собрался,
// потом выяснилось, почему именно — и почему не по той причине, которая
// казалась очевидной, — потом починка оказалась в одно слово. Обратный
// порядок, «здесь надо ставить std::move, потому что так правильно»,
// объясняет ровно ничего.
class LambdaRule : public RuleBase {
 public:
    using Predicate = Function<bool(const Event&)>;

    LambdaRule(const std::string& id, Severity severity, Predicate predicate);

    bool Check(const Event& event) override;

 private:
    Predicate predicate_;
};


// Порог: N совпадений за T миллисекунд.
//
// Третий вид правила и второй с состоянием. «Сорок файлов за шесть секунд»
// нельзя выразить ни одним событием, ни парой: нужен счёт.
//
// Состояние — кольцевой буфер отметок времени, по одному на ключ. Ключ обычно
// "pid": порог считается на процесс, а не на весь хост, иначе десять
// безобидных процессов сложатся в один детект. Пустой ключ означает
// «считать всё вместе».
//
// N — non-type параметр: сколько отметок помнить. Оно же и есть предел порога:
// правило с count больше N не сработает никогда, и static_assert об этом
// не скажет, потому что count задаётся во время работы. Проверка в конструкторе.
template <std::size_t N>
class ThresholdRule : public RuleBase {
 public:
    ThresholdRule(const std::string& id, Severity severity, std::size_t count,
                  uint64_t window_ms, const std::string& key)
        : RuleBase(id, severity),
          count_(count),
          window_ms_(window_ms),
          key_(key) {}

    void SetCondition(std::unique_ptr<ICondition> condition) {
        condition_ = std::move(condition);
    }

    bool Check(const Event& event) override {
        if (condition_ == nullptr || !condition_->Matches(event)) {
            return false;
        }
        if (count_ == 0 || count_ > N) {
            // Порог, которого буфер не вмещает, — ошибка настройки правила.
            // Молчать здесь правильнее, чем срабатывать: правило, которое
            // не может сработать, лучше правила, которое срабатывает не так.
            return false;
        }

        // ЗАНЯТИЕ Ф1. Словарь вместо линейного поиска: одна строка вместо
        // цикла, и время обращения перестало зависеть от числа ключей.
        // operator[] создаёт запись, если ключа не было, — и это то самое
        // поведение, ради которого FindOrAddBucket существовал.
        Bucket& bucket = buckets_[KeyOf(event)];
        bucket.stamps.PushBack(event.ts());
        bucket.last_seen = event.ts();

        // Считаем, сколько отметок попадает в окно, считая от текущей.
        // Проход по кольцевому буферу — проход по непрерывной памяти.
        //
        // ЗАНЯТИЕ Ф1. std::count_if вместо цикла со счётчиком (лекция 13).
        // Быстрее не станет: там тот же проход. Лучше становится другое —
        // из кода видно, что здесь считают, а не фильтруют, суммируют или
        // ищут первое подходящее. Цикл со счётчиком приходится прочитать
        // целиком, чтобы это узнать.
        const uint64_t now = event.ts().ms;
        const uint64_t window = window_ms_;
        const auto in_window = static_cast<std::size_t>(
            std::count_if(bucket.stamps.begin(), bucket.stamps.end(),
                          [now, window](const Timestamp& stamp) {
                              return now >= stamp.ms && now - stamp.ms <= window;
                          }));

        if (in_window < count_) {
            return false;
        }

        // Буфер очищается после детекта: иначе одно превышение порога дало бы
        // детект на каждом следующем событии, и на потоке это стало бы шумом.
        bucket.stamps.Clear();
        CountHit();
        return true;
    }

    // ЗАНЯТИЕ Ф1. Вытеснение счётчиков, к которым давно не обращались.
    //
    // Здесь была утечка с полезной нагрузкой — та самая, про которую лекция 13
    // говорит «формально всё достижимо, фактически процесс растёт». Ключ
    // счётчика — pid, номера выдаются по кругу, и на нагрузочном журнале
    // правило набирает счётчик на каждый номер, который когда-либо встретился.
    // Один счётчик — это `std::string` и N отметок времени; при N = 64 это
    // больше полукилобайта, и шестьдесят пять тысяч номеров дают десятки
    // мегабайт, которые не освободятся никогда.
    //
    // std::erase_if (C++20) — ровно то, что нужно, и заодно единственный
    // способ не наступить на классическую ошибку: удаление во время ручного
    // обхода инвалидирует итератор, и ++it после erase читает мусор.
    void OnTick(Timestamp now) override {
        if (idle_ms_ == 0) {
            return;
        }
        // Проход по всем счётчикам — не на каждом тике, а не чаще, чем раз
        // в восьмую часть срока простоя. Причина та же, что у модели: тик
        // приходит каждые несколько десятков событий, проход стоит линейно
        // от числа счётчиков, а находит он редко. Замер: без этого условия
        // уборка счётчиков стоила семь секунд из четырнадцати.
        //
        // Плата названа: счётчик может пережить свой срок на одну восьмую
        // этого срока. Для порогового правила это безразлично — счётчик,
        // к которому не обращались, на решение не влияет.
        if (now.ms < last_sweep_ms_ + idle_ms_ / 8) {
            return;
        }
        last_sweep_ms_ = now.ms;

        const uint64_t deadline_from = now.ms;
        const uint64_t idle = idle_ms_;
        // Structured bindings (лекция 13): элемент словаря — пара, и назвать
        // её половины именами читается лучше, чем item.second.last_seen.
        std::erase_if(buckets_, [deadline_from, idle](const auto& item) {
            const auto& [key, bucket] = item;
            return deadline_from > bucket.last_seen.ms &&
                   deadline_from - bucket.last_seen.ms > idle;
        });
    }

    // Сколько модельного времени счётчик живёт без обращений. Ноль отключает
    // вытеснение — так ведут себя тесты, которым важен сам порог, а не память.
    void set_idle_ms(uint64_t value) { idle_ms_ = value; }

    std::size_t bucket_count() const { return buckets_.size(); }

 private:
    struct Bucket {
        RingBuffer<Timestamp, N> stamps;
        Timestamp last_seen;
    };

    std::string KeyOf(const Event& event) const {
        if (key_.empty()) {
            return std::string();
        }
        const std::string* value = FindField(event, key_);
        return value != nullptr ? *value : std::string();
    }

    std::unique_ptr<ICondition> condition_;
    std::size_t count_;
    uint64_t window_ms_;
    std::string key_;
    uint64_t idle_ms_ = 0;
    uint64_t last_sweep_ms_ = 0;
    std::unordered_map<std::string, Bucket> buckets_;
};

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_RULE_H
