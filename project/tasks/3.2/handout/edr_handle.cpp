// Вторая граница и получатель поверх неё. Занятие 3.2. См. edr_handle.h.
//
// ЭТОТ ФАЙЛ ВЫДАН ГОТОВЫМ. Скопируйте его в `src/` — и дальше он ваш.
//
// Почему выдан. RAII-обёртку над C-границей вы уже писали на занятии 2.1,
// здесь она та же, только граница другая. А перевод детекта в `edr_message` —
// работа с C-структурами и временем жизни строк, к лямбдам и стиранию типа
// отношения не имеет. Смысл занятия здесь в другом: `EdrSink` — получатель,
// устроенный совсем иначе остальных, и встаёт он в раздачу без правок движка.

#include "edr_handle.h"

#include <cstddef>
#include <cstdint>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

#include "field_traits.h"
#include "rules.h"

namespace nano_edr {
namespace {

// pid в шапке сообщения — число, а у детекта он строкой: у события он мог
// отсутствовать вовсе, и пустая строка передаёт это точнее нуля. Здесь
// приходится вернуться к числу, потому что так устроена граница.
//
// Разбор — тот же FieldTraits<uint64_t> с занятия 3.1: правило «одно и то же
// разбирается одним кодом» не отменяется тем, что разбирают на границе.
uint32_t PidNumber(const std::string& pid) {
    uint64_t value = 0;
    if (!FieldTraits<uint64_t>::Parse(pid, &value)) {
        return 0;  // «неприменимо» по контракту edr.h
    }
    if (value > UINT32_MAX) {
        return 0;
    }
    return static_cast<uint32_t>(value);
}

std::string JoinActions(const std::vector<std::string>& actions) {
    std::string text;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (i > 0) {
            text += ',';
        }
        text += actions[i];
    }
    return text;
}

}  // namespace

// --- EdrHandle -------------------------------------------------------------

EdrHandle::EdrHandle(const std::string& config_path) {
    const edr_status status = edr_init(config_path.c_str(), &handle_);
    if (status != EDR_OK) {
        // Причина в сообщении обязательна: «edr_init не прошёл» без причины
        // не даёт сделать ни одного следующего шага.
        throw std::runtime_error("edr_init не прошёл (" +
                                 std::string(edr_status_str(status)) + "): " +
                                 config_path);
    }
}

EdrHandle::~EdrHandle() {
    // Не бросает и ничего не возвращает: edr_free сам сбрасывает накопленное,
    // а деструктору всё равно нечего было бы сделать с кодом возврата.
    edr_free(handle_);
    handle_ = nullptr;
}

edr_status EdrHandle::Send(const edr_message& msg) {
    return edr_send(handle_, &msg);
}

edr_status EdrHandle::Flush() {
    return edr_flush(handle_);
}

std::string EdrHandle::Config(const std::string& key,
                              const std::string& fallback) {
    std::size_t needed = 0;
    edr_status status =
        edr_config_get(handle_, key.c_str(), nullptr, 0, &needed);
    if (status == EDR_NO_CONFIG) {
        // Не ошибка. Сервер не давал значения — значит своё умолчание.
        return fallback;
    }
    if (status != EDR_BUFFER_TOO_SMALL || needed == 0) {
        // Сюда попадает всё остальное: EDR_INVALID_ARG на пустом ключе,
        // сервер недоступен. Работать с умолчанием честнее, чем падать из-за
        // недоступной конфигурации.
        return fallback;
    }

    std::string value(needed, '\0');
    status = edr_config_get(handle_, key.c_str(), value.data(), value.size(),
                            &needed);
    if (status != EDR_OK) {
        return fallback;
    }
    // В needed длина с нулевым терминатором — в строке он лишний.
    value.resize(needed - 1);
    return value;
}

// --- EdrSink ---------------------------------------------------------------

void EdrSink::operator()(const Detection& detection) const {
    if (edr_ == nullptr) {
        return;
    }

    // Строки живут до конца вызова — ровно то, что требует контракт границы.
    // Держать их дольше не нужно: реализация копирует всё, что ей нужно,
    // до возврата.
    const std::string severity = SeverityName(detection.severity);
    const std::string actions = JoinActions(detection.actions);

    std::vector<edr_kv> fields;
    fields.reserve(4);
    // rule обязателен: детект без правила граница отвергает, и правильно.
    fields.push_back(edr_kv{"rule", detection.rule.c_str()});
    fields.push_back(edr_kv{"severity", severity.c_str()});
    if (!detection.chain.empty()) {
        fields.push_back(edr_kv{"chain", detection.chain.c_str()});
    }
    if (!actions.empty()) {
        fields.push_back(edr_kv{"actions", actions.c_str()});
    }

    edr_message message{};
    message.ts = detection.ts.ms;
    message.type = "detection";
    message.pid = PidNumber(detection.pid);
    message.fields = fields.data();
    message.field_count = fields.size();

    const edr_status status = edr_->Send(message);
    if (status != EDR_OK) {
        // Симулятор всегда отвечает EDR_OK, так что в этом курсе ветвь
        // не срабатывает ни разу. Она здесь потому, что агент, молча
        // теряющий детект при отказе отправки, — сломанный агент, и увидеть
        // это словами дешевле, чем потом на живой сети.
        std::print(stderr, "[WARN] детект не ушёл наверх: {} ({})\n",
                   detection.rule, edr_status_str(status));
    }
}

}  // namespace nano_edr
