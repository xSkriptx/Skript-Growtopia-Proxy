#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>
#include <spdlog/spdlog.h>
#include "strenc.hpp"






typedef LONG (NTAPI* pfnNtQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);

static bool NtCheckDebugPort() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return false;

    std::string fname = DEC("NtQueryInformationProcess");
    auto NtQIP = reinterpret_cast<pfnNtQIP>(GetProcAddress(ntdll, fname.c_str()));
    SecureZeroMemory(&fname[0], fname.size());
    if (!NtQIP) return false;

    
    BYTE* fn = (BYTE*)NtQIP;
    if (fn[0] == 0xE9 || fn[0] == 0xEB) return true;

    DWORD_PTR debugPort = 0;
    LONG status = NtQIP(GetCurrentProcess(), 7, &debugPort, sizeof(debugPort), nullptr);
    if (status >= 0 && debugPort != 0) return true;

    DWORD debugFlags = 0;
    status = NtQIP(GetCurrentProcess(), 0x1F, &debugFlags, sizeof(debugFlags), nullptr);
    if (status >= 0 && debugFlags == 0) return true;

    return false;
}




static bool PebDebuggerCheck() {
#ifdef _WIN64
    BYTE* peb = (BYTE*)__readgsqword(0x60);
#else
    BYTE* peb = (BYTE*)__readfsdword(0x30);
#endif
    
    if (peb[2] != 0) return true;

    
#ifdef _WIN64
    DWORD ntGlobalFlag = *(DWORD*)(peb + 0xBC);
#else
    DWORD ntGlobalFlag = *(DWORD*)(peb + 0x68);
#endif
    if (ntGlobalFlag & 0x70) return true;

    return false;
}

static bool HeapFlagCheck() {
#ifdef _WIN64
    BYTE* peb  = (BYTE*)__readgsqword(0x60);
    PVOID heap = *(PVOID*)(peb + 0x30);
    DWORD flags      = *(DWORD*)((BYTE*)heap + 0x70);
    DWORD forceFlags = *(DWORD*)((BYTE*)heap + 0x74);
#else
    BYTE* peb  = (BYTE*)__readfsdword(0x30);
    PVOID heap = *(PVOID*)(peb + 0x18);
    DWORD flags      = *(DWORD*)((BYTE*)heap + 0x40);
    DWORD forceFlags = *(DWORD*)((BYTE*)heap + 0x44);
#endif
    return (flags & ~0x2) || (forceFlags != 0);
}




static bool HardwareBreakpointCheck() {
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx)) return false;
    return ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3;
}




static bool RemoteDebuggerCheck() {
    
    BYTE* fn = (BYTE*)CheckRemoteDebuggerPresent;
    if (fn[0] == 0xE9 || fn[0] == 0xEB) return true; 

    BOOL present = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &present);
    return present != FALSE;
}




static bool TimingCheck() {
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    volatile DWORD sum = 0;
    for (int i = 0; i < 1'000'000; ++i) sum ^= (DWORD)i;
    QueryPerformanceCounter(&t1);
    double ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart;
    return ms > 1500.0;
}




static bool SuspiciousProcessCheck() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;

    
    auto check = [&](const wchar_t* procName, const char* decName, size_t decLen) {
        char narrow[MAX_PATH]{};
        WideCharToMultiByte(CP_ACP, 0, procName, -1, narrow, MAX_PATH, nullptr, nullptr);
        for (int i = 0; narrow[i]; ++i) narrow[i] = (char)tolower((unsigned char)narrow[i]);
        return strncmp(narrow, decName, decLen) == 0 && narrow[decLen] == '\0';
    };



#define BL(s) do { \
    if (!found) { \
        std::string _dn = DEC(s); \
        if (check(pe.szExeFile, _dn.c_str(), _dn.size())) found = true; \
        SecureZeroMemory(&_dn[0], _dn.size()); \
    } \
} while(0)

    if (Process32FirstW(snap, &pe)) {
        do {
            BL("cheatengine-x86_64.exe");
            BL("cheatengine.exe");
            BL("cheatengine-i386.exe");
            BL("x64dbg.exe");
            BL("x32dbg.exe");
            BL("ollydbg.exe");
            BL("ida.exe");
            BL("ida64.exe");
            BL("idaq.exe");
            BL("idaq64.exe");
            BL("scylla.exe");
            BL("scylla_x64.exe");
            BL("scylla_x86.exe");
            BL("megadumper.exe");
            BL("processhacker.exe");
            BL("processhacker2.exe");
            BL("systeminformer.exe");
            BL("tcpview.exe");
            BL("wireshark.exe");
            BL("fiddler.exe");
            BL("charles.exe");
            BL("httpdebugger.exe");
            BL("apimonitor-x64.exe");
            BL("apimonitor-x86.exe");
            BL("pestudio.exe");
            BL("pe-bear.exe");
            BL("windbg.exe");
            BL("cdb.exe");
            BL("immunitydebugger.exe");
            if (found) break;
        } while (Process32NextW(snap, &pe));
    }
#undef BL

    CloseHandle(snap);
    return found;
}




static bool SuspiciousWindowCheck() {
    struct EnumData { bool found; };
    EnumData data{ false };

    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto* d = reinterpret_cast<EnumData*>(lp);
        char cls[256]{}, title[256]{};
        GetClassNameA(hwnd, cls, sizeof(cls));
        GetWindowTextA(hwnd, title, sizeof(title));

        for (int i = 0; cls[i]; ++i)   cls[i]   = (char)tolower((unsigned char)cls[i]);
        for (int i = 0; title[i]; ++i) title[i] = (char)tolower((unsigned char)title[i]);

        
        if (strstr(cls,   "tform")        ||
            strstr(title, "cheat engine") ||
            strstr(title, "x64dbg")       ||
            strstr(title, "x32dbg")       ||
            strstr(title, "ollydbg")      ||
            strstr(title, "windbg")       ||
            strstr(cls,   "id-debugger")) {
            d->found = true;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));

    return data.found;
}





static DWORD  g_textHash  = 0;
static BYTE*  g_textBase  = nullptr;
static SIZE_T g_textSize  = 0;

static DWORD HashRegion(const BYTE* base, SIZE_T size) {
    DWORD h = 0x12345678;
    for (SIZE_T i = 0; i < size; ++i)
        h = (h ^ base[i]) * 1000003;
    return h;
}

static void CaptureCodeHash() {
    HMODULE self = GetModuleHandleW(nullptr);
    if (!self) return;

    BYTE* base = (BYTE*)self;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (memcmp(sec->Name, ".text", 5) == 0) {
            g_textBase = base + sec->VirtualAddress;
            g_textSize = sec->Misc.VirtualSize;
            g_textHash = HashRegion(g_textBase, g_textSize);
            return;
        }
    }
}

static bool IntegrityCheck() {
    if (!g_textBase || g_textSize == 0) return false;
    DWORD current = HashRegion(g_textBase, g_textSize);
    return current != g_textHash;
}




static void TerminateSelf() {
    for (volatile int i = 0; i < 200'000; ++i) {} // misdirect timing
    TerminateProcess(GetCurrentProcess(), 0xDEAD);
}

// ============================================================
//  Public API
// ============================================================
static std::atomic<bool> g_monitor_running{ false };

bool runAntiDebugChecks() {
    bool detected = false;

    if (PebDebuggerCheck())        { spdlog::warn("[Sec] PEB flag");           detected = true; }
    if (RemoteDebuggerCheck())     { spdlog::warn("[Sec] Remote dbg");         detected = true; }
    if (NtCheckDebugPort())        { spdlog::warn("[Sec] NtQIP trigger");      detected = true; }
    if (HeapFlagCheck())           { spdlog::warn("[Sec] Heap flags");         detected = true; }
    if (HardwareBreakpointCheck()) { spdlog::warn("[Sec] HW breakpoint");      detected = true; }
    if (SuspiciousProcessCheck())  { spdlog::warn("[Sec] Blacklist hit");      detected = true; }
    if (SuspiciousWindowCheck())   { spdlog::warn("[Sec] Suspicious window");  detected = true; }
    if (IntegrityCheck())          { spdlog::warn("[Sec] Code patch detected");detected = true; }

    if (detected) TerminateSelf();
    return detected;
}

bool runTimingCheck() {
    if (TimingCheck()) {
        spdlog::warn("[Sec] Timing anomaly");
        TerminateSelf();
        return true;
    }
    return false;
}

void startAntiDebugMonitor() {
    // Capture a baseline hash of our own .text section NOW
    CaptureCodeHash();

    if (g_monitor_running.exchange(true)) return;

    std::thread([]() {
        // Stagger start to avoid predictable timing
        std::this_thread::sleep_for(std::chrono::milliseconds(3700));
        while (g_monitor_running) {
            runAntiDebugChecks();
            // Vary sleep interval so it can't be timed and skipped
            LARGE_INTEGER li;
            QueryPerformanceCounter(&li);
            int delay = 4000 + (int)(li.QuadPart % 3000); 
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }).detach();
}

void stopAntiDebugMonitor() {
    g_monitor_running = false;
}
