#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

static std::string url_decode(const std::string& src) {
    std::string out;
    out.reserve(src.size());

    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < src.size()) {
            auto hex = [](char x) -> int {
                if (x >= '0' && x <= '9') return x - '0';
                if (x >= 'a' && x <= 'f') return 10 + (x - 'a');
                if (x >= 'A' && x <= 'F') return 10 + (x - 'A');
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

static std::string json_to_highlighted_html(const json& value) {
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
                if (std::isdigit(static_cast<unsigned char>(nc)) || nc == '.' || nc == 'e' || nc == 'E' ||
                    nc == '+' || nc == '-') {
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

static std::string render_page(const std::string& input_json,
                               const std::string& output_json_html,
                               const std::string& error_message) {
    std::string html;
    html += R"(<!doctype html>
<html lang="ru">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>JSON Formatter</title>
  <style>
    body {
      margin: 0;
      padding: 24px;
      font-family: Arial, sans-serif;
      background: #f5f7fb;
      color: #1f2937;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    h1 {
      margin-top: 0;
    }
    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 16px;
    }
    textarea, .json-output {
      width: 100%;
      min-height: 420px;
      box-sizing: border-box;
      padding: 12px;
      border: 1px solid #cbd5e1;
      border-radius: 10px;
      font-family: Consolas, monospace;
      font-size: 14px;
      resize: vertical;
      background: white;
    }
    textarea {
      white-space: pre;
    }
    .json-output {
      overflow: auto;
      white-space: pre-wrap;
      word-break: break-word;
    }
    .actions {
      margin: 16px 0;
    }
    button {
      padding: 10px 18px;
      border: none;
      border-radius: 10px;
      background: #2563eb;
      color: white;
      font-size: 14px;
      cursor: pointer;
    }
    button:hover {
      background: #1d4ed8;
    }
    .hint {
      color: #6b7280;
      font-size: 14px;
    }
    .error {
      margin-top: 16px;
      color: #dc2626;
      white-space: pre-wrap;
    }
    .json-key { color: #7c3aed; font-weight: bold; }
    .json-string { color: #15803d; }
    .json-number { color: #d97706; }
    .json-bool { color: #2563eb; font-weight: bold; }
    .json-null { color: #dc2626; font-weight: bold; }
    @media (max-width: 900px) {
      .grid {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>JSON Formatter</h1>
    <p class="hint">Вставь JSON слева и нажми кнопку форматирования.</p>

    <form method="post" action="/format">
      <div class="grid">
        <div>
          <h3>Неформатированный JSON</h3>
          <textarea name="json" placeholder='{"name":"Alex","age":20,"items":[1,2,3]}'>)";
    html += html_escape(input_json);
    html += R"(</textarea>
        </div>
        <div>
          <h3>Форматированный JSON</h3>
          <div class="json-output">)";
    html += output_json_html.empty() ? std::string() : output_json_html;
    html += R"(</div>
        </div>
      </div>

      <div class="actions">
        <button type="submit">Format</button>
      </div>
    </form>)";

    if (!error_message.empty()) {
        html += R"(<div class="error">)";
        html += html_escape(error_message);
        html += R"(</div>)";
    }

    html += R"(
  </div>
</body>
</html>)";
    return html;
}

static std::string extract_json_from_form(const std::string& body) {
    const std::string prefix = "json=";
    if (body.rfind(prefix, 0) == 0) {
        return url_decode(body.substr(prefix.size()));
    }
    return body;
}

int main() {
    httplib::Server server;

    server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(render_page("", "", ""), "text/html; charset=UTF-8");
    });

    server.Post("/format", [](const httplib::Request& req, httplib::Response& res) {
        std::string input_json = extract_json_from_form(req.body);
        std::string output_json_html;
        std::string error_message;

        try {
            json parsed = json::parse(input_json);
            output_json_html = json_to_highlighted_html(parsed);
        } catch (const std::exception& e) {
            error_message = std::string("Ошибка JSON: ") + e.what();
        }

        res.set_content(render_page(input_json, output_json_html, error_message), "text/html; charset=UTF-8");
    });

    std::cout << "Server started: http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);
    return 0;
}