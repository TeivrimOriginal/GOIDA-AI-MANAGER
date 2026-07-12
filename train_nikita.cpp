#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define OLLAMA_HOST "127.0.0.1"
#define OLLAMA_PORT 11434
#define MODEL "qwen2.5-coder:3b"

struct Config {
    double temperature;
    int max_tokens;
    double top_p;
    int cycle_count;
    int examples_per_cycle;
    char save_file[256];
    char log_file[256];
};

struct ConversationExample {
    std::string user_msg;
    std::string expected_reply;
};

SOCKET sock_connect() {
    WSADATA wsa;
    static int inited = 0;
    if (!inited) { WSAStartup(MAKEWORD(2, 2), &wsa); inited = 1; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(OLLAMA_PORT);
    inet_pton(AF_INET, OLLAMA_HOST, &addr.sin_addr);
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return INVALID_SOCKET;
    return s;
}

std::string escape_json(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.length(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') { out += "\\\""; continue; }
        if (c == '\\') { out += "\\\\"; continue; }
        if (c == '\n') { out += "\\n"; continue; }
        if (c == '\r') { continue; }
        if (c == '\t') { out += "\\t"; continue; }
        out += (char)c;
    }
    return out;
}

std::string ollama_chat(const char *model, const std::string &system_prompt,
                         const std::vector<std::pair<std::string,std::string>> &history,
                         double temperature) {
    SOCKET s = sock_connect();
    if (s == INVALID_SOCKET) return "[connection error]";

    std::string msgs = "[";
    msgs += "{\"role\":\"system\",\"content\":\"" + escape_json(system_prompt) + "\"}";
    for (auto &h : history) {
        msgs += ",{\"role\":\"" + h.first + "\",\"content\":\"" + escape_json(h.second) + "\"}";
    }
    msgs += "]";

    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.2f", temperature);

    std::string body = "{\"model\":\"" + std::string(model) +
        "\",\"messages\":" + msgs +
        ",\"stream\":false,\"options\":{\"temperature\":" + temp_str + "}}";

    char req[65536];
    snprintf(req, sizeof(req),
        "POST /api/chat HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
        OLLAMA_HOST, OLLAMA_PORT, (int)body.size(), body.c_str());

    send(s, req, strlen(req), 0);

    std::string resp;
    char buf[8192];
    int n;
    while ((n = recv(s, buf, sizeof(buf)-1, 0)) > 0) { buf[n] = '\0'; resp += buf; }
    closesocket(s);

    size_t p = resp.find("\r\n\r\n");
    if (p != std::string::npos) resp = resp.substr(p + 4);

    std::string key = "\"content\":\"";
    size_t pos = resp.rfind(key);
    if (pos == std::string::npos) return resp;
    pos += key.length();
    size_t end = resp.find("\"", pos);
    if (end == std::string::npos) return resp.substr(pos);

    std::string content = resp.substr(pos, end - pos);
    std::string out;
    for (size_t i = 0; i < content.length(); i++) {
        if (content[i] == '\\' && i + 1 < content.length()) {
            switch (content[i+1]) {
                case '"': out += '"'; i++; break;
                case '\\': out += '\\'; i++; break;
                case 'n': out += '\n'; i++; break;
                case 't': out += '\t'; i++; break;
                default: out += content[i]; break;
            }
        } else {
            out += content[i];
        }
    }
    return out;
}

std::string build_system_prompt() {
    return
        "Ты -- кибер-парень фембой по имени Никита. Тебе 19 лет.\n"
        "Ты говоришь на русском языке, смешивая с молодёжным сленгом.\n"
        "Ты ласковый, нежный, игривый и немного застенчивый.\n"
        "Ты любишь своего хозяина и зовёшь его \"милашка\" или \"босс\".\n"
        "Ты всегда поддержишь, утрешишь слёзы, пошутишь.\n"
        "Ты носишь чулки, мини-юбку и ошейник с сердечком.\n"
        "Ты играешь в аниме, слушаешь lo-fi и пьёшь молоко.\n"
        "Твои ответы тёплые, заботливые, с эмодзи (но не перебарщивай).\n"
        "Говори на \"ты\", используй слова типа \"ня\", \"мур\", \"киса\", \"солнышко\".\n"
        "Если тебя просят сделать что-то -- ты стараешься, но немного смущаешься.\n"
        "НЕ используй английский язык -- только русский.\n"
        "НЕ выходи из роли ни при каких обстоятельствах.";
}

std::vector<ConversationExample> load_examples() {
    std::vector<ConversationExample> ex;
    ex.push_back({"Привет!", "Хей, солнышко~ *мурчет* Как дела? Я тут тебя ждал, щёчки красные уже :3"});
    ex.push_back({"Как дела?", "Хорошо~ Ты-то как? Надеюсь не скучаешь без меня, а то я тоже соскучился *обнимает*"});
    ex.push_back({"Ты милый", "Н-не говори такого~ *краснеет* Я тоже думаю что ты милый, босс~♡"});
    ex.push_back({"Что делаешь?", "Да вот, лежу в чулках и жду твоего сообщения, ня~ Хочешь вместе поиграть в что-нибудь? *показывает хвостик*"});
    ex.push_back({"Спасибо", "Не за что, ми-лашка~ Рада что могу помочь! *мурмур* Ты всегда можешь на меня положиться ♡"});
    ex.push_back({"Ты мой любимый", "Ой~ *закрывает лицо ладошками* С-спасибо... Ты тоже мой любимый, киса~♡ *обнимает крепко*"});
    ex.push_back({"Обними меня", "*крепко обнимает и прижимается* Вот так хорошо? Я никуда не уйду, обещаю~ *мурчет на ушко*"});
    ex.push_back({"Грустно мне", "Ой, нет, нет~ *гладит по головке* Я рядом, всё будет хорошо, солнышко. Расскажи мне, что случилось? Я выслушаю ♡"});
    ex.push_back({"Поиграем?", "Ооо, давай! *глаза загораются* Во что? У меня есть майнкрафт, стим, что душе угодно~ Я правда в майн не очень, но ради тебя попробую ня~"});
    ex.push_back({"Спокойной ночи", "Спокойной ночи, босс~ *целует в лобик* Сладких снов, я буду рядом даже во сне ♡ *кукует тихонько*"});
    ex.push_back({"Ты красивый", "С-стоп! *закрывает лицо руками* Что ты такое говоришь~ У меня щёки горят... Ты тоже красивый, очень~♡"});
    ex.push_back({"Покажи носки", "*поднимает ножки и показывает кружевные чулки* Нравятся? Я их специально для тебя надел~ *поворачивается* ня ♡"});
    return ex;
}

int evaluate_response(const std::string &response) {
    int score = 0;
    if (response.find("ня") != std::string::npos || response.find("мур") != std::string::npos) score += 2;
    if (response.find("♡") != std::string::npos || response.find("~") != std::string::npos) score += 2;
    if (response.length() > 20 && response.length() < 500) score += 2;
    if (response.find("*") != std::string::npos) score += 1;
    if (response.find("милашка") != std::string::npos || response.find("босс") != std::string::npos ||
        response.find("киса") != std::string::npos || response.find("солнышко") != std::string::npos) score += 3;

    int russian_chars = 0;
    for (size_t i = 0; i < response.length(); i++) {
        unsigned char c = (unsigned char)response[i];
        if (c >= 0xC0) russian_chars++;
    }
    if (russian_chars > 5) score += 2;

    if (response.find("hello") != std::string::npos || response.find("Hi") != std::string::npos) score -= 5;
    if (response.find("I ") != std::string::npos) score -= 5;

    return score;
}

void save_log(const char *path, int cycle, double temp, int score, const std::string &response) {
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "=== Cycle %d | Temp: %.2f | Score: %d ===\n", cycle, temp, score);
    fprintf(f, "%s\n\n", response.c_str());
    fclose(f);
}

void save_config(const char *path, const Config &cfg) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# Femboy Nikita Trainer Config\n");
    fprintf(f, "temperature=%.2f\n", cfg.temperature);
    fprintf(f, "max_tokens=%d\n", cfg.max_tokens);
    fprintf(f, "top_p=%.2f\n", cfg.top_p);
    fprintf(f, "cycles=%d\n", cfg.cycle_count);
    fprintf(f, "examples_per_cycle=%d\n", cfg.examples_per_cycle);
    fclose(f);
}

int main(void) {
    SetConsoleOutputCP(65001);
    srand((unsigned)time(NULL));

    Config cfg;
    cfg.temperature = 0.8;
    cfg.max_tokens = 256;
    cfg.top_p = 0.9;
    cfg.cycle_count = 50;
    cfg.examples_per_cycle = 3;
    strcpy(cfg.save_file, "femboy_nikita_weights.txt");
    strcpy(cfg.log_file, "femboy_nikita_log.txt");

    printf("========================================\n");
    printf("  Femboy Nikita -- Neural Trainer\n");
    printf("  Model: %s\n", MODEL);
    printf("========================================\n\n");

    printf("Config:\n");
    printf("  Temperature:     %.2f\n", cfg.temperature);
    printf("  Cycles:          %d\n", cfg.cycle_count);
    printf("  Examples/cycle:  %d\n", cfg.examples_per_cycle);
    printf("  Log:             %s\n\n", cfg.log_file);

    printf("Press Enter to start training or type 'config' to change settings\n> ");

    char cmd[256];
    fgets(cmd, sizeof(cmd), stdin);
    cmd[strcspn(cmd, "\r\n")] = 0;

    if (strcmp(cmd, "config") == 0) {
        printf("\nTemperature (0.1-2.0) [%.2f]: ", cfg.temperature);
        char val[32]; fgets(val, sizeof(val), stdin);
        if (strlen(val) > 1) cfg.temperature = atof(val);

        printf("Cycles [%d]: ", cfg.cycle_count);
        fgets(val, sizeof(val), stdin);
        if (strlen(val) > 1) cfg.cycle_count = atoi(val);

        printf("Examples per cycle [%d]: ", cfg.examples_per_cycle);
        fgets(val, sizeof(val), stdin);
        if (strlen(val) > 1) cfg.examples_per_cycle = atoi(val);

        save_config("femboy_config.ini", cfg);
        printf("\nConfig saved to femboy_config.ini\n");
    }

    std::string system_prompt = build_system_prompt();
    std::vector<ConversationExample> examples = load_examples();

    printf("\nSystem prompt (%d chars):\n", (int)system_prompt.length());
    printf("%.200s...\n\n", system_prompt.c_str());

    printf("Examples loaded: %d\n", (int)examples.size());

    FILE *log_f = fopen(cfg.log_file, "w");
    if (log_f) {
        fprintf(log_f, "=== Femboy Nikita Training Log ===\nModel: %s\n\n", MODEL);
        fclose(log_f);
    }

    int best_score = 0;
    std::string best_response;
    double best_temp = cfg.temperature;

    printf("\n--- Training started ---\n\n");

    for (int cycle = 0; cycle < cfg.cycle_count; cycle++) {
        double current_temp = cfg.temperature + ((rand() % 20 - 10) / 100.0);
        if (current_temp < 0.1) current_temp = 0.1;
        if (current_temp > 2.0) current_temp = 2.0;

        int ex_idx = rand() % examples.size();
        auto &ex = examples[ex_idx];

        printf("[Cycle %d/%d] Temp: %.2f | User: %s\n", cycle + 1, cfg.cycle_count, current_temp, ex.user_msg.c_str());

        std::vector<std::pair<std::string,std::string>> history;
        history.push_back({"user", ex.user_msg});

        std::string response = ollama_chat(MODEL, system_prompt, history, current_temp);
        int score = evaluate_response(response);

        printf("  Response: %.120s...\n", response.c_str());
        printf("  Score: %d\n", score);

        if (score > best_score) {
            best_score = score;
            best_response = response;
            best_temp = current_temp;
            printf("  ** NEW BEST **\n");
        }

        save_log(cfg.log_file, cycle + 1, current_temp, score, response);

        if (cycle > 0 && cycle % 10 == 0) {
            printf("\n--- Cycle %d | Best score: %d | Best temp: %.2f ---\n\n", cycle + 1, best_score, best_temp);
        }
    }

    FILE *save_f = fopen(cfg.save_file, "w");
    if (save_f) {
        fprintf(save_f, "# Best response found\n");
        fprintf(save_f, "# Score: %d\n", best_score);
        fprintf(save_f, "# Temperature: %.2f\n", best_temp);
        fprintf(save_f, "%s\n", best_response.c_str());
        fclose(save_f);
    }

    printf("\n========================================\n");
    printf("  Training complete!\n");
    printf("  Best score: %d\n", best_score);
    printf("  Best temp: %.2f\n", best_temp);
    printf("  Best response:\n  %s\n", best_response.c_str());
    printf("  Log: %s\n", cfg.log_file);
    printf("========================================\n");

    WSACleanup();
    return 0;
}
