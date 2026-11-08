// Файл для записи, который можно копировать. Занятие 3.2.
//
// AlertSink — это std::function, а std::function копирует то, что в неё
// кладут. std::ofstream не копируется, поэтому FileSink держит FileStream:
// копии FileStream пишут в один и тот же файл, а закрывается файл, когда
// исчезает последняя копия.
//
// Пишут в него так же, как в std::ofstream: `out << text << '\n'`.

#ifndef NANO_EDR_KIT_FILE_STREAM_H
#define NANO_EDR_KIT_FILE_STREAM_H

#include <fstream>
#include <memory>
#include <ostream>
#include <string>

namespace nano_edr {

class FileStream {
 public:
    explicit FileStream(const std::string& path)
        : out_(std::make_shared<std::ofstream>(path)) {}

    bool is_open() const { return out_->is_open(); }

    explicit operator bool() const { return static_cast<bool>(*out_); }

    template <typename T>
    FileStream& operator<<(const T& value) {
        *out_ << value;
        return *this;
    }

    // std::endl и std::flush.
    FileStream& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
        *out_ << manipulator;
        return *this;
    }

 private:
    std::shared_ptr<std::ofstream> out_;
};

}  // namespace nano_edr

#endif  // NANO_EDR_KIT_FILE_STREAM_H
