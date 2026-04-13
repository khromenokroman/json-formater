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
    syslog(LOG_DEBUG, "Получены настройки:\n%s", cfg.dump(1).c_str());
}
void JsonFormater::run() {
    m_server.Get("/", [this](const httplib::Request &req, httplib::Response &res) {
        syslog(LOG_DEBUG, "Поступил запрос от %s:%d на %s:%d", req.remote_addr.c_str(), req.remote_port,
               req.local_addr.c_str(), req.local_port);

        syslog(LOG_DEBUG, "Отображение основной страницы '/'");
        res.set_content(render_page("", "", ""), "text/html; charset=UTF-8");
    });

    m_server.Post("/format", [this](const httplib::Request &req, httplib::Response &res) {
        syslog(LOG_DEBUG, "Поступил запрос от %s:%d на %s:%d", req.remote_addr.c_str(), req.remote_port,
               req.local_addr.c_str(), req.local_port);

        std::string input_json = std::move(extract_json_from_form(std::move(req.body)));
        std::string output_json_html;
        std::string error_message;
        ::nlohmann::json parsed{};

        try {
            parsed = std::move(::nlohmann::json::parse(input_json));
            output_json_html = std::move(json_to_highlighted_html(parsed));
        } catch (const std::exception &ex) {
            error_message = ::fmt::format("Ошибка JSON: {}", ex.what());
        }

        syslog(LOG_DEBUG, "Отображение страницы '/format'");
        syslog(LOG_DEBUG, "Исходный json:\n%s", input_json.c_str());
        syslog(LOG_DEBUG, "Форматированный json:\n%s", parsed.dump(1).c_str());
        syslog(LOG_DEBUG, "Форматированный json(с подсветкой):\n%s", output_json_html.c_str());
        if (!error_message.empty()) {
            syslog(LOG_DEBUG, "Есть ошибка при конвертации: %s", error_message.c_str());
        }

        res.set_content(render_page(input_json, output_json_html, error_message), "text/html; charset=UTF-8");
    });

    m_server.Post("/compress", [this](const httplib::Request &req, httplib::Response &res) {
        syslog(LOG_DEBUG, "Поступил запрос от %s:%d на %s:%d", req.remote_addr.c_str(), req.remote_port,
               req.local_addr.c_str(), req.local_port);

        std::string input_json = extract_json_from_form(req.body);
        std::string output_json_html;
        std::string error_message;
        ::nlohmann::json parsed{};

        try {
            parsed = std::move(::nlohmann::json::parse(std::move(input_json)));
            output_json_html = html_escape(parsed.dump());
        } catch (const std::exception &ex) {
            error_message = ::fmt::format("Ошибка JSON: {}", ex.what());
        }

        syslog(LOG_DEBUG, "Отображение страницы '/compress'");
        syslog(LOG_DEBUG, "Исходный json:\n%s", input_json.c_str());
        syslog(LOG_DEBUG, "Сжатый json:\n%s", parsed.dump().c_str());
        syslog(LOG_DEBUG, "Сжатый json(с эскейпами):\n%s", output_json_html.c_str());
        if (!error_message.empty()) {
            syslog(LOG_DEBUG, "Есть ошибка при конвертации: %s", error_message.c_str());
        }

        res.set_content(render_page(input_json, output_json_html, error_message), "text/html; charset=UTF-8");
    });

    syslog(LOG_NOTICE, "Запущен сервер на http://127.0.0.1:%d", static_cast<int>(m_port));
    m_server.listen("0.0.0.0", m_port);
}
std::string JsonFormater::html_escape(std::string const &s) {
    std::string out;
    out.reserve(s.size());
    for (char c: s) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}
std::string JsonFormater::url_decode(std::string const &src) {
    std::string out;
    out.reserve(src.size());

    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < src.size()) {
            auto hex = [](char x) -> int {
                if (x >= '0' && x <= '9')
                    return x - '0';
                if (x >= 'a' && x <= 'f')
                    return 10 + (x - 'a');
                if (x >= 'A' && x <= 'F')
                    return 10 + (x - 'A');
                return -1;
            };
            int hi = hex(src[i + 1]);
            int lo = hex(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
            } else {
                out += c;
            }
        } else {
            out += c;
        }
    }

    return out;
}
std::string JsonFormater::json_to_highlighted_html(nlohmann::json const &value) {
    std::ostringstream oss;
    std::string pretty = value.dump(4);

    bool in_string = false;
    bool escaped = false;

    for (size_t i = 0; i < pretty.size(); ++i) {
        char c = pretty[i];

        if (in_string) {
            if (escaped) {
                oss << html_escape(std::string(1, c));
                escaped = false;
                continue;
            }

            if (c == '\\') {
                oss << "\\";
                escaped = true;
                continue;
            }

            if (c == '"') {
                oss << "\"</span>";
                in_string = false;
                continue;
            }

            oss << html_escape(std::string(1, c));
            continue;
        }

        if (c == '"') {
            bool key_candidate = false;

            size_t k = i + 1;
            bool escaped2 = false;
            while (k < pretty.size()) {
                char nc = pretty[k];
                if (escaped2) {
                    escaped2 = false;
                } else if (nc == '\\') {
                    escaped2 = true;
                } else if (nc == '"') {
                    size_t t = k + 1;
                    while (t < pretty.size() && std::isspace(static_cast<unsigned char>(pretty[t]))) {
                        ++t;
                    }
                    if (t < pretty.size() && pretty[t] == ':') {
                        key_candidate = true;
                    }
                    break;
                }
                ++k;
            }

            if (key_candidate) {
                oss << "<span class=\"json-key\">\"";
            } else {
                oss << "<span class=\"json-string\">\"";
            }

            in_string = true;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '-' && i + 1 < pretty.size() && std::isdigit(static_cast<unsigned char>(pretty[i + 1])))) {
            size_t start = i;
            size_t end = i + 1;
            while (end < pretty.size()) {
                char nc = pretty[end];
                if (std::isdigit(static_cast<unsigned char>(nc)) || nc == '.' || nc == 'e' || nc == 'E' || nc == '+' ||
                    nc == '-') {
                    ++end;
                } else {
                    break;
                }
            }
            oss << "<span class=\"json-number\">" << html_escape(pretty.substr(start, end - start)) << "</span>";
            i = end - 1;
            continue;
        }

        if (pretty.compare(i, 4, "true") == 0) {
            oss << "<span class=\"json-bool\">true</span>";
            i += 3;
            continue;
        }
        if (pretty.compare(i, 5, "false") == 0) {
            oss << "<span class=\"json-bool\">false</span>";
            i += 4;
            continue;
        }
        if (pretty.compare(i, 4, "null") == 0) {
            oss << "<span class=\"json-null\">null</span>";
            i += 3;
            continue;
        }

        oss << html_escape(std::string(1, c));
    }

    return oss.str();
}

std::string JsonFormater::extract_json_from_form(std::string const &body) {
    const std::string prefix = "json=";
    if (body.rfind(prefix, 0) == 0) {
        return url_decode(body.substr(prefix.size()));
    }
    return body;
}
std::string JsonFormater::render_page(std::string const &input_json, std::string const &output_json_html,
                                      std::string const &error_message) {
    std::string html;
    html += R"HTML(<!doctype html>
<html lang="ru">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>JSON Formatter</title>
  <style>
    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      padding: 24px;
      font-family: "Cambria", serif;
      background: #f5f7fb;
      color: #1f2937;
      font-size: 18px;
    }

    button, input, textarea, select, option, .json-output, .hint, .error, h1, h3, .small-btn {
      font-family: "Cambria", serif;
    }

    .container {
      max-width: 1600px;
      margin: 0 auto;
    }

    h1 {
      margin-top: 0;
      font-size: 34px;
    }

    h3 {
      font-size: 22px;
      margin: 0;
    }

    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
      align-items: start;
    }

    .panel {
      display: flex;
      flex-direction: column;
      height: 100%;
    }

    .panel-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
      gap: 10px;
    }

    .panel-actions {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
    }

    .small-btn {
      padding: 6px 10px;
      border: 1px solid #cbd5e1;
      border-radius: 8px;
      background: #ffffff;
      color: #1f2937;
      font-size: 14px;
      cursor: pointer;
    }

    .small-btn:hover {
      background: #e2e8f0;
    }

    .left-panel textarea,
    .json-output {
      width: 100%;
      box-sizing: border-box;
      padding: 16px;
      border: 1px solid #cbd5e1;
      border-radius: 10px;
      font-size: 18px;
      line-height: 1.5;
      background: white;
    }

    .left-panel textarea {
      height: 35vh;
      min-height: 250px;
      max-height: 37.5vh;
      resize: vertical;
      white-space: pre;
      overflow: auto;
    }

    .right-panel .json-output {
      height: 70vh;
      min-height: 500px;
      max-height: 75vh;
      overflow: auto;
      white-space: pre-wrap;
      word-break: break-word;
    }

    .actions {
      margin-top: 12px;
      display: flex;
      gap: 12px;
      justify-content: flex-start;
      align-items: flex-end;
      min-height: 52px;
    }

    .main-btn {
      padding: 12px 22px;
      border: none;
      border-radius: 10px;
      background: #2563eb;
      color: white;
      font-size: 18px;
      cursor: pointer;
    }

    .main-btn:hover {
      background: #1d4ed8;
    }

    .hint {
      color: #6b7280;
      font-size: 16px;
    }

    .error {
      margin-top: 16px;
      color: #dc2626;
      white-space: pre-wrap;
      font-size: 16px;
    }

    .json-key { color: #7c3aed; }
    .json-string { color: #15803d; }
    .json-number { color: #d97706; }
    .json-bool { color: #2563eb; }
    .json-null { color: #dc2626; }

    @media (max-width: 900px) {
      .grid {
        grid-template-columns: 1fr;
      }

      .left-panel textarea,
      .json-output {
        height: 50vh;
        max-height: 50vh;
      }

      body {
        font-size: 16px;
      }

      h1 {
        font-size: 28px;
      }

      h3 {
        font-size: 20px;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>JSON Formatter</h1>
    <p class="hint">Вставь JSON слева и нажми Format или Compress.</p>

    <form method="post" action="/format">
      <div class="grid">
        <div class="panel left-panel">
          <div class="panel-header">
            <h3>Неформатированный JSON</h3>
            <div class="panel-actions">
              <button type="button" class="small-btn" onclick="clearInput()">Clear</button>
              <button type="button" class="small-btn" onclick="copyInput()">Copy</button>
            </div>
          </div>

          <textarea id="inputJson" name="json" placeholder='{"name":"Alex","age":20,"items":[1,2,3]}'>)HTML";
    html += html_escape(input_json);
    html += R"HTML(</textarea>

          <div class="actions">
            <button type="submit" formaction="/format" class="main-btn">Format</button>
            <button type="submit" formaction="/compress" class="main-btn">Compress</button>
          </div>
        </div>

        <div class="panel right-panel">
          <div class="panel-header">
            <h3>Результат</h3>
            <div class="panel-actions">
              <button type="button" class="small-btn" onclick="clearOutput()">Clear</button>
              <button type="button" class="small-btn" onclick="copyOutput()">Copy</button>
            </div>
          </div>

          <div id="outputJson" class="json-output">)HTML";
    html += output_json_html.empty() ? std::string() : output_json_html;
    html += R"HTML(</div>
        </div>
      </div>
    </form>)HTML";

    if (!error_message.empty()) {
        html += R"HTML(<div class="error">)HTML";
        html += html_escape(error_message);
        html += R"HTML(</div>)HTML";
    }

    html += R"HTML(
  </div>

  <script>
    function clearInput() {
      document.getElementById('inputJson').value = '';
    }

    function clearOutput() {
      document.getElementById('outputJson').innerHTML = '';
    }

    async function copyInput() {
      const text = document.getElementById('inputJson').value;
      await navigator.clipboard.writeText(text);
    }

    async function copyOutput() {
      const el = document.getElementById('outputJson');
      const text = el.innerText;
      await navigator.clipboard.writeText(text);
    }
  </script>
</body>
</html>)HTML";
    return html;
}
