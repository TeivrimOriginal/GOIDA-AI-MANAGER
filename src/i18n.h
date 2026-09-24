// GOIDA i18n — ru/en JSON loader, UTF-8, fallback to key
#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include "utf8.h"

namespace goida::i18n {

class I18n {
    std::unordered_map<std::string, std::string> dict_;
    std::string lang_="en";
public:
    bool load(const wchar_t* path, const std::string& lang) {
        lang_=lang;
        dict_.clear();
        std::string data = utf8::read_file_utf8(path);
        // Fallback: also try std::ifstream if CreateFile failed
        if (data.empty()) {
            char mbPath[512]={}; WideCharToMultiByte(CP_UTF8,0,path,-1,mbPath,sizeof(mbPath),nullptr,nullptr);
            std::ifstream f(mbPath, std::ios::binary);
            if (f) { data.assign(std::istreambuf_iterator<char>(f), {}); if(data.size()>=3 && (unsigned char)data[0]==0xEF) data.erase(0,3); }
        }
        if (data.empty()) return false;
        // Very small JSON parser: extract "key": "value"
        size_t pos=0;
        while (true) {
            size_t k1 = data.find('"', pos);
            if (k1==std::string::npos) break;
            size_t k2 = data.find('"', k1+1);
            if (k2==std::string::npos) break;
            std::string key = data.substr(k1+1, k2-k1-1);
            size_t colon = data.find(':', k2);
            if (colon==std::string::npos) break;
            size_t v1 = data.find('"', colon);
            if (v1==std::string::npos) break;
            // find closing quote handling escapes
            size_t v2 = v1+1;
            std::string val;
            while (v2 < data.size()) {
                if (data[v2]=='"' && data[v2-1]!='\\') break;
                if (data[v2]=='\\' && v2+1 < data.size()) {
                    char esc = data[v2+1];
                    if (esc=='n') val.push_back('\n');
                    else if (esc=='r') val.push_back('\r');
                    else if (esc=='t') val.push_back('\t');
                    else if (esc=='"') val.push_back('"');
                    else if (esc=='\\') val.push_back('\\');
                    else { val.push_back('\\'); val.push_back(esc); }
                    v2+=2; continue;
                }
                val.push_back(data[v2]); v2++;
            }
            // data[v2] is closing quote
            dict_[key]=val;
            pos = v2+1;
        }
        return !dict_.empty();
    }
    std::string get(const std::string& key) const {
        auto it = dict_.find(key);
        if (it!=dict_.end()) return it->second;
        return key;
    }
    std::wstring wget(const std::string& key) const {
        return utf8::utf8_to_w(get(key));
    }
    const std::string& lang() const { return lang_; }
    size_t size() const { return dict_.size(); }
};

inline I18n& instance() { static I18n inst; return inst; }
inline std::string t(const std::string& k){ return instance().get(k); }
inline std::wstring wt(const std::string& k){ return instance().wget(k); }

} // namespace goida::i18n
