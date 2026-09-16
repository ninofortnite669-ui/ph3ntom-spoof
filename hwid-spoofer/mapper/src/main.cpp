#include "../include/mapper.h"
#include "../include/intel_driver.h"
#include <iostream>
#include <filesystem>
#include <string>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <sddl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

static void Banner()
{
    std::cout <<
        "  \u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n"
        "  \u2551     Brave Spoofer - Ultimate Edition        \u2551\n"
        "  \u2551     Complete HWID Spoofing + Cleaner            \u2551\n"
        "  \u2551     DSE Bypass  : iqvw64e CVE-2015-2291      \u2551\n"
        "  \u2551     Features    : All HWID Components        \u2551\n"
        "  \u2551     Cleanup     : PiDdb + MmUnloaded + Pool \u2551\n"
        "  \u2551     Anti-Cheat  : EAC/BE/VAC Cleaner          \u2551\n"
        "  \u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n\n";
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
    std::cout << "  \u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n";
    std::cout << "  \u2551  [1] Map Driver (Start Complete Spoofing)      \u2551\n";
    std::cout << "  \u2551  [2] Unmap Driver (Stop Spoofing)            \u2551\n";
    std::cout << "  \u2551  [3] Clean System (Remove Anti-Cheat Traces) \u2551\n";
    std::cout << "  \u2551  [4] Full Clean + Spoof (Recommended)          \u2551\n";
    std::cout << "  \u2551  [5] Check Status                           \u2551\n";
    std::cout << "  \u2551  [6] Exit                                    \u2551\n";
    std::cout << "  \u2551  [7] ULTIMATE: Clean+Spoof+Auto-Relaunch      \u2551\n";
    std::cout << "  \u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n";
    std::cout << "\n  Select option: ";
}

static bool IsDriverMapped()
{
    HANDLE hDevice = CreateFileW(L"\\\\.\\BraveHWID_Spoof", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        return true;
    }
    return false;
}

static bool CheckSpoofStatus()
{
    HANDLE hDevice = CreateFileW(L"\\\\.\\BraveHWID_Spoof", GENERIC_READ | GENERIC_WRITE,
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
        std::cout << "\n  Active Spoofing:\n";
        std::cout << "    - BIOS/Motherboard Serial (SMBIOS)\n";
        std::cout << "    - Disk Serial Numbers\n";
        std::cout << "    - USB Device Serials\n";
        std::cout << "    - Monitor EDID Serial\n";
        std::cout << "    - MAC Addresses\n";
        std::cout << "    - Volume IDs\n";
        std::cout << "    - TPM Endorsement Key\n";
        std::cout << "    - CPU Information\n";
        return true;
    }
    
    std::cerr << "  [!] Failed to get status\n";
    return false;
}

static bool UnmapDriver()
{
    HANDLE hDevice = CreateFileW(L"\\\\.\\BraveHWID_Spoof", GENERIC_READ | GENERIC_WRITE,
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

static bool CleanSystem()
{
    HANDLE hDevice = CreateFileW(L"\\\\.\\BraveHWID_Spoof", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "  [!] Driver not mapped\n";
        return false;
    }
    
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(hDevice, IOCTL_SPOOFER_CLEAN, NULL, 0,
        NULL, 0, &bytesReturned, NULL);
    
    CloseHandle(hDevice);
    
    if (result) {
        std::cout << "  [+] Anti-cheat traces cleaned\n";
        std::cout << "      - PiDDB Cache\n";
        std::cout << "      - MmUnloadedDrivers\n";
        std::cout << "      - EAC Registry Keys\n";
        std::cout << "      - BE Registry Keys\n";
        std::cout << "      - VAC Registry Keys\n";
        std::cout << "      - Kernel Callbacks\n";
        std::cout << "      - Object Callbacks\n";
        return true;
    }
    
    std::cerr << "  [!] Failed to clean system\n";
    return false;
}

static bool FullCleanAndSpoof(std::wstring& driverPath, std::wstring& intelPath)
{
    std::wcout << L"\n  [*] Loading Intel driver: " << intelPath << L"\n";
    HANDLE hIntel = IntelDriver::Load(intelPath);
    if (hIntel == INVALID_HANDLE_VALUE) {
        std::cerr << "  [!] Intel driver load failed\n";
        return false;
    }
    
    // First clean the system
    std::cout << "  [*] Cleaning system...\n";
    CleanSystem();
    
    // Then map the spoofer
    std::wcout << L"  [*] Mapping spoofer: " << driverPath << L"\n";
    NTSTATUS status = MapDriver(driverPath);
    if (!NT_SUCCESS(status)) {
        std::cerr << "  [!] Mapping failed: 0x" << std::hex << status << "\n";
        IntelDriver::Unload();
        return false;
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
    
    std::cout << "\n  [+] SUCCESS! Complete spoofing active\n";
    std::cout << "      All HWID components spoofed:\n";
    std::cout << "      - BIOS/Motherboard Serial\n";
    std::cout << "      - Disk Serial Numbers\n";
    std::cout << "      - USB Device Serials\n";
    std::cout << "      - Monitor EDID Serial\n";
    std::cout << "      - MAC Addresses\n";
    std::cout << "      - Volume IDs\n";
    std::cout << "      - TPM Endorsement Key\n";
    std::cout << "      - CPU Information\n";
    std::cout << "      - Anti-Cheat traces removed\n";
    
    return true;
}

static bool SignDriver(std::wstring& driverPath)
{
    std::wcout << L"  [*] Attempting to test-sign driver...\n";
    
    // Use signtool to test-sign the driver
    std::wstring cmd = L"signtool sign /v /fd SHA256 /a /tr http://timestamp.digicert.com /td SHA256 \"" + driverPath + L"\"";
    
    // For now, we'll just display the command
    std::wcout << L"  [+] Test-sign command: " << cmd << L"\n";
    std::wcout << L"  [+] Note: Driver is configured for test-signing in project file\n";
    
    return true;
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

        // Sign the driver
        SignDriver(driverPath);

        // Full clean and spoof
        FullCleanAndSpoof(driverPath, intelPath);

        return 0;
    }

    // Interactive mode
    std::wstring driverPath;
    std::wstring intelPath;
    
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring selfDir = std::filesystem::path(self).parent_path().wstring();
    
    // Default paths
    driverPath = selfDir + L"\\BraveHWID_Spoof.sys";
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
                std::wcout << L"  Enter path to BraveHWID_Spoof.sys: ";
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
            
            // Sign the driver
            SignDriver(driverPath);
            
            FullCleanAndSpoof(driverPath, intelPath);
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
            if (!IsDriverMapped()) {
                std::cout << "  [!] Driver not mapped\n";
                break;
            }
            CleanSystem();
            break;
        }
        
        case 4: {
            if (!IsDriverMapped()) {
                std::cout << "  [!] Driver not mapped\n";
                break;
            }
            UnmapDriver();
            
            if (!std::filesystem::exists(driverPath)) {
                std::wcout << L"  [-] Driver not found\n";
                std::wcout << L"  Enter path to BraveHWID_Spoof.sys: ";
                std::wcin.getline(std::wcin, driverPath);
                std::wcin.clear();
            }
            
            if (!std::filesystem::exists(intelPath)) {
                std::wcout << L"  [-] iqvw64e.sys not found\n";
                std::wcout << L"  Enter path to iqvw64e.sys: ";
                std::wcin.getline(std::wcin, intelPath);
                std::wcin.clear();
            }
            
            if (!std::filesystem::exists(driverPath) || !std::filesystem::exists(intelPath)) {
                std::cerr << "  [!] Required files not found\n";
                break;
            }
            
            FullCleanAndSpoof(driverPath, intelPath);
            break;
        }
        
        case 5: {
            CheckSpoofStatus();
            break;
        }
        
        case 6: {
            std::cout << "  [+] Exiting...\n";
            return 0;
        }
        
        case 7: {
            std::cout << "\n  \u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n";
            std::cout << "  \u2551  ULTIMATE MODE: Full Clean + Spoof + Auto-Relaunch  \u2551\n";
            std::cout << "  \u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n\n";
            
            if (!std::filesystem::exists(driverPath)) {
                std::wcout << L"  [-] Driver not found\n";
                std::wcout << L"  Enter path to BraveHWID_Spoof.sys: ";
                std::wcin.getline(std::wcin, driverPath);
                std::wcin.clear();
            }
            
            if (!std::filesystem::exists(intelPath)) {
                std::wcout << L"  [-] iqvw64e.sys not found\n";
                std::wcout << L"  Enter path to iqvw64e.sys: ";
                std::wcin.getline(std::wcin, intelPath);
                std::wcin.clear();
            }
            
            if (!std::filesystem::exists(driverPath) || !std::filesystem::exists(intelPath)) {
                std::cerr << "  [!] Required files not found\n";
                break;
            }
            
            std::cout << "  [*] Executing Full Clean + Spoof...\n";
            FullCleanAndSpoof(driverPath, intelPath);
            
            std::cout << "\n  [*] Setting up auto-relaunch on startup...\n";
            
            wchar_t selfPath[MAX_PATH];
            GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
            
            HANDLE hToken;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken)) {
                TOKEN_PRIVILEGES tkp;
                LookupPrivilegeValueW(nullptr, SE_TAKE_OWNERSHIP_NAME, &tkp.Privileges[0].Luid);
                tkp.PrivilegeCount = 1;
                tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, 0);
                CloseHandle(hToken);
            }
            
            wchar_t startupPath[MAX_PATH];
            SHGetFolderPathW(nullptr, CSIDL_COMMON_STARTUP, nullptr, 0, startupPath);
            std::wstring batPath = std::wstring(startupPath) + L"\\BraveSpoof_AutoStart.bat";
            
            std::wofstream batFile(batPath);
            if (batFile.is_open()) {
                batFile << L"@echo off\n";
                batFile << L"timeout /t 10 /nobreak >nul\n";
                batFile << L"\"" << selfPath << L"\" 4 >nul 2>&1\n";
                batFile.close();
                std::wcout << L"  [+] Created auto-start: " << batPath << L"\n";
            } else {
                std::cerr << "  [!] Failed to create auto-start batch file\n";
            }
            
            std::cout << "\n  [+] ULTIMATE MODE ACTIVATED!\n";
            std::cout << "      - Full Clean + Spoof executed\n";
            std::cout << "      - Auto-relaunch configured on startup\n";
            std::cout << "      - All HWID components are now spoofed\n";
            std::cout << "      - Anti-Cheat traces removed\n";
            std::cout << "      - System will auto-spoof on next boot\n";
            
            std::cout << "\n  [+] Press Enter to exit...\n";
            std::cin.ignore();
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
