#include "../include/mapper.h"
#include "../include/intel_driver.h"
#include <iostream>
#include <filesystem>
#include <string>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

static void Banner()
{
    std::cout <<
        "  ╔══════════════════════════════════════════════╗\n"
        "  ║     HWID Spoofer — Manual Mapper v2          ║\n"
        "  ║     DSE bypass  : iqvw64e CVE-2015-2291      ║\n"
        "  ║     Exec        : NtQueryIntervalProfile     ║\n"
        "  ║     Cleanup     : PiDdb + MmUnloaded + Pool  ║\n"
        "  ╚══════════════════════════════════════════════╝\n\n";
}

static bool IsAdmin()
{
    BOOL ok = FALSE;
    PSID sid = nullptr;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&auth, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0,0,0,0,0,0, &sid)) {
        CheckTokenMembership(nullptr, sid, &ok);
        FreeSid(sid);
    }
    return ok == TRUE;
}

static std::wstring FindIntelDriver(const std::wstring& selfDir)
{
    std::wstring local = selfDir + L"\\" + INTEL_DRIVER_NAME;
    if (std::filesystem::exists(local)) return local;

    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring sys32 = std::wstring(sys) + L"\\drivers\\" + INTEL_DRIVER_NAME;
    if (std::filesystem::exists(sys32)) return sys32;
    return L"";
}

int wmain(int argc, wchar_t* argv[])
{
    Banner();

    if (!IsAdmin()) {
        std::cerr << "[!] Run as administrator\n";
        return 1;
    }

    if (argc < 2) {
        std::wcout << L"Usage: mapper.exe <spoofer.sys> [iqvw64e.sys]\n";
        return 1;
    }

    std::wstring driverPath = argv[1];
    if (!std::filesystem::exists(driverPath)) {
        std::wcerr << L"[!] Not found: " << driverPath << L"\n";
        return 1;
    }

    // Localiser iqvw64e
    std::wstring intelPath;
    if (argc >= 3) {
        intelPath = argv[2];
    } else {
        wchar_t self[MAX_PATH];
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        intelPath = FindIntelDriver(std::filesystem::path(self).parent_path().wstring());
    }

    if (intelPath.empty() || !std::filesystem::exists(intelPath)) {
        std::cerr << "[!] iqvw64e.sys not found.\n"
                  << "    Place next to mapper.exe or pass as 2nd arg.\n";
        return 1;
    }

    std::wcout << L"[*] Target  : " << driverPath << L"\n";
    std::wcout << L"[*] Exploit : " << intelPath  << L"\n\n";

    // ── Étape 1 : charger le driver Intel ──────────────────────────────────
    std::cout << "[1/5] Loading Intel vulnerable driver...\n";
    HANDLE hIntel = IntelDriver::Load(intelPath);
    if (hIntel == INVALID_HANDLE_VALUE) {
        std::cerr << "[!] Intel driver load failed\n";
        return 1;
    }

    // ── Étape 2 : mapper le spoofer ────────────────────────────────────────
    std::cout << "[2/5] Mapping spoofer...\n";
    NTSTATUS status = MapDriver(driverPath);
    if (!NT_SUCCESS(status)) {
        std::cerr << "[!] Mapping failed: 0x" << std::hex << status << "\n";
        IntelDriver::Unload();
        return 1;
    }

    // ── Étape 3 : PiDdb cleanup ────────────────────────────────────────────
    std::cout << "[3/5] Cleaning PiDdbCacheTable...\n";
    if (IntelDriver::ClearPiDdbCache(hIntel, INTEL_DRIVER_NAME, 0))
        std::cout << "      OK\n";
    else
        std::cout << "      WARN: PiDdb cleanup failed (update patterns?)\n";

    // ── Étape 4 : MmUnloadedDrivers cleanup ───────────────────────────────
    std::cout << "[4/5] Cleaning MmUnloadedDrivers...\n";
    if (IntelDriver::ClearMmUnloadedDrivers(hIntel, INTEL_DRIVER_NAME))
        std::cout << "      OK\n";
    else
        std::cout << "      WARN: MmUnloaded cleanup failed\n";

    // ── Étape 5 : unload Intel ─────────────────────────────────────────────
    std::cout << "[5/5] Unloading Intel driver...\n";
    IntelDriver::Unload();

    std::cout << "\n[+] Done. Spoofer active.\n";
    std::cout << "    Device accessible via NtCreateFile \\\\Device\\\\{GUID}\n";
    std::cout << "    (GUID logged by driver via DbgPrint)\n";

    return 0;
}
