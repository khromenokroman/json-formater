#include "json-formater.hpp"
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <syslog.h>

JsonFormater::JsonFormater() {
    openlog("JsonFormater", LOG_PID | LOG_CONS, LOG_USER);

    std::ifstream file_cfg(m_file_cfg.data());
    if (!file_cfg.is_open()) {
        auto err = errno;
        syslog(LOG_ERR, "Не могу открыть настройки(%s): %s", m_file_cfg.data(), strerror(err));
        throw std::runtime_error(::fmt::format("Не могу открыть настройки({}): {}", m_file_cfg, strerror(err)));
    }
    ::nlohmann::json cfg;
    file_cfg >> cfg;
    file_cfg.close();
    m_port = cfg.value("port", 8080);
    auto log_level = cfg.value("log_level", LOG_INFO);
    setlogmask(LOG_UPTO(log_level));
}
void JsonFormater::run() {}
