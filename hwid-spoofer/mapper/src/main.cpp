#include "../include/mapper.h"
#include "../include/intel_driver.h"
#include <iostream>
#include <filesystem>
#include <string>
#include <windows.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

static void Banner()
{
    std::cout <<
        "  \u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n"
        "  \u2551     Ph3ntom Spoofer - Ultimate Edition        \u2551\n"
        "  \u2551     DSE Bypass  : iqvw64e CVE-2015-2291      \u2551\n"
        "  \u2551     Features    : Disk+SMBIOS+NIC+TPM      \u2551\n"
        "  \u2551     Cleanup     : PiDdb + MmUnloaded + Pool \u2551\n"
        "  \u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n\n";
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

static void ShowMenu()
{
    std::cout << "\n";
    std::cout << "  [1] Map Driver (Start Spoofing)\n";
    std::cout << "  [2] Unmap Driver (Stop Spoofing)\n";
    std::cout << "  [3] Check Status\n";
    std::cout << "  [4] Exit\n";
    std::cout << "\n  Select option: ";
}

static bool IsDriverMapped()
{
    HANDLE hDevice = CreateFileW(L"\\\\.\\Ph3ntomSpoof", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        return true;
    }
    return false;
}

static bool CheckSpoofStatus()
{
    HANDLE hDevice = CreateFileW(L"\\\\.\\Ph3ntomSpoof", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "  [!] Driver not mapped\n";
        return false;
    }
    
    CHAR status[256];
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(hDevice, IOCTL_SPOOFER_STATUS, NULL, 0,
        status, sizeof(status), &bytesReturned, NULL);
    
    CloseHandle(hDevice);
    
    if (result) {
        std::cout << "  [+] Status: " << status << "\n";
        return true;
    }
    
    std::cerr << "  [!] Failed to get status\n";
    return false;
}

static bool UnmapDriver()
{
    HANDLE hDevice = CreateFileW(L"\\\\.\\Ph3ntomSpoof", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "  [!] Driver not mapped\n";
        return false;
    }
    
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(hDevice, IOCTL_SPOOFER_RESTORE, NULL, 0,
        NULL, 0, &bytesReturned, NULL);
    
    CloseHandle(hDevice);
    
    if (result) {
        std::cout << "  [+] Spoofing stopped and restored\n";
        return true;
    }
    
    std::cerr << "  [!] Failed to stop spoofing\n";
    return false;
}

int wmain(int argc, wchar_t* argv[])
{
    Banner();

    if (!IsAdmin()) {
        std::cerr << "[!] Run as administrator\n";
        return 1;
    }

    // Check if we should run in legacy mode (with arguments)
    if (argc >= 2) {
        // Legacy mode - for compatibility with existing scripts
        std::wstring driverPath = argv[1];
        if (!std::filesystem::exists(driverPath)) {
            std::wcerr << L"[!] Not found: " << driverPath << L"\n";
            return 1;
        }

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

        // Step 1: Load Intel vulnerable driver
        std::cout << "[1/6] Loading Intel vulnerable driver...\n";
        HANDLE hIntel = IntelDriver::Load(intelPath);
        if (hIntel == INVALID_HANDLE_VALUE) {
            std::cerr << "[!] Intel driver load failed\n";
            return 1;
        }

        // Step 2: Map spoofer
        std::cout << "[2/6] Mapping Ph3ntomSpoof driver...\n";
        NTSTATUS status = MapDriver(driverPath);
        if (!NT_SUCCESS(status)) {
            std::cerr << "[!] Mapping failed: 0x" << std::hex << status << "\n";
            IntelDriver::Unload();
            return 1;
        }

        // Step 3: PiDdb cleanup
        std::cout << "[3/6] Cleaning PiDdbCacheTable...\n";
        if (IntelDriver::ClearPiDdbCache(hIntel, INTEL_DRIVER_NAME, 0))
            std::cout << "      OK\n";
        else
            std::cout << "      WARN: PiDdb cleanup failed\n";

        // Step 4: MmUnloadedDrivers cleanup
        std::cout << "[4/6] Cleaning MmUnloadedDrivers...\n";
        if (IntelDriver::ClearMmUnloadedDrivers(hIntel, INTEL_DRIVER_NAME))
            std::cout << "      OK\n";
        else
            std::cout << "      WARN: MmUnloaded cleanup failed\n";

        // Step 5: Unload Intel
        std::cout << "[5/6] Unloading Intel driver...\n";
        IntelDriver::Unload();

        std::cout << "\n[+] Ph3ntom Spoofer active!\n";
        std::cout << "    All HWID spoofing functions enabled:\n";
        std::cout << "    - Disk Serial Numbers\n";
        std::cout << "    - SMBIOS (System/Board/Chassis)\n";
        std::cout << "    - NIC MAC Addresses\n";
        std::cout << "    - TPM Information\n";
        std::cout << "    Device: \\\\Device\\\\Ph3ntom_XXXXXXXX\n";

        return 0;
    }

    // Interactive mode
    std::wstring driverPath;
    std::wstring intelPath;
    
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring selfDir = std::filesystem::path(self).parent_path().wstring();
    
    // Default paths
    driverPath = selfDir + L"\\Ph3ntomSpoof.sys";
    intelPath = FindIntelDriver(selfDir);

    while (true) {
        ShowMenu();
        
        int choice = 0;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        switch (choice) {
        case 1: {
            if (IsDriverMapped()) {
                std::cout << "  [!] Driver already mapped\n";
                break;
            }
            
            if (!std::filesystem::exists(driverPath)) {
                std::wcout << L"  [-] Driver not found at: " << driverPath << L"\n";
                std::wcout << L"  Enter path to Ph3ntomSpoof.sys: ";
                std::wcin.getline(std::wcin, driverPath);
                std::wcin.clear();
            }
            
            if (!std::filesystem::exists(intelPath)) {
                std::wcout << L"  [-] iqvw64e.sys not found at: " << intelPath << L"\n";
                std::wcout << L"  Enter path to iqvw64e.sys: ";
                std::wcin.getline(std::wcin, intelPath);
                std::wcin.clear();
            }
            
            if (!std::filesystem::exists(driverPath) || !std::filesystem::exists(intelPath)) {
                std::cerr << "  [!] Required files not found\n";
                break;
            }
            
            std::wcout << L"\n  [*] Loading Intel driver: " << intelPath << L"\n";
            HANDLE hIntel = IntelDriver::Load(intelPath);
            if (hIntel == INVALID_HANDLE_VALUE) {
                std::cerr << "  [!] Intel driver load failed\n";
                break;
            }
            
            std::wcout << L"  [*] Mapping spoofer: " << driverPath << L"\n";
            NTSTATUS status = MapDriver(driverPath);
            if (!NT_SUCCESS(status)) {
                std::cerr << "  [!] Mapping failed: 0x" << std::hex << status << "\n";
                IntelDriver::Unload();
                break;
            }
            
            std::cout << "  [*] Cleaning PiDdbCacheTable...\n";
            if (IntelDriver::ClearPiDdbCache(hIntel, INTEL_DRIVER_NAME, 0))
                std::cout << "      OK\n";
            else
                std::cout << "      WARN\n";
            
            std::cout << "  [*] Cleaning MmUnloadedDrivers...\n";
            if (IntelDriver::ClearMmUnloadedDrivers(hIntel, INTEL_DRIVER_NAME))
                std::cout << "      OK\n";
            else
                std::cout << "      WARN\n";
            
            std::cout << "  [*] Unloading Intel driver...\n";
            IntelDriver::Unload();
            
            std::cout << "\n  [+] SUCCESS! Ph3ntom Spoofer is now active\n";
            std::cout << "      All HWID spoofing functions enabled:\n";
            std::cout << "      - Disk Serial Numbers\n";
            std::cout << "      - SMBIOS (System/Board/Chassis)\n";
            std::cout << "      - NIC MAC Addresses\n";
            std::cout << "      - TPM Information\n";
            break;
        }
        
        case 2: {
            if (!IsDriverMapped()) {
                std::cout << "  [!] Driver not mapped\n";
                break;
            }
            UnmapDriver();
            break;
        }
        
        case 3: {
            CheckSpoofStatus();
            break;
        }
        
        case 4: {
            std::cout << "  [+] Exiting...\n";
            return 0;
        }
        
        default: {
            std::cout << "  [!] Invalid option\n";
            break;
        }
        }
        
        std::cout << "\n  Press Enter to continue...";
        std::cin.ignore();
        std::cout << "\n";
    }
    
    return 0;
}
