#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define OLLAMA_HOST "127.0.0.1"
#define OLLAMA_PORT 11434
#define MODEL "qwen2.5-coder:3b"

SOCKET sock_connect(const char *host, int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return INVALID_SOCKET;
    return s;
}

std::string escape_json(const std::string &s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string unicode_escape_to_utf8(const std::string &str) {
    std::string out;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 5 < str.length() && str[i+1] == 'u') {
            unsigned int cp = 0;
            for (int j = 2; j < 6; j++) {
                char c = str[i+j];
                if (c >= '0' && c <= '9') cp = (cp << 4) | (c - '0');
                else if (c >= 'a' && c <= 'f') cp = (cp << 4) | (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') cp = (cp << 4) | (c - 'A' + 10);
            }
            i += 5;
            if (cp < 0x80) out += (char)cp;
            else if (cp < 0x800) { out += (char)(0xC0|(cp>>6)); out += (char)(0x80|(cp&0x3F)); }
            else if (cp < 0x10000) { out += (char)(0xE0|(cp>>12)); out += (char)(0x80|((cp>>6)&0x3F)); out += (char)(0x80|(cp&0x3F)); }
            else { out += (char)(0xF0|(cp>>18)); out += (char)(0x80|((cp>>12)&0x3F)); out += (char)(0x80|((cp>>6)&0x3F)); out += (char)(0x80|(cp&0x3F)); }
        } else {
            out += str[i];
        }
    }
    return out;
}

std::string extract_stream_token(const std::string &line) {
    std::string key = "\"content\":\"";
    size_t pos = line.find(key);
    if (pos == std::string::npos) return "";
    pos += key.length();
    std::string token;
    while (pos < line.length()) {
        if (line[pos] == '"' && (pos == 0 || line[pos - 1] != '\\')) break;
        if (line[pos] == '\\' && pos + 1 < line.length()) {
            switch (line[pos + 1]) {
                case '"': token += '"'; pos += 2; continue;
                case '\\': token += '\\'; pos += 2; continue;
                case 'n': token += '\n'; pos += 2; continue;
                case 'r': pos += 2; continue;
                case 't': token += '\t'; pos += 2; continue;
                case 'u': {
                    std::string esc = line.substr(pos, 6);
                    token += unicode_escape_to_utf8(esc);
                    pos += 6;
                    continue;
                }
                default: token += line[pos]; pos++; continue;
            }
        }
        token += line[pos];
        pos++;
    }
    return token;
}

std::string stream_chat(const char *body) {
    SOCKET s = sock_connect(OLLAMA_HOST, OLLAMA_PORT);
    if (s == INVALID_SOCKET) return "[connection error]";

    char request[65536];
    snprintf(request, sizeof(request),
        "POST /api/chat HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
        OLLAMA_HOST, OLLAMA_PORT, (int)strlen(body), body);

    send(s, request, strlen(request), 0);

    std::string full_reply;
    std::string buffer;
    char chunk[4096];
    int n;

    while ((n = recv(s, chunk, sizeof(chunk)-1, 0)) > 0) {
        chunk[n] = '\0';
        buffer += chunk;

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (line.empty()) continue;

            std::string token = extract_stream_token(line);
            if (!token.empty()) {
                full_reply += token;
                printf("%s", token.c_str());
                fflush(stdout);
            }
            if (line.find("\"done\":true") != std::string::npos) goto done;
        }
    }
done:
    closesocket(s);
    return full_reply;
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);

    printf("=== Chat with %s (streaming) ===\n", MODEL);
    printf("Type 'exit' to quit, 'clear' to reset history\n\n");

    std::vector<std::pair<std::string, std::string>> history;

    while (true) {
        printf("You: ");
        fflush(stdout);

        char input[4096];
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\r\n")] = 0;

        if (strlen(input) == 0) continue;
        if (strcmp(input, "exit") == 0) break;
        if (strcmp(input, "clear") == 0) {
            history.clear();
            printf("[History cleared]\n\n");
            continue;
        }

        history.push_back({"user", input});

        std::string messages_json = "[";
        for (size_t i = 0; i < history.size(); i++) {
            if (i > 0) messages_json += ",";
            messages_json += "{\"role\":\"" + history[i].first + "\",\"content\":\"" + escape_json(history[i].second) + "\"}";
        }
        messages_json += "]";

        std::string body = "{\"model\":\"" + std::string(MODEL) + "\",\"messages\":" + messages_json + ",\"stream\":true}";

        printf("\n%s: ", MODEL);
        fflush(stdout);

        std::string reply = stream_chat(body.c_str());

        if (!reply.empty() && reply[0] != '[') {
            history.push_back({"assistant", reply});
        }
        printf("\n\n");
    }

    WSACleanup();
    printf("Bye!\n");
    return 0;
}
