#include <stdexcept>
#include <string>

#include "alert_sink.h"
#include "dedup_sink.h"
#include "detection.h"

namespace nano_edr {

// --- ConsoleSink -----------------------------------------------------------

void ConsoleSink::operator()(const Detection&) const {}

// --- FileSink --------------------------------------------------------------

FileSink::FileSink(const std::string& path) : out_(path) {
    if (!out_.is_open()) {
        throw std::runtime_error("не удалось открыть файл: " + path);
    }
}

void FileSink::operator()(const Detection& detection) {
    out_ << ToString(detection) << '\n';
}

// --- CountingSink ----------------------------------------------------------

void CountingSink::operator()(const Detection&) const {}

// --- FanOutSink ------------------------------------------------------------

void FanOutSink::Add(AlertSink) {}

void FanOutSink::operator()(const Detection&) const {}

// --- DedupSink -------------------------------------------------------------

DedupSink::DedupSink(AlertSink, uint64_t, std::size_t*) {}

void DedupSink::operator()(const Detection&) {}

}  // namespace nano_edr
