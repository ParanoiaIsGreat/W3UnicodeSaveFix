#include "common.hpp"
#include <intrin.h>
using namespace w3;
namespace {
HMODULE selfModule = nullptr;
HANDLE logHandle = INVALID_HANDLE_VALUE;
bool log(const std::string &s) {
    if (logHandle == INVALID_HANDLE_VALUE)
        return false;
    std::string line = stamp() + " " + s + "\r\n";
    DWORD n = 0;
    return WriteFile(logHandle, line.data(), (DWORD)line.size(), &n, nullptr) && n == line.size() &&
           FlushFileBuffers(logHandle);
}
// Escape non-ASCII before logging. With ACP=UTF-8 an ordinary UTF-8 log string
// would itself create another private-memory copy of the search needle.
std::string escapedPath(const std::wstring &s) {
    std::ostringstream o;
    for (auto c : s) {
        if (c >= 32 && c < 127)
            o << (char)c;
        else
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (unsigned)c << std::dec;
    }
    return o.str();
}
struct Pattern {
    BYTE *base = nullptr;
    size_t length = 0;
    BYTE *source = nullptr;
    BYTE *replacement = nullptr;
    ~Pattern() {
        if (base)
            VirtualFree(base, 0, MEM_RELEASE);
    }
};
bool writableRegion(const MEMORY_BASIC_INFORMATION &m) {
    DWORD p = m.Protect & 255;
    return m.State == MEM_COMMIT && m.Type == MEM_PRIVATE &&
           !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
           (p == PAGE_READWRITE || p == PAGE_WRITECOPY);
}
bool regionValid(BYTE *p, size_t n, const Pattern &pat) {
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(p, &m, sizeof m) || !writableRegion(m) || m.AllocationBase == pat.base)
        return false;
    uintptr_t a = (uintptr_t)p, b = (uintptr_t)m.BaseAddress;
    return a >= b && n <= m.RegionSize && a - b <= m.RegionSize - n;
}
bool equalSafe(const BYTE *a, const BYTE *b, size_t n) {
    __try {
        return memcmp(a, b, n) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool copySafe(BYTE *d, const BYTE *s, size_t n) {
    __try {
        memcpy(d, s, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// All buffers containing the ANSI needle and rollback image live in one dedicated
// allocation excluded from every scan. No source path is stored as a narrow literal.
void makePattern(Pattern &p, const std::wstring &source, const std::wstring &dest, UINT cp) {
    BOOL used = FALSE;
    DWORD flags = cp == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    auto count = [&](const std::wstring &s) {
        used = FALSE;
        int n = WideCharToMultiByte(cp, flags, s.c_str(), -1, nullptr, 0, nullptr,
                                    cp == CP_UTF8 ? nullptr : &used);
        require(n > 0 && !used, "Source is not losslessly representable in configured code page");
        return n;
    };
    int n = count(source), k = count(dest);
    require(k < n, "Target must be strictly shorter than source buffer");
    require(n <= 4096, "Source path too long");
    p.base = (BYTE *)VirtualAlloc(nullptr, 16384, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    require(p.base != nullptr, "Pattern allocation failed");
    p.length = n;
    p.source = p.base;
    p.replacement = p.base + 4096;
    used = FALSE;
    require(WideCharToMultiByte(cp, flags, source.c_str(), -1, (char *)p.source, n, nullptr,
                                cp == CP_UTF8 ? nullptr : &used) == n &&
                !used,
            "Source conversion failed");
    used = FALSE;
    require(WideCharToMultiByte(cp, flags, dest.c_str(), -1, (char *)p.replacement, k, nullptr,
                                cp == CP_UTF8 ? nullptr : &used) == k &&
                !used,
            "Target conversion failed");
    // VirtualAlloc zero-initializes the remainder; replacement includes the original capacity.
}
struct Scan {
    size_t count = 0;
    BYTE *address = nullptr;
    bool complete = true;
    uint64_t bytes = 0;
};
bool scanRange(BYTE *base, size_t n, const Pattern &p, Scan &r) {
    __try {
        if (n < p.length)
            return true;
        for (size_t i = 0; i <= n - p.length;) {
            auto q = (BYTE *)memchr(base + i, p.source[0], n - p.length - i + 1);
            if (!q)
                break;
            i = (size_t)(q - base);
            if (memcmp(q, p.source, p.length) == 0) {
                r.address = q;
                if (++r.count > 1)
                    return true;
            }
            ++i;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
Scan scan(const Pattern &p, DWORD budgetMiB, DWORD budgetMs) {
    Scan r;
    uintptr_t cursor = 0;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    auto max = (uintptr_t)si.lpMaximumApplicationAddress;
    auto started = GetTickCount64();
    while (cursor < max) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery((void *)cursor, &m, sizeof m)) {
            r.complete = false;
            break;
        }
        auto b = (uintptr_t)m.BaseAddress;
        auto next = b + m.RegionSize;
        if (next <= cursor) {
            r.complete = false;
            break;
        }
        if (writableRegion(m) && m.AllocationBase != p.base) {
            r.bytes += m.RegionSize;
            if (r.bytes > (uint64_t)budgetMiB * 1024 * 1024) {
                r.complete = false;
                break;
            }
            constexpr size_t chunk = 1024 * 1024;
            for (size_t off = 0; off < m.RegionSize;) {
                size_t core = std::min(chunk, m.RegionSize - off);
                size_t read = std::min(core + p.length - 1, m.RegionSize - off);
                Scan part;
                if (!scanRange((BYTE *)b + off, read, p, part)) {
                    r.complete = false;
                    return r;
                }
                r.count += part.count;
                if (part.address)
                    r.address = part.address;
                if (r.count > 1)
                    return r;
                off += core;
                if (GetTickCount64() - started > budgetMs) {
                    r.complete = false;
                    return r;
                }
                Sleep(1);
            }
        }
        cursor = next;
    }
    return r;
}
// A full-path change is not atomic. Guard each access with SEH, recheck the full
// old bytes, write suffix/zero padding first and the leading byte last, then read back.
// Conditional rollback avoids overwriting a buffer already changed by the game.
enum class Patch { Applied, Unchanged, Restored, Indeterminate };
Patch patch(BYTE *address, Pattern &p) {
    BYTE *backup = p.base + 8192;
    BYTE *observed = p.base + 12288;
    if (!regionValid(address, p.length, p) || !equalSafe(address, p.source, p.length) ||
        !copySafe(backup, address, p.length))
        return Patch::Unchanged;
    if (!regionValid(address, p.length, p) || !equalSafe(address, p.source, p.length))
        return Patch::Unchanged;
    // Nothing here changes page permissions, suspends a thread or attaches a debugger.
    bool written = false;
    __try {
        memcpy(address + 1, p.replacement + 1, p.length - 1);
        MemoryBarrier();
        address[0] = p.replacement[0];
        written = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    if (written && equalSafe(address, p.replacement, p.length))
        return Patch::Applied;
    // If a write faulted halfway, the state is not known to be ours: do not blindly roll back.
    if (!written || !regionValid(address, p.length, p) || !copySafe(observed, address, p.length))
        return Patch::Indeterminate;
    // Readback failure may be a concurrent game write; restoring would overwrite it.
    if (!equalSafe(observed, p.replacement, p.length))
        return Patch::Indeterminate;
    if (copySafe(address, backup, p.length) && equalSafe(address, backup, p.length))
        return Patch::Restored;
    return Patch::Indeterminate;
}
BOOL CALLBACK findWindow(HWND h, LPARAM p) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(h) && !GetWindow(h, GW_OWNER)) {
        *(bool *)p = true;
        return FALSE;
    }
    return TRUE;
}
DWORD WINAPI worker(void *) {
    Handle closeLog(INVALID_HANDLE_VALUE);
    try {
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                    GET_MODULE_HANDLE_EX_FLAG_PIN,
                                (LPCWSTR)&worker, &pinned))
            return 0;
        auto module = fs::path(modulePath(selfModule));
        auto folder = module.parent_path();
        logHandle = CreateFileW((folder / L"W3UnicodeSaveFix.log").c_str(), FILE_APPEND_DATA,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (logHandle == INVALID_HANDLE_VALUE) {
            auto fallback = target();
            noLinks(fallback);
            if (fs::is_directory(fallback))
                logHandle = CreateFileW((fallback / L"W3UnicodeSaveFix.log").c_str(),
                                        FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
        if (logHandle == INVALID_HANDLE_VALUE) {
            OutputDebugStringW(L"W3UnicodeSaveFix: cannot open log; no patch.\n");
            return 0;
        }
        closeLog.h = logHandle;
        auto exe = fs::path(modulePath());
        if (lower(exe.filename().wstring()) != L"witcher3.exe" ||
            lower(exe.parent_path().filename().wstring()) != L"x64_dx12") {
            log("Patch applied: NO; unsupported host (DX12 only)");
            return 0;
        }
        log("W3UnicodeSaveFix 0.1.0-experimental starting; PID=" +
            std::to_string(GetCurrentProcessId()));
        auto cfg = parse(readFile(folder / L"W3UnicodeSaveFix.ini"));
        if (get(cfg, "Fix.Enabled", "1") != "1") {
            log("Patch applied: NO; disabled");
            return 0;
        }
        auto documents = docs();
        auto source = slash(fs::path(documents) / L"The Witcher 3" / L"gamesaves");
        auto dest = slash(target());
        UINT cp = get(cfg, "Fix.CodePage", "auto") == "auto"
                      ? GetACP()
                      : number(cfg, "Fix.CodePage", GetACP(), 1, 65535);
        log("Detected Documents: " + escapedPath(documents));
        log("Original save path: " + escapedPath(source));
        log("Target save path: " + escapedPath(dest));
        log("Encoding/codepage: " + std::to_string(cp));
        if (ascii(source)) {
            log("Patch applied: NO; source is already ASCII");
            return 0;
        }
        require(ascii(dest), "Target must be ASCII");
        noLinks(target());
        require(writable(target()), "Target missing or not writable");
        Pattern p;
        makePattern(p, source, dest, cp);
        DWORD delay = number(cfg, "Fix.InitialDelayMs", 30000, 5000, 300000),
              timeout = number(cfg, "Fix.TimeoutMs", 180000, 10000, 600000),
              stable = number(cfg, "Fix.StableDelayMs", 2000, 500, 30000),
              budget = number(cfg, "Fix.ScanBudgetMiB", 32768, 64, 262144),
              scanMs = number(cfg, "Fix.ScanTimeoutMs", 60000, 1000, 300000);
        Sleep(delay);
        auto start = GetTickCount64();
        bool ready = false;
        while (GetTickCount64() - start < timeout) {
            ready = false;
            EnumWindows(findWindow, (LPARAM)&ready);
            if (ready)
                break;
            Sleep(500);
        }
        require(ready, "Initialization wait timed out (no visible game window)");
        Scan a;
        do {
            a = scan(p, budget, scanMs);
            log("Memory candidates: " + std::to_string(a.count) +
                "; complete=" + (a.complete ? "YES" : "NO"));
            require(a.complete, "Incomplete scan; refusing patch");
            if (a.count > 1) {
                log("Patch applied: NO; ambiguous exact matches");
                return 0;
            }
            if (a.count == 1)
                break;
            Sleep(5000);
        } while (GetTickCount64() - start < timeout);
        if (a.count != 1) {
            log("Patch applied: NO; no unique match before timeout");
            return 0;
        }
        Sleep(stable);
        auto b = scan(p, budget, scanMs);
        require(b.complete && b.count == 1 && b.address == a.address,
                "Unique candidate did not remain stable across two scans");
        require(writable(target()), "Target no longer writable");
        if (get(cfg, "Fix.DryRun", "0") == "1") {
            log("Patch applied: NO; DryRun=1, unique stable candidate validated");
            return 0;
        }
        std::ostringstream where;
        where << "0x" << std::hex << (uintptr_t)b.address;
        require(log("Prepared patch at " + where.str() + "; old path: " + escapedPath(source) +
                    "; new path: " + escapedPath(dest) + "; capacity=" + std::to_string(p.length)),
                "Cannot persist pre-write log");
        auto result = patch(b.address, p);
        if (result == Patch::Applied)
            log("Patch applied: YES; Validation: SUCCESS");
        else if (result == Patch::Unchanged)
            log("Patch applied: NO; Validation: FAILED; original content changed before write");
        else if (result == Patch::Restored)
            log("Patch applied: NO; Validation: FAILED; original bytes restored");
        else
            log("Patch applied: UNKNOWN; Validation: FAILED; concurrent change/write fault; no "
                "blind rollback or retry");
    } catch (const std::exception &e) {
        log(std::string("Patch applied: NO; ") + e.what());
    } catch (...) {
        log("Patch applied: NO; unexpected C++ exception");
    }
    logHandle = INVALID_HANDLE_VALUE;
    return 0;
}
} // namespace
#ifndef W3_TEST
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        selfModule = instance;
        HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (t)
            CloseHandle(t);
    }
    return TRUE;
}
#endif
