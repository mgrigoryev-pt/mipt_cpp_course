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

#include <cstddef>
#include <cstdint>
#include <memory>
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

        Bucket* bucket = FindOrAddBucket(KeyOf(event));
        bucket->stamps.PushBack(event.ts());

        // Считаем, сколько отметок попадает в окно, считая от текущей.
        // Проход по кольцевому буферу — проход по непрерывной памяти.
        std::size_t in_window = 0;
        for (const Timestamp& stamp : bucket->stamps) {
            if (event.ts().ms >= stamp.ms &&
                event.ts().ms - stamp.ms <= window_ms_) {
                ++in_window;
            }
        }

        if (in_window < count_) {
            return false;
        }

        // Буфер очищается после детекта: иначе одно превышение порога дало бы
        // детект на каждом следующем событии, и на потоке это стало бы шумом.
        bucket->stamps.Clear();
        CountHit();
        return true;
    }

    std::size_t bucket_count() const { return buckets_.size(); }

 private:
    struct Bucket {
        std::string key;
        RingBuffer<Timestamp, N> stamps;
    };

    std::string KeyOf(const Event& event) const {
        if (key_.empty()) {
            return std::string();
        }
        const std::string* value = FindField(event, key_);
        return value != nullptr ? *value : std::string();
    }

    Bucket* FindOrAddBucket(const std::string& key) {
        for (std::size_t i = 0; i < buckets_.size(); ++i) {
            if (buckets_[i].key == key) {
                return &buckets_[i];
            }
        }
        // Линейный поиск по ключам. Словарей ещё нет — они на занятии 4.1,
        // где этот же поиск и заменится с замером. На десятке процессов
        // разницы не будет, и знать это полезнее, чем сразу писать словарь.
        Bucket bucket;
        bucket.key = key;
        buckets_.push_back(bucket);
        return &buckets_.back();
    }

    std::unique_ptr<ICondition> condition_;
    std::size_t count_;
    uint64_t window_ms_;
    std::string key_;
    std::vector<Bucket> buckets_;
};

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_RULE_H
