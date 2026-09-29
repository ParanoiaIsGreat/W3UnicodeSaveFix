#define W3_TEST
#include "plugin.cpp"
#include <iostream>
int main() {
    try {
        auto i = parse("\xEF\xBB\xBF[Fix]\nCodePage=1250\nDryRun=1\n");
        require(number(i, "Fix.CodePage", 0, 1, 65535) == 1250, "INI numeric");
        bool bad = false;
        try {
            parse("[Fix]\nX=1\nX=2\n");
        } catch (...) {
            bad = true;
        }
        require(bad, "Duplicate INI must fail");
        Pattern p;
        makePattern(p, L"C:\\Users\\Tester\u0161\\Documents\\The Witcher 3\\gamesaves\\",
                    L"C:\\W3Saves\\0123456789abcdef\\", 1250);
        require(escapedPath(L"C:\\Users\\Tester\u0161\\").find("\\u0161") != std::string::npos,
                "Path logs must not create ANSI/UTF-8 search copies");
        BYTE *memory =
            (BYTE *)VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        require(memory != nullptr, "Test alloc");
        BYTE *one = memory + 128;
        memcpy(one, p.source, p.length);
        Scan a;
        require(scanRange(memory, 8192, p, a) && a.count == 1, "Exact unique source");
        one[p.length - 1] = 'x';
        Scan suffix;
        require(scanRange(memory, 8192, p, suffix) && suffix.count == 0,
                "Non-NUL source must not match");
        one[p.length - 1] = 0;
        memcpy(memory + 512, p.source, p.length);
        Scan twice;
        require(scanRange(memory, 8192, p, twice) && twice.count == 2, "Duplicate detection");
        memset(memory + 512, 0, p.length);
        auto full = scan(p, 4096, 30000);
        require(
            full.complete && full.count == 1 && full.address == one,
            "Full VirtualQuery scan must exclude plugin-owned needle and find exactly one fixture");
        memcpy(memory + 512, p.source, p.length);
        auto ambiguous = scan(p, 4096, 30000);
        require(ambiguous.count > 1, "Full scan must detect ambiguity");
        memset(memory + 512, 0, p.length);
        BYTE *boundary = (BYTE *)VirtualAlloc(nullptr, 2 * 1024 * 1024, MEM_COMMIT | MEM_RESERVE,
                                              PAGE_READWRITE);
        require(boundary != nullptr, "Boundary alloc");
        memset(one, 0, p.length);
        BYTE *crossing = boundary + 1024 * 1024 - p.length / 2;
        memcpy(crossing, p.source, p.length);
        auto crossed = scan(p, 4096, 30000);
        require(crossed.complete && crossed.count == 1 && crossed.address == crossing,
                "Chunk-boundary match must appear once");
        memset(crossing, 0, p.length);
        BYTE *atBoundary = boundary + 1024 * 1024;
        memcpy(atBoundary, p.source, p.length);
        auto exactBoundary = scan(p, 4096, 30000);
        require(exactBoundary.complete && exactBoundary.count == 1 &&
                    exactBoundary.address == atBoundary,
                "Exact chunk-boundary match must not be double-counted");
        VirtualFree(boundary, 0, MEM_RELEASE);
        memcpy(one, p.source, p.length);
        DWORD old;
        VirtualProtect(memory, 8192, PAGE_READONLY, &old);
        require(!regionValid(one, p.length, p), "Read-only rejection");
        VirtualProtect(memory, 8192, PAGE_READWRITE, &old);
        auto outcome = patch(one, p);
        require(outcome == Patch::Applied && memcmp(one, p.replacement, p.length) == 0,
                "Short replacement and zero padding");
        auto n = strlen((char *)p.replacement) + 1;
        for (size_t j = n; j < p.length; j++)
            require(one[j] == 0, "Padding not zero");
        require(patch(one, p) == Patch::Unchanged, "Changed original rejection");
        require(!regionValid(p.source, p.length, p), "Plugin-owned pattern exclusion");
        MEMORY_BASIC_INFORMATION region{};
        VirtualQuery(one, &region, sizeof region);
        region.Type = MEM_IMAGE;
        require(!writableRegion(region), "Image rejection");
        region.Type = MEM_MAPPED;
        require(!writableRegion(region), "Mapped rejection");
        region.Type = MEM_PRIVATE;
        region.Protect = PAGE_READWRITE | PAGE_GUARD;
        require(!writableRegion(region), "Guard rejection");
        BYTE *guarded =
            (BYTE *)VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        Scan fault;
        require(!scanRange(guarded, 4096, p, fault), "Read fault trapped");
        VirtualFree(guarded, 0, MEM_RELEASE);
        VirtualFree(memory, 0, MEM_RELEASE);
        Pattern utf;
        bad = false;
        try {
            makePattern(utf, L"C:\\Users\\\u4e2d\\Documents\\The Witcher 3\\gamesaves\\",
                        L"C:\\W3Saves\\0123456789abcdef\\", 1250);
        } catch (...) {
            bad = true;
        }
        require(bad, "Unrepresentable CP1250 source rejection");
        Pattern longTarget;
        bad = false;
        try {
            makePattern(longTarget, L"C:\\x\\", L"C:\\longtarget\\", 1250);
        } catch (...) {
            bad = true;
        }
        require(bad, "Longer target rejection");
        std::cout << "PASS: INI/BOM/duplicate keys, exact NUL match, full VirtualQuery "
                     "uniqueness/ambiguity/chunk-boundary scans, read-only/owned-buffer rejection, "
                     "shorter full path replacement, zero padding, stale-source refusal, SEH read "
                     "fault, lossy encoding refusal, length refusal\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
