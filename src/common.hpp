#pragma once
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <sddl.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include <cstring>
#include <cstdint>
namespace w3 {
namespace fs = std::filesystem;
inline void require(bool ok, const char *s) {
    if (!ok)
        throw std::runtime_error(s);
}
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE p) : h(p) {}
    ~Handle() {
        if (h != INVALID_HANDLE_VALUE && h)
            CloseHandle(h);
    }
    Handle(const Handle &) = delete;
};
inline std::string utf8(const std::wstring &s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0,
                                nullptr, nullptr);
    require(n > 0, "UTF-8 encoding failed");
    std::string o(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), o.data(), n, nullptr, nullptr);
    return o;
}
inline std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
    require(n > 0, "INI must be valid UTF-8");
    std::wstring o(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), o.data(), n);
    return o;
}
inline std::wstring modulePath(HMODULE m = nullptr) {
    std::wstring p(32768, 0);
    DWORD n = GetModuleFileNameW(m, p.data(), (DWORD)p.size());
    require(n && n < p.size(), "Module path unavailable");
    p.resize(n);
    return p;
}
inline std::wstring lower(std::wstring s) {
    for (auto &c : s)
        c = (wchar_t)towlower(c);
    return s;
}
inline std::wstring docs() {
    PWSTR p = nullptr;
    require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p)),
            "Documents folder unavailable");
    std::wstring s(p);
    CoTaskMemFree(p);
    return s;
}
inline std::wstring sid() {
    HANDLE t = nullptr;
    require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t) != 0, "Cannot read user token");
    Handle h(t);
    DWORD n = 0;
    GetTokenInformation(t, TokenUser, nullptr, 0, &n);
    std::vector<BYTE> b(n);
    require(GetTokenInformation(t, TokenUser, b.data(), n, &n) != 0, "Cannot read SID");
    PWSTR s = nullptr;
    require(ConvertSidToStringSidW(((TOKEN_USER *)b.data())->User.Sid, &s) != 0,
            "Cannot format SID");
    std::wstring o(s);
    LocalFree(s);
    return o;
}
inline std::string hex(const BYTE *p, size_t n) {
    std::ostringstream s;
    s << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; i++)
        s << std::setw(2) << (unsigned)p[i];
    return s.str();
}
class Sha {
    BCRYPT_ALG_HANDLE a = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;

  public:
    Sha() {
        require(BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0,
                "SHA provider failed");
        if (BCryptCreateHash(a, &h, nullptr, 0, nullptr, 0, 0) != 0) {
            BCryptCloseAlgorithmProvider(a, 0);
            throw std::runtime_error("SHA init failed");
        }
    }
    ~Sha() {
        if (h)
            BCryptDestroyHash(h);
        if (a)
            BCryptCloseAlgorithmProvider(a, 0);
    }
    void add(const void *p, size_t n) {
        require(n <= ULONG_MAX && BCryptHashData(h, (PUCHAR)p, (ULONG)n, 0) == 0,
                "SHA update failed");
    }
    std::string finish() {
        BYTE b[32];
        require(BCryptFinishHash(h, b, 32, 0) == 0, "SHA finish failed");
        return hex(b, 32);
    }
};
inline std::string hashText(const std::string &s) {
    Sha h;
    h.add(s.data(), s.size());
    return h.finish();
}
inline std::string hashFile(const fs::path &p) {
    Handle f(CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                         FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    require(f.h != INVALID_HANDLE_VALUE, "Cannot open file for SHA-256");
    Sha h;
    BYTE b[65536];
    DWORD n;
    for (;;) {
        require(ReadFile(f.h, b, sizeof b, &n, nullptr) != 0, "File hash read failed");
        if (!n)
            break;
        h.add(b, n);
    }
    return h.finish();
}
inline fs::path systemRoot() {
    wchar_t b[MAX_PATH];
    require(GetWindowsDirectoryW(b, MAX_PATH) > 2, "Windows drive unavailable");
    return fs::path(b).root_path();
}
inline fs::path target() {
    return systemRoot() / L"W3Saves" / wide(hashText(utf8(sid())).substr(0, 16));
}
inline std::wstring slash(const fs::path &p) {
    auto s = p.wstring();
    if (s.empty() || s.back() != L'\\')
        s += L'\\';
    return s;
}
inline bool ascii(const std::wstring &s) {
    return std::all_of(s.begin(), s.end(), [](wchar_t c) { return c > 0 && c < 128; });
}
inline std::string readFile(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    require(!!f, "Cannot read file");
    std::string s((std::istreambuf_iterator<char>(f)), {});
    require(s.size() < 1024 * 1024, "Configuration too large");
    return s;
}
inline std::string trim(std::string s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
using Ini = std::map<std::string, std::string>;
inline Ini parse(std::string s) {
    if (s.rfind("\xEF\xBB\xBF", 0) == 0)
        s.erase(0, 3);
    wide(s);
    require(s.find('\0') == std::string::npos, "NUL in INI");
    Ini o;
    std::string section, line;
    std::istringstream f(s);
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;
        if (line[0] == '[') {
            require(line.back() == ']', "Invalid section");
            section = line.substr(1, line.size() - 2);
            continue;
        }
        auto n = line.find('=');
        require(n != std::string::npos, "Invalid INI entry");
        auto k = section + "." + trim(line.substr(0, n));
        require(o.emplace(k, trim(line.substr(n + 1))).second, "Duplicate INI key");
    }
    return o;
}
inline std::string get(const Ini &i, const std::string &k, const std::string &fallback = {}) {
    auto p = i.find(k);
    return p == i.end() ? fallback : p->second;
}
inline DWORD number(const Ini &i, const char *k, DWORD d, DWORD min, DWORD max) {
    auto s = get(i, k);
    if (s.empty())
        return d;
    size_t pos = 0;
    unsigned long v = std::stoul(s, &pos);
    require(pos == s.size() && v >= min && v <= max, "Invalid numeric setting");
    return (DWORD)v;
}
inline void writeNew(const fs::path &p, const std::string &s) {
    Handle f(CreateFileW(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, nullptr));
    require(f.h != INVALID_HANDLE_VALUE, "Refusing to overwrite existing file");
    DWORD n = 0;
    require(WriteFile(f.h, s.data(), (DWORD)s.size(), &n, nullptr) && n == s.size() &&
                FlushFileBuffers(f.h),
            "File write failed");
}
inline std::string stamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char b[64];
    sprintf_s(b, "%04u%02u%02u-%02u%02u%02u-%03u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
              t.wSecond, t.wMilliseconds);
    return b;
}
inline std::string nonce() {
    BYTE b[12];
    require(BCryptGenRandom(nullptr, b, sizeof b, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0,
            "Random generation failed");
    return hex(b, sizeof b);
}
inline void noLinks(const fs::path &p) {
    for (auto v = p; !v.empty();) {
        DWORD a = GetFileAttributesW(v.c_str());
        if (a != INVALID_FILE_ATTRIBUTES)
            require(!(a & FILE_ATTRIBUTE_REPARSE_POINT),
                    "Reparse point in destination path; refusing");
        auto parent = v.parent_path();
        if (parent == v)
            break;
        v = parent;
    }
}
inline bool writable(const fs::path &p) {
    if (!fs::is_directory(p))
        return false;
    auto f = p / wide(".W3UnicodeSaveFix-probe-" + nonce() + ".tmp");
    Handle h(CreateFileW(f.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW,
                         FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
    if (h.h == INVALID_HANDLE_VALUE)
        return false;
    const char b[] = "W3UnicodeSaveFix write probe";
    DWORD n = 0;
    return WriteFile(h.h, b, sizeof b, &n, nullptr) && n == sizeof b && FlushFileBuffers(h.h);
}
inline bool x64pe(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    IMAGE_DOS_HEADER d{};
    f.read((char *)&d, sizeof d);
    if (!f || d.e_magic != IMAGE_DOS_SIGNATURE || d.e_lfanew <= 0 || d.e_lfanew > 1024 * 1024)
        return false;
    f.seekg(d.e_lfanew);
    DWORD sig = 0;
    IMAGE_FILE_HEADER h{};
    f.read((char *)&sig, 4);
    f.read((char *)&h, sizeof h);
    return f && sig == IMAGE_NT_SIGNATURE && h.Machine == IMAGE_FILE_MACHINE_AMD64;
}
inline bool gameRunning() {
    Handle h(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    require(h.h != INVALID_HANDLE_VALUE, "Cannot enumerate running processes");
    PROCESSENTRY32W e{};
    e.dwSize = sizeof e;
    if (Process32FirstW(h.h, &e))
        do {
            if (_wcsicmp(e.szExeFile, L"witcher3.exe") == 0)
                return true;
        } while (Process32NextW(h.h, &e));
    return false;
}
inline void copyVerified(const fs::path &s, const fs::path &d) {
    noLinks(d);
    require(!fs::exists(d), "Destination exists; no overwrite permitted");
    fs::create_directories(d.parent_path());
    auto before = hashFile(s);
    auto size = fs::file_size(s);
    require(CopyFileW(s.c_str(), d.c_str(), TRUE) != 0, "Copy failed");
    require(fs::file_size(d) == size && hashFile(d) == before && hashFile(s) == before,
            "Backup/migration size or SHA-256 mismatch");
}
inline std::vector<fs::path> files(const fs::path &root) {
    std::vector<fs::path> o;
    if (!fs::exists(root))
        return o;
    require(fs::is_directory(root), "Expected directory");
    for (auto &e : fs::recursive_directory_iterator(root)) {
        require(!(GetFileAttributesW(e.path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT),
                "Reparse point inside save tree; refusing");
        if (e.is_regular_file())
            o.push_back(e.path());
    }
    std::sort(o.begin(), o.end());
    return o;
}
inline bool saveExtension(const fs::path &p) {
    auto x = lower(p.extension().wstring());
    return x == L".sav" || x == L".png" || x == L".json";
}
} // namespace w3
