// GOIDA application configuration persisted in SQLite
#pragma once
#include <windows.h>
#include <string>
#include "db.h"

struct Config {
    std::wstring api_url = L"http://localhost:11434/api/chat";
    std::wstring model = L"qwen:14b";
    float temp = 0.7f;
    int max_tokens = 4096;
    long long seed = -1;
    std::wstring sys_prompt = L"You are a helpful AI assistant.";
    bool json_mode = false;
    std::wstring lang = L"ru";
    std::wstring theme = L"dark";
    std::wstring accent = L"blue";
    bool rounded = true;
    bool adaptive = true;
    std::wstring f_name = L"femboy", f_voice = L"soft", f_eyes = L"round", f_hair = L"short";
    std::wstring f_top = L"tank", f_bottom = L"briefs", f_feet = L"sneakers", f_ears = L"normal";
    std::wstring update_url = L"https://example.com/goida-latest.exe";

    void Load() {
        api_url = DBGet(L"api_url", api_url.c_str());
        model = DBGet(L"model", model.c_str());
        std::wstring t = DBGet(L"temp"); if (!t.empty()) temp = static_cast<float>(_wtof(t.c_str()));
        std::wstring m = DBGet(L"max_tokens"); if (!m.empty()) max_tokens = _wtoi(m.c_str());
        std::wstring sd = DBGet(L"seed"); if (!sd.empty()) seed = _wtoi64(sd.c_str());
        sys_prompt = DBGet(L"sys_prompt", sys_prompt.c_str());
        std::wstring jm = DBGet(L"json_mode"); if (!jm.empty()) json_mode = (_wtoi(jm.c_str()) != 0);
        lang = DBGet(L"lang", lang.c_str());
        theme = DBGet(L"theme", theme.c_str());
        accent = DBGet(L"accent", accent.c_str());
        std::wstring rd = DBGet(L"rounded"); if (!rd.empty()) rounded = (_wtoi(rd.c_str()) != 0);
        std::wstring ad = DBGet(L"adaptive"); if (!ad.empty()) adaptive = (_wtoi(ad.c_str()) != 0);
        f_name = DBGet(L"f_name", f_name.c_str()); f_voice = DBGet(L"f_voice", f_voice.c_str());
        f_eyes = DBGet(L"f_eyes", f_eyes.c_str()); f_hair = DBGet(L"f_hair", f_hair.c_str());
        f_top = DBGet(L"f_top", f_top.c_str()); f_bottom = DBGet(L"f_bottom", f_bottom.c_str());
        f_feet = DBGet(L"f_feet", f_feet.c_str()); f_ears = DBGet(L"f_ears", f_ears.c_str());
        update_url = DBGet(L"update_url", update_url.c_str());
    }

    void Save() {
        DBSet(L"api_url", api_url.c_str()); DBSet(L"model", model.c_str());
        wchar_t b[64]; swprintf(b, L"%.2f", temp); DBSet(L"temp", b);
        swprintf(b, L"%d", max_tokens); DBSet(L"max_tokens", b);
        swprintf(b, L"%lld", seed); DBSet(L"seed", b);
        DBSet(L"sys_prompt", sys_prompt.c_str());
        DBSet(L"json_mode", json_mode ? L"1" : L"0");
        DBSet(L"lang", lang.c_str()); DBSet(L"theme", theme.c_str()); DBSet(L"accent", accent.c_str());
        DBSet(L"rounded", rounded ? L"1" : L"0"); DBSet(L"adaptive", adaptive ? L"1" : L"0");
        DBSet(L"f_name", f_name.c_str()); DBSet(L"f_voice", f_voice.c_str());
        DBSet(L"f_eyes", f_eyes.c_str()); DBSet(L"f_hair", f_hair.c_str());
        DBSet(L"f_top", f_top.c_str()); DBSet(L"f_bottom", f_bottom.c_str());
        DBSet(L"f_feet", f_feet.c_str()); DBSet(L"f_ears", f_ears.c_str());
        DBSet(L"update_url", update_url.c_str());
    }
};
