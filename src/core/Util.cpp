#include "Util.h"
#include <Shlobj.h>
#include <Psapi.h>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace Glacier::Util {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string wideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    auto b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, b - a + 1);
}

std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    std::stringstream ss(s);
    while (std::getline(ss, cur, d)) out.push_back(cur);
    return out;
}

std::wstring appDataPath(const std::wstring& sub) {
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        result = path;
        result += L"\\Glacier";
        if (!sub.empty()) {
            result += L"\\";
            result += sub;
        }
        CoTaskMemFree(path);
    }
    CreateDirectoryW(result.c_str(), nullptr); // best-effort; ignore failure
    return result;
}

std::wstring moduleDir() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&moduleDir), &self);
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(self, buf, MAX_PATH);
    std::wstring p = buf;
    auto slash = p.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? L"." : p.substr(0, slash);
}

HMODULE moduleHandle(const wchar_t* name) { return GetModuleHandleW(name); }

static bool patternMatch(const std::uint8_t* data, const std::vector<int>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] < 0) continue;
        if (data[i] != static_cast<std::uint8_t>(bytes[i])) return false;
    }
    return true;
}

static std::vector<int> parsePattern(const char* p) {
    std::vector<int> out;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (*p == '?') {
            out.push_back(-1);
            while (*p == '?') ++p;
        } else {
            char c[3] = { p[0], (p[1] && p[1] != ' ') ? p[1] : '\0', '\0' };
            out.push_back(static_cast<int>(std::strtol(c, nullptr, 16)));
            while (*p && *p != ' ') ++p;
        }
    }
    return out;
}

std::uintptr_t patternScan(HMODULE mod, const char* pattern) {
    if (!mod) return 0;
    MODULEINFO mi{};
    if (!GetModuleInformation(GetCurrentProcess(), mod, &mi, sizeof(mi))) return 0;
    auto bytes = parsePattern(pattern);
    if (bytes.empty()) return 0;
    auto* base = reinterpret_cast<std::uint8_t*>(mi.lpBaseOfDll);
    auto end = mi.SizeOfImage - bytes.size();
    for (size_t i = 0; i < end; ++i) {
        if (patternMatch(base + i, bytes)) return reinterpret_cast<std::uintptr_t>(base + i);
    }
    return 0;
}

} // namespace Glacier::Util
