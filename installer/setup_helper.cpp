#include "../src/common.hpp"
#include <aclapi.h>
#include <regex>
using namespace w3;
constexpr const char *LoaderHash =
    "031a3e5576d91dce1e438d36b9a3d462c7334ab4791990a8ff1e3ddc0e132daf";
std::wstring reg(HKEY root, const std::wstring &key, const wchar_t *value,
                 REGSAM view = RRF_SUBKEY_WOW6464KEY) {
    DWORD n = 0;
    DWORD flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | view;
    if (RegGetValueW(root, key.c_str(), value, flags, nullptr, nullptr, &n) != ERROR_SUCCESS)
        return {};
    std::wstring s(n / 2, 0);
    if (RegGetValueW(root, key.c_str(), value, flags, nullptr, s.data(), &n) != ERROR_SUCCESS)
        return {};
    while (!s.empty() && !s.back())
        s.pop_back();
    return s;
}
fs::path binPath(fs::path p) {
    if (lower(p.filename().wstring()) == L"witcher3.exe")
        p = p.parent_path();
    for (auto c : {p, p / L"bin" / L"x64_dx12"})
        if (lower(c.filename().wstring()) == L"x64_dx12" && fs::exists(c / L"witcher3.exe") &&
            x64pe(c / L"witcher3.exe"))
            return fs::weakly_canonical(c);
    return {};
}
std::vector<fs::path> detect() {
    std::set<fs::path> roots;
    for (auto view : {RRF_SUBKEY_WOW6464KEY, RRF_SUBKEY_WOW6432KEY}) {
        for (auto root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
            auto s = reg(root, L"SOFTWARE\\Valve\\Steam", L"InstallPath", view);
            if (s.empty())
                s = reg(root, L"SOFTWARE\\Valve\\Steam", L"SteamPath", view);
            if (!s.empty())
                roots.insert(s);
        }
    }
    std::vector<fs::path> libraries(roots.begin(), roots.end());
    for (auto &r : roots) {
        auto f = r / L"steamapps" / L"libraryfolders.vdf";
        if (fs::exists(f)) {
            auto text = readFile(f);
            std::regex rx("\\\"path\\\"\\s+\\\"([^\\\"]+)\\\"");
            for (std::sregex_iterator i(text.begin(), text.end(), rx), e; i != e; ++i) {
                auto path = (*i)[1].str();
                for (size_t j = 0; (j = path.find("\\\\", j)) != std::string::npos;)
                    path.replace(j, 2, "\\");
                libraries.push_back(wide(path));
            }
        }
    }
    std::set<fs::path> found;
    for (auto &p : libraries) {
        auto b = binPath(p / L"steamapps" / L"common" / L"The Witcher 3");
        if (!b.empty())
            found.insert(b);
        auto manifest = p / L"steamapps" / L"appmanifest_292030.acf";
        if (fs::exists(manifest)) {
            auto s = readFile(manifest);
            std::regex rx("\\\"installdir\\\"\\s+\\\"([^\\\"]+)\\\"");
            std::smatch m;
            if (std::regex_search(s, m, rx)) {
                b = binPath(p / L"steamapps" / L"common" / wide(m[1].str()));
                if (!b.empty())
                    found.insert(b);
            }
        }
    }
    for (auto view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        HKEY k = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.com\\Games", 0, KEY_READ | view,
                          &k) == ERROR_SUCCESS) {
            for (DWORD i = 0;; i++) {
                wchar_t name[256];
                DWORD n = 256;
                if (RegEnumKeyExW(k, i, name, &n, nullptr, nullptr, nullptr, nullptr) !=
                    ERROR_SUCCESS)
                    break;
                auto s = reg(
                    HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.com\\Games\\" + std::wstring(name), L"path",
                    view == KEY_WOW64_32KEY ? RRF_SUBKEY_WOW6432KEY : RRF_SUBKEY_WOW6464KEY);
                if (!s.empty()) {
                    auto b = binPath(s);
                    if (!b.empty())
                        found.insert(b);
                }
            }
            RegCloseKey(k);
        }
    }
    return {found.begin(), found.end()};
}
void intendedUser() {
    DWORD pid = 0;
    HWND shell = GetShellWindow();
    require(shell != nullptr, "No interactive desktop user; cannot select safe save identity");
    GetWindowThreadProcessId(shell, &pid);
    Handle p(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    require(p.h != nullptr, "Cannot identify desktop user");
    HANDLE raw;
    require(OpenProcessToken(p.h, TOKEN_QUERY, &raw) != 0, "Cannot query desktop user token");
    Handle t(raw);
    DWORD n = 0;
    GetTokenInformation(raw, TokenUser, nullptr, 0, &n);
    std::vector<BYTE> b(n);
    require(GetTokenInformation(raw, TokenUser, b.data(), n, &n) != 0, "Cannot query desktop SID");
    PWSTR s = nullptr;
    require(ConvertSidToStringSidW(((TOKEN_USER *)b.data())->User.Sid, &s) != 0,
            "Cannot format desktop SID");
    std::wstring desktop(s);
    LocalFree(s);
    require(desktop == sid(), "Installer elevated as another account. Cancel and run as the "
                              "intended desktop user; no files changed.");
}
void secureCreate(const fs::path &p) {
    noLinks(p);
    if (fs::exists(p))
        return;
    fs::create_directories(p.parent_path());
    auto descriptor = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;" + sid() + L")";
    PSECURITY_DESCRIPTOR sd = nullptr;
    require(ConvertStringSecurityDescriptorToSecurityDescriptorW(
                descriptor.c_str(), SDDL_REVISION_1, &sd, nullptr) != 0,
            "Cannot create directory ACL");
    SECURITY_ATTRIBUTES a{sizeof a, sd, FALSE};
    BOOL ok = CreateDirectoryW(p.c_str(), &a);
    DWORD error = GetLastError();
    LocalFree(sd);
    require(ok || error == ERROR_ALREADY_EXISTS, "Cannot create protected destination");
    noLinks(p);
}
std::string loaderStatus(const fs::path &b) {
    if (b.empty())
        return "Select a DX12 game installation";
    for (auto n : {L"dxgi.dll", L"version.dll", L"winmm.dll", L"winhttp.dll", L"d3d11.dll",
                   L"d3d12.dll", L"dsound.dll", L"xinput1_3.dll"})
        if (fs::exists(b / n))
            return "BLOCKED: another proxy DLL exists; manual compatibility review needed";
    auto d = b / L"dinput8.dll";
    if (!fs::exists(d))
        return "Bundled Ultimate ASI Loader 9.7.4 x64 will be installed";
    if (x64pe(d) && hashFile(d) == LoaderHash)
        return "Compatible pinned loader exists; reused, not owned by mod";
    return "BLOCKED: existing dinput8.dll is unknown; will not overwrite";
}
void validateGame(const fs::path &b) {
    require(!b.empty() && !binPath(b).empty(), "Select the x64 DX12 witcher3.exe directory");
    noLinks(b);
    require(!gameRunning(), "Close The Witcher 3 yourself before installation/uninstallation. No "
                            "process is terminated by this tool.");
    require(loaderStatus(b).rfind("BLOCKED", 0) != 0,
            "Unknown existing ASI/proxy loader; refusing overwrite");
}
using Manifest = std::map<std::wstring, std::pair<uintmax_t, std::string>>;
Manifest inventory(const fs::path &root) {
    Manifest m;
    for (auto &p : files(root))
        m.emplace(fs::relative(p, root).wstring(), std::make_pair(fs::file_size(p), hashFile(p)));
    return m;
}
void snapshot(const fs::path &src, const fs::path &dst) {
    require(!fs::exists(dst), "Backup destination already exists");
    auto before = inventory(src);
    secureCreate(dst);
    for (auto &[name, meta] : before)
        copyVerified(src / name, dst / name);
    require(inventory(src) == before && inventory(dst) == before,
            "Backup count/size/hash verification failed or source changed");
    std::ostringstream report;
    report << "Files=" << before.size() << "\n";
    for (auto &[name, meta] : before)
        report << meta.first << '\t' << meta.second << '\t' << utf8(name) << '\n';
    writeNew(dst.parent_path() / L"manifest-sha256.txt", report.str());
}
std::string stateValue(const Ini &i, const char *k) {
    auto v = get(i, std::string("State.") + k);
    require(!v.empty(), "Invalid installation state");
    return v;
}
void install(const fs::path &game, const fs::path &backup, const fs::path &payload,
             const fs::path &state) {
    intendedUser();
    validateGame(game);
    require(!fs::exists(state), "Already installed; uninstall this copy before reinstalling");
    auto src = fs::path(docs()) / L"The Witcher 3" / L"gamesaves";
    auto dst = target();
    auto expectedBase = systemRoot() / L"W3UnicodeSaveFixBackup";
    require(backup.parent_path() == expectedBase && !backup.filename().empty(),
            "Invalid backup destination");
    noLinks(backup);
    require(!fs::exists(backup), "Backup path collision");
    noLinks(src);
    noLinks(dst);
    require(!fs::exists(game / L"W3UnicodeSaveFix.asi") &&
                !fs::exists(game / L"W3UnicodeSaveFix.ini"),
            "Existing mod files; refusing overwrite");
    require(x64pe(payload / L"W3UnicodeSaveFix.asi"), "Plugin is not x64 PE");
    require(x64pe(payload / L"dinput8.dll") && hashFile(payload / L"dinput8.dll") == LoaderHash,
            "Bundled loader architecture/hash mismatch");
    // Fail on different destination saves BEFORE any migration. Existing identical files are
    // retained.
    for (auto &p : files(src))
        if (saveExtension(p)) {
            auto q = dst / fs::relative(p, src);
            if (fs::exists(q))
                require(hashFile(q) == hashFile(p), "Conflicting save already exists in ASCII "
                                                    "destination; resolve manually, no overwrite");
        }
    // The first persistent change in game/save locations is the verified backup.
    snapshot(src, backup / L"original-saves");
    auto backedUpSource = inventory(backup / L"original-saves");
    require(inventory(src) == backedUpSource,
            "Original saves changed after backup; migration stopped");
    if (fs::exists(dst)) {
        auto before = inventory(dst);
        secureCreate(backup / L"ascii-before");
        for (auto &[name, meta] : before)
            copyVerified(dst / name, backup / L"ascii-before" / name);
        require(inventory(dst) == before && inventory(backup / L"ascii-before") == before,
                "Existing ASCII saves backup verification failed");
    }
    secureCreate(dst);
    require(writable(dst), "ASCII directory is not writable");
    for (auto &p : files(src))
        if (saveExtension(p)) {
            auto q = dst / fs::relative(p, src);
            if (!fs::exists(q))
                copyVerified(p, q);
            else
                require(hashFile(q) == hashFile(p), "Concurrent destination conflict");
        }
    require(inventory(src) == backedUpSource,
            "Original saves changed during migration; game files not installed");
    require(!gameRunning(), "Witcher 3 started during backup/migration; game files not installed");
    bool ownLoader = !fs::exists(game / L"dinput8.dll");
    std::ostringstream manifest;
    manifest << "[State]\nVersion=0.1.0\nSID=" << utf8(sid()) << "\nGame=" << utf8(game.wstring())
             << "\nSource=" << utf8(src.wstring()) << "\nTarget=" << utf8(dst.wstring())
             << "\nBackup=" << utf8(backup.wstring()) << "\nLoaderOwned=" << (ownLoader ? 1 : 0)
             << "\nASIHash=" << hashFile(payload / L"W3UnicodeSaveFix.asi")
             << "\nINIHash=" << hashFile(payload / L"W3UnicodeSaveFix.ini")
             << "\nLoaderHash=" << LoaderHash << "\n";
    // Record recovery instructions before creating any game binaries.
    writeNew(state, manifest.str());
    writeNew(backup / L"installation-state.ini", manifest.str());
    std::vector<std::pair<fs::path, std::string>> added;
    try {
        for (auto name : {L"W3UnicodeSaveFix.asi", L"W3UnicodeSaveFix.ini"}) {
            auto h = hashFile(payload / name);
            copyVerified(payload / name, game / name);
            added.push_back({game / name, h});
        }
        if (ownLoader) {
            copyVerified(payload / L"dinput8.dll", game / L"dinput8.dll");
            added.push_back({game / L"dinput8.dll", LoaderHash});
        }
    } catch (...) {
        for (auto &[p, h] : added)
            if (fs::exists(p) && hashFile(p) == h)
                DeleteFileW(p.c_str());
        throw;
    }
}
void removeInstalled(const fs::path &state, bool restore) {
    intendedUser();
    require(!gameRunning(), "Close Witcher 3 before uninstalling; this tool never terminates it");
    auto i = parse(readFile(state));
    require(wide(stateValue(i, "SID")) == sid(),
            "Uninstall must run as the installing Windows user");
    fs::path game = wide(stateValue(i, "Game")), src = wide(stateValue(i, "Source")),
             dst = wide(stateValue(i, "Target")), backup = wide(stateValue(i, "Backup"));
    require(!binPath(game).empty() && dst == target(),
            "Installation state does not match valid game/user target");
    noLinks(game);
    noLinks(src);
    noLinks(dst);
    noLinks(backup);
    if (restore && fs::exists(dst)) {
        // Full backup of the destination before any user-confirmed conflict overwrite.
        auto restoreBackup = backup / (L"before-restore-" + wide(stamp() + "-" + nonce()));
        snapshot(src, restoreBackup / L"original-saves");
        fs::create_directories(src);
        for (auto &p : files(dst))
            if (saveExtension(p)) {
                auto q = src / fs::relative(p, dst);
                if (!fs::exists(q)) {
                    copyVerified(p, q);
                    continue;
                }
                if (hashFile(p) == hashFile(q) || fs::last_write_time(p) <= fs::last_write_time(q))
                    continue;
                auto msg = L"A newer, different save exists in the ASCII folder.\n\nSource: " +
                           p.wstring() + L"\nDestination: " + q.wstring() +
                           L"\n\nThe original was backed up and verified. Replace this individual "
                           L"file?\nNo leaves both copies unchanged.";
                if (MessageBoxW(nullptr, msg.c_str(), L"W3UnicodeSaveFix — save conflict",
                                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
                    auto oldHash = hashFile(q);
                    auto relative = fs::relative(q, src);
                    require(hashFile(restoreBackup / L"original-saves" / relative) == oldHash,
                            "Original changed after restore backup; refusing overwrite");
                    auto staging = q;
                    staging += L".W3UnicodeSaveFix-" + wide(nonce()) + L".tmp";
                    copyVerified(p, staging);
                    require(hashFile(q) == oldHash,
                            "Original changed while staging restore; refusing overwrite");
                    require(
                        ReplaceFileW(q.c_str(), staging.c_str(), nullptr, 0, nullptr, nullptr) != 0,
                        "Atomic replacement failed; original and staged files retained");
                    require(hashFile(p) == hashFile(q),
                            "Restored file hash mismatch; verified backup retained");
                }
            }
    }
    for (auto pair : {std::make_pair(L"W3UnicodeSaveFix.asi", "ASIHash"),
                      std::make_pair(L"W3UnicodeSaveFix.ini", "INIHash")}) {
        auto p = game / pair.first;
        if (fs::exists(p)) {
            if (hashFile(p) == stateValue(i, pair.second))
                require(DeleteFileW(p.c_str()) != 0, "Cannot remove mod file");
            else
                MessageBoxW(nullptr, (L"Modified file preserved: " + p.wstring()).c_str(),
                            L"W3UnicodeSaveFix", MB_OK | MB_ICONINFORMATION);
        }
    }
    if (get(i, "State.LoaderOwned") == "1") {
        auto p = game / L"dinput8.dll";
        if (fs::exists(p) && hashFile(p) == LoaderHash) {
            bool others = false;
            for (auto &e : fs::directory_iterator(game))
                if (lower(e.path().extension().wstring()) == L".asi")
                    others = true;
            for (auto folder : {L"scripts", L"plugins", L"update"})
                if (fs::exists(game / folder))
                    others = true;
            if (!others)
                require(DeleteFileW(p.c_str()) != 0, "Cannot remove owned loader");
        }
    }
    copyVerified(state, backup / (L"uninstalled-state-" + wide(stamp() + "-" + nonce()) + L".ini"));
    require(DeleteFileW(state.c_str()) != 0,
            "Cannot remove completed installation state; recovery copy retained in backup");
    // Save directories, backups, logs, other mods and modified files are NEVER removed.
}
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc >= 3 && wcscmp(argv[1], L"probe") == 0) {
            intendedUser();
            auto found = detect();
            auto b = argc >= 4 ? binPath(argv[3]) : (found.empty() ? fs::path{} : found[0]);
            auto backup = systemRoot() / L"W3UnicodeSaveFixBackup" / wide(stamp() + "-" + nonce());
            std::ostringstream s;
            s << "[Probe]\nGame=" << utf8(b.wstring())
              << "\nSource=" << utf8((fs::path(docs()) / L"The Witcher 3" / L"gamesaves").wstring())
              << "\nTarget=" << utf8(target().wstring()) << "\nBackup=" << utf8(backup.wstring())
              << "\nLoader=" << loaderStatus(b) << "\nSID=" << utf8(sid())
              << "\nDetectedInstallations=" << found.size() << "\n";
            writeNew(argv[2], "\xEF\xBB\xBF" + s.str());
            return 0;
        }
        if (argc == 6 && wcscmp(argv[1], L"install") == 0) {
            install(argv[2], argv[3], argv[4], argv[5]);
            return 0;
        }
        if (argc == 4 && wcscmp(argv[1], L"uninstall") == 0) {
            removeInstalled(argv[2], wcscmp(argv[3], L"restore") == 0);
            return 0;
        }
        if (argc == 3 && wcscmp(argv[1], L"selftest") == 0) {
            auto root = fs::absolute(argv[2]);
            require(!fs::exists(root), "Test path exists");
            fs::create_directories(root / L"source");
            writeNew(root / L"source" / L"test.sav", "synthetic save");
            snapshot(root / L"source", root / L"backup" / L"original-saves");
            require(hashFile(root / L"source" / L"test.sav") ==
                        hashFile(root / L"backup" / L"original-saves" / L"test.sav"),
                    "Backup test failed");
            bool refused = false;
            try {
                copyVerified(root / L"source" / L"test.sav",
                             root / L"backup" / L"original-saves" / L"test.sav");
            } catch (...) {
                refused = true;
            }
            require(refused, "Overwrite refusal failed");
            auto ini = parse("[Fix]\nEnabled=1\nCodePage=auto\n");
            require(get(ini, "Fix.Enabled") == "1", "INI parser failed");
            wprintf(L"PASS: verified backup, SHA-256 equality, collision refusal, INI parser\n");
            return 0;
        }
        throw std::runtime_error("Invalid helper command");
    } catch (const std::exception &e) {
        auto msg = wide(e.what());
        if (argc > 1 && wcscmp(argv[1], L"selftest") == 0) {
            fwprintf(stderr, L"FAIL: %s\n", msg.c_str());
        } else
            MessageBoxW(nullptr, msg.c_str(), L"W3UnicodeSaveFix — operation stopped",
                        MB_OK | MB_ICONERROR);
        return 1;
    }
}
