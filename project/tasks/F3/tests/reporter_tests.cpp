// Тесты потока отправки сработок. Занятие Ф3.
//
// Поток тестировать трудно, и трудность у этого своя: неверная программа
// проходит тест девять раз из десяти. Поэтому здесь проверяется не «работает
// ли», а три свойства, каждое из которых у сломанной реализации нарушается
// детерминированно:
//
//   1. ничего не потеряно — сколько положили, столько и ушло;
//   2. порядок сохранён — очередь есть очередь;
//   3. Stop дожидается, а не просто просит остановиться.
//
// Третье — самое важное и самое частое место ошибки. Поток, которому сказали
// «остановись», и поток, который остановился, — разные состояния, и между
// ними успевает потеряться последняя сработка.

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "doctest.h"
#include "detection.h"
#include "reporter.h"
#include "rules.h"

using nano_edr::Detection;
using nano_edr::Reporter;
using nano_edr::ReportStats;
using nano_edr::Severity;

namespace {

Detection Make(const std::string& rule) {
    Detection detection;
    detection.rule = rule;
    detection.severity = Severity::kHigh;
    detection.pid = "1042";
    return detection;
}

// Получатель, запоминающий, что до него дошло. Мьютекс здесь нужен даже
// при одном потоке отчёта: читает-то вектор главный поток.
class Recorder {
 public:
    void operator()(const Detection& detection) {
        const std::lock_guard<std::mutex> guard(mutex_);
        seen_.push_back(detection.rule);
    }

    std::vector<std::string> seen() const {
        const std::lock_guard<std::mutex> guard(mutex_);
        return seen_;
    }

 private:
    mutable std::mutex mutex_;
    std::vector<std::string> seen_;
};

}  // namespace

TEST_CASE("всё положенное уходит получателю") {
    Recorder recorder;
    {
        Reporter reporter{[&recorder](const Detection& d) { recorder(d); }};
        nano_edr::AlertSink sink = reporter.Sink();
        for (int i = 0; i < 100; ++i) {
            sink(Make("rule" + std::to_string(i)));
        }
        reporter.Stop();
    }
    CHECK(recorder.seen().size() == 100);
}

TEST_CASE("порядок сохраняется") {
    // Очередь есть очередь. Порядок сработок — не косметика: по нему человек
    // восстанавливает ход атаки.
    Recorder recorder;
    {
        Reporter reporter{[&recorder](const Detection& d) { recorder(d); }};
        nano_edr::AlertSink sink = reporter.Sink();
        for (int i = 0; i < 50; ++i) {
            sink(Make("rule" + std::to_string(i)));
        }
        reporter.Stop();
    }

    const std::vector<std::string> seen = recorder.seen();
    REQUIRE(seen.size() == 50);
    for (std::size_t i = 0; i < seen.size(); ++i) {
        CHECK(seen[i] == "rule" + std::to_string(i));
    }
}

TEST_CASE("последняя сработка не теряется на остановке") {
    // Ключевой случай. Сработка кладётся и тут же запрашивается остановка;
    // реализация, которая на остановке бросает остаток очереди, потеряет её.
    Recorder recorder;
    {
        Reporter reporter{[&recorder](const Detection& d) { recorder(d); }};
        reporter.Sink()(Make("последняя"));
        reporter.Stop();
    }

    const std::vector<std::string> seen = recorder.seen();
    REQUIRE(seen.size() == 1);
    CHECK(seen[0] == "последняя");
}

TEST_CASE("Stop дожидается потока, а не только просит остановиться") {
    // Проверяется через получателя, который засыпает: если Stop вернулся
    // раньше, чем поток доработал, счётчик будет неполным.
    Recorder recorder;
    {
        Reporter reporter{[&recorder](const Detection& d) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            recorder(d);
        }};
        nano_edr::AlertSink sink = reporter.Sink();
        for (int i = 0; i < 20; ++i) {
            sink(Make("rule" + std::to_string(i)));
        }
        reporter.Stop();

        // Сразу после Stop, не дожидаясь деструктора.
        CHECK(recorder.seen().size() == 20);
    }
}

TEST_CASE("повторный Stop безвреден") {
    Recorder recorder;
    Reporter reporter{[&recorder](const Detection& d) { recorder(d); }};
    reporter.Sink()(Make("одна"));
    reporter.Stop();
    reporter.Stop();
    CHECK(recorder.seen().size() == 1);
}

TEST_CASE("деструктор останавливает поток сам") {
    // jthread существует ровно для этого: забыть join нельзя. Тест
    // не проверяет ничего, кроме того, что программа не виснет, — и это
    // как раз то, что ломается при std::thread без join.
    Recorder recorder;
    {
        Reporter reporter{[&recorder](const Detection& d) { recorder(d); }};
        reporter.Sink()(Make("одна"));
    }
    CHECK(recorder.seen().size() == 1);
}

TEST_CASE("несколько потоков кладут одновременно") {
    // Один производитель — обычный режим агента, но очередь обязана
    // выдерживать и большее: получателей у сработки несколько, и ничто
    // не обещает, что все они на одном потоке.
    Recorder recorder;
    {
        Reporter reporter{[&recorder](const Detection& d) { recorder(d); }};

        std::vector<std::jthread> writers;
        for (int t = 0; t < 4; ++t) {
            writers.emplace_back([&reporter, t] {
                nano_edr::AlertSink sink = reporter.Sink();
                for (int i = 0; i < 50; ++i) {
                    sink(Make("t" + std::to_string(t)));
                }
            });
        }
        writers.clear();  // дождаться всех
        reporter.Stop();
    }
    CHECK(recorder.seen().size() == 200);
}

TEST_CASE("статистика считает положенное и отправленное") {
    Recorder recorder;
    Reporter reporter{[&recorder](const Detection& d) { recorder(d); }};
    nano_edr::AlertSink sink = reporter.Sink();
    for (int i = 0; i < 10; ++i) {
        sink(Make("rule"));
    }
    reporter.Stop();

    const ReportStats stats = reporter.stats();
    CHECK(stats.posted == 10);
    CHECK(stats.sent == 10);
    CHECK(stats.high_water >= 1);
}
