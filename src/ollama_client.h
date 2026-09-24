// GOIDA ollama client — общие url/http хелперы (этап1 рефактор)
#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include "utf8.h"

namespace goida::ollama {

struct UrlParts {
    std::wstring host;
    std::wstring path = L"/";
    int port = 80;
    bool https = false;
    bool ok = false;
};

inline UrlParts ParseUrl(const std::wstring& url) {
    UrlParts p;
    p.https = (url.find(L"https://")==0);
    size_t s = url.find(L"://"); if (s==std::wstring::npos) return p;
    size_t st = s+3, sl = url.find(L'/', st);
    std::wstring hp = (sl==std::wstring::npos) ? url.substr(st) : url.substr(st, sl-st);
    p.path = (sl==std::wstring::npos) ? L"/" : url.substr(sl);
    size_t co = hp.find(L':');
    if (co!=std::wstring::npos) { p.host = hp.substr(0,co); p.port = _wtoi(hp.substr(co+1).c_str()); }
    else { p.host = hp; p.port = p.https?443:80; }
    p.ok = !p.host.empty();
    return p;
}

inline std::wstring ApiBase(const std::wstring& api_url) {
    size_t sp = api_url.rfind(L'/');
    if (sp!=std::wstring::npos) return api_url.substr(0, sp);
    return api_url;
}

inline std::wstring BuildPullUrl(const std::wstring& api_url) { return ApiBase(api_url)+L"/pull"; }
inline std::wstring BuildDeleteUrl(const std::wstring& api_url) { return ApiBase(api_url)+L"/delete"; }
inline std::wstring BuildShowUrl(const std::wstring& api_url) { return ApiBase(api_url)+L"/show"; }
inline std::wstring BuildEmbedUrl(const std::wstring& api_url) { return ApiBase(api_url)+L"/embed"; }
inline std::wstring BuildTagsUrl(const std::wstring& api_url) { (void)api_url; return L"/api/tags"; }
inline std::wstring BuildPsUrl(const std::wstring& api_url) { return ApiBase(api_url)+L"/ps"; }
inline std::wstring BuildVersionUrl(const std::wstring& api_url) { return ApiBase(api_url)+L"/version"; }

} // namespace goida::ollama
