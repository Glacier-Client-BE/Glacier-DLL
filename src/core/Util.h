#pragma once
#include <string>
#include <vector>
#include <Windows.h>

namespace Glacier::Util {

// String conversion
std::wstring utf8ToWide (const std::string&  s);
std::string  wideToUtf8 (const std::wstring& s);

// Trim/split
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim);

// %APPDATA%/Glacier/<sub>
std::wstring appDataPath(const std::wstring& sub = L"");

// Get the directory of the loaded DLL (used for logs/icons fallback)
std::wstring moduleDir();

// Find a loaded module's image base; returns nullptr if not loaded.
HMODULE moduleHandle(const wchar_t* name);

// Pattern scan in a loaded module.  Pattern format:  "48 8B ? ? 89 ?? ?"
// '?' or '??' = wildcard byte.  Returns 0 if not found.
std::uintptr_t patternScan(HMODULE mod, const char* pattern);

} // namespace Glacier::Util
