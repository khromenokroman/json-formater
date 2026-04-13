#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

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

static std::string build_html_page(const std::string& input_json = "", const std::string& output_json = "", const std::string& status = "") {
    std::ostringstream html;
    html << R"(<!doctype html>
<html lang="ru">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>JSON Formatter</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 24px;
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
      padding: 12px;
      border: 1px solid #cbd5e1;
      border-radius: 10px;
      font-family: Consolas, monospace;
      font-size: 14px;
      resize: vertical;
      box-sizing: border-box;
      background: white;
    }
    .actions {
      margin: 16px 0;
      display: flex;
      gap: 12px;
      align-items: center;
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
      color: #dc2626;
      font-size: 14px;
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
    <p class="hint">Вставь JSON слева, нажми кнопку, и отформатированный результат появится справа.</p>

    <form method="POST" action="/format">
      <div class="grid">
        <div>
          <h3>Неформатированный JSON</h3>
          <textarea name="json" placeholder='{"name":"Alex","age":20,"items":[1,2,3]}'>)";
    html << html_escape(input_json);
    html << R"(</textarea>
        </div>
        <div>
          <h3>Форматированный JSON</h3>
          <textarea readonly>)";
    html << html_escape(output_json);
    html << R"(</textarea>
        </div>
      </div>

      <div class="actions">
        <button type="submit">Format</button>
      </div>
    </form>)";

    if (!status.empty()) {
        html << R"(<div class="error">)" << html_escape(status) << R"(</div>)";
    }

    html << R"(
  </div>
</body>
</html>)";
    return html.str();
}

static std::string get_body(const std::string& request) {
    const std::string sep = "\r\n\r\n";
    size_t pos = request.find(sep);
    if (pos == std::string::npos) return {};
    return request.substr(pos + sep.size());
}

static std::string make_response(const std::string& body, const std::string& content_type = "text/html; charset=UTF-8", const std::string& status = "200 OK") {
    std::ostringstream resp;
    resp << "HTTP/1.1 " << status << "\r\n";
    resp << "Content-Type: " << content_type << "\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    resp << body;
    return resp.str();
}

static void handle_client(int client_fd) {
    std::string request(8192, '\0');
    ssize_t received = recv(client_fd, request.data(), request.size() - 1, 0);
    if (received <= 0) {
        close(client_fd);
        return;
    }
    request.resize(received);

    std::istringstream req_stream(request);
    std::string method, path, version;
    req_stream >> method >> path >> version;

    if (method == "GET" && path == "/") {
        std::string body = build_html_page();
        std::string resp = make_response(body);
        send(client_fd, resp.c_str(), resp.size(), 0);
    } else if (method == "POST" && path == "/format") {
        std::string body = get_body(request);
        std::string input_json;

        if (body.rfind("json=", 0) == 0) {
            input_json = url_decode(body.substr(5));
        } else {
            input_json = body;
        }

        std::string output_json;
        std::string error;

        try {
            json parsed = json::parse(input_json);
            output_json = parsed.dump(4);
        } catch (const std::exception& e) {
            error = std::string("Ошибка JSON: ") + e.what();
        }

        std::string page = build_html_page(input_json, output_json, error);
        std::string resp = make_response(page);
        send(client_fd, resp.c_str(), resp.size(), 0);
    } else {
        std::string body = "Not Found";
        std::string resp = make_response(body, "text/plain; charset=UTF-8", "404 Not Found");
        send(client_fd, resp.c_str(), resp.size(), 0);
    }

    close(client_fd);
}

int main() {
    const int port = 8080;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Ошибка: не удалось создать socket\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Ошибка: bind failed\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        std::cerr << "Ошибка: listen failed\n";
        close(server_fd);
        return 1;
    }

    std::cout << "JSON formatter server started: http://localhost:" << port << std::endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            continue;
        }

        std::thread(handle_client, client_fd).detach();
    }

    close(server_fd);
    return 0;
}