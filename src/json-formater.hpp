#pragma once
#include <httplib.h>

class JsonFormater {
public:
    JsonFormater();

    JsonFormater(JsonFormater const &) = delete;
    JsonFormater(JsonFormater &&) = delete;
    JsonFormater &operator=(JsonFormater const &) = delete;
    JsonFormater &operator=(JsonFormater &&) = delete;

    void run();
private:
    httplib::Server m_server; // 752
    std::string_view m_file_cfg{"/etc/json-formater/cfg.json"}; // 16
    uint64_t m_port; // 8
};
