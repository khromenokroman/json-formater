#include <iostream>
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

static std::string render_page(const std::string& input_json, const std::string& output_json, const std::string& error_message) {
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
    textarea {
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
          <textarea readonly>)";
    html += html_escape(output_json);
    html += R"(</textarea>
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
        std::string value = body.substr(prefix.size());

        std::string decoded;
        decoded.reserve(value.size());

        for (size_t i = 0; i < value.size(); ++i) {
            char c = value[i];
            if (c == '+') {
                decoded += ' ';
            } else if (c == '%' && i + 2 < value.size()) {
                auto hex = [](char x) -> int {
                    if (x >= '0' && x <= '9') return x - '0';
                    if (x >= 'a' && x <= 'f') return 10 + (x - 'a');
                    if (x >= 'A' && x <= 'F') return 10 + (x - 'A');
                    return -1;
                };
                int hi = hex(value[i + 1]);
                int lo = hex(value[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    decoded += static_cast<char>((hi << 4) | lo);
                    i += 2;
                } else {
                    decoded += c;
                }
            } else {
                decoded += c;
            }
        }

        return decoded;
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
        std::string output_json;
        std::string error_message;

        try {
            json parsed = json::parse(input_json);
            output_json = parsed.dump(4);
        } catch (const std::exception& e) {
            error_message = std::string("Ошибка JSON: ") + e.what();
        }

        res.set_content(render_page(input_json, output_json, error_message), "text/html; charset=UTF-8");
    });

    std::cout << "Server started: http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);
    return 0;
}