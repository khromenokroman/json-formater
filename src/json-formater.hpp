#pragma once
#include <httplib.h>
#include <nlohmann/json.hpp>

class JsonFormater {
public:
    JsonFormater();

    JsonFormater(JsonFormater const &) = delete;
    JsonFormater(JsonFormater &&) = delete;
    JsonFormater &operator=(JsonFormater const &) = delete;
    JsonFormater &operator=(JsonFormater &&) = delete;

    void run();

private:
    std::string html_escape(std::string const &s);
    std::string url_decode(std::string const &src);
    std::string json_to_highlighted_html(::nlohmann::json const &value);
    std::string extract_json_from_form(std::string const &body);
    std::string render_page(std::string const &input_json, std::string const &output_json_html,
                            std::string const &error_message);

    httplib::Server m_server; // 752
    std::string_view m_file_cfg{"/etc/json-formater/cfg.json"}; // 16
    uint64_t m_port; // 8
};
