#include "../include/mapper.h"
#include "../include/intel_driver.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")
#include <winternl.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#endif
#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xC0000008L)
#endif
#ifndef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xC000009AL)
#endif
#ifndef STATUS_OBJECT_NAME_NOT_FOUND
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034L)
#endif
#ifndef STATUS_INVALID_IMAGE_FORMAT
#define STATUS_INVALID_IMAGE_FORMAT ((NTSTATUS)0xC000007BL)
#endif
#ifndef STATUS_DRIVER_ENTRYPOINT_NOT_FOUND
#define STATUS_DRIVER_ENTRYPOINT_NOT_FOUND ((NTSTATUS)0xC0000263L)
#endif

// ─────────────────────────────────────────────────────────────────────────────
// ntoskrnl base
// ─────────────────────────────────────────────────────────────────────────────
uint64_t GetNtoskrnlBase()
{
    LPVOID mods[1024]; DWORD needed = 0;
    if (!EnumDeviceDrivers(mods, sizeof(mods), &needed)) return 0;
    DWORD cnt = needed / sizeof(LPVOID);
    WCHAR name[MAX_PATH];
    for (DWORD i = 0; i < cnt; i++) {
        if (GetDeviceDriverBaseNameW(mods[i], name, MAX_PATH))
            if (!_wcsicmp(name, L"ntoskrnl.exe") || !_wcsicmp(name, L"ntkrnlmp.exe"))
                return (uint64_t)mods[i];
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Export résolution
// ─────────────────────────────────────────────────────────────────────────────
uint64_t GetKernelProcAddress(uint64_t ntoskrnlKernelBase, const char* funcName)
{
    HMODULE hNtos = LoadLibraryExW(L"ntoskrnl.exe", nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hNtos) return 0;
    uint64_t local = (uint64_t)GetProcAddress(hNtos, funcName);
    uint64_t base  = (uint64_t)hNtos;
    FreeLibrary(hNtos);
    if (!local) return 0;
    return ntoskrnlKernelBase + (local - base);
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse PE
// ─────────────────────────────────────────────────────────────────────────────
bool ParsePeHeaders(const uint8_t* raw, size_t rawSize, MapContext& ctx)
{
    if (rawSize < sizeof(IMAGE_DOS_HEADER)) return false;
    auto* dos = (const IMAGE_DOS_HEADER*)raw;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = (const IMAGE_NT_HEADERS64*)(raw + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return false;

    ctx.KernelSize    = nt->OptionalHeader.SizeOfImage;
    ctx.EntryPointRVA = nt->OptionalHeader.AddressOfEntryPoint;
    ctx.LocalSize     = ctx.KernelSize;
    ctx.LocalCopy     = (uint8_t*)VirtualAlloc(nullptr, ctx.LocalSize,
                             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ctx.LocalCopy) return false;
    memset(ctx.LocalCopy, 0, ctx.LocalSize);
    memcpy(ctx.LocalCopy, raw, nt->OptionalHeader.SizeOfHeaders);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Copy sections
// ─────────────────────────────────────────────────────────────────────────────
bool CopySections(const uint8_t* raw, MapContext& ctx)
{
    auto* dos = (const IMAGE_DOS_HEADER*)raw;
    auto* nt  = (const IMAGE_NT_HEADERS64*)(raw + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (!sec->SizeOfRawData) continue;
        if (sec->VirtualAddress + sec->SizeOfRawData > ctx.LocalSize) return false;
        memcpy(ctx.LocalCopy + sec->VirtualAddress,
               raw + sec->PointerToRawData, sec->SizeOfRawData);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Relocations
// ─────────────────────────────────────────────────────────────────────────────
bool FixRelocations(const uint8_t* raw, MapContext& ctx)
{
    auto* dos = (const IMAGE_DOS_HEADER*)raw;
    auto* nt  = (const IMAGE_NT_HEADERS64*)(raw + dos->e_lfanew);
    int64_t delta = (int64_t)(ctx.KernelBase - nt->OptionalHeader.ImageBase);
    if (!delta) return true;

    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (!dir.VirtualAddress) return (delta == 0);

    auto* reloc = (const IMAGE_BASE_RELOCATION*)(ctx.LocalCopy + dir.VirtualAddress);
    uint32_t done = 0;
    while (done < dir.Size && reloc->VirtualAddress) {
        uint32_t cnt = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        auto* entries = (const WORD*)((const uint8_t*)reloc + sizeof(IMAGE_BASE_RELOCATION));
        for (uint32_t i = 0; i < cnt; i++) {
            if ((entries[i] >> 12) == IMAGE_REL_BASED_DIR64) {
                uint64_t* p = (uint64_t*)(ctx.LocalCopy +
                    reloc->VirtualAddress + (entries[i] & 0xFFF));
                *p += (uint64_t)delta;
            }
        }
        done += reloc->SizeOfBlock;
        reloc = (const IMAGE_BASE_RELOCATION*)((const uint8_t*)reloc + reloc->SizeOfBlock);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Imports
// ─────────────────────────────────────────────────────────────────────────────
bool ResolveImports(const uint8_t* raw, MapContext& ctx)
{
    auto* dos = (const IMAGE_DOS_HEADER*)raw;
    auto* nt  = (const IMAGE_NT_HEADERS64*)(raw + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return true;

    uint64_t ntoskrnl = GetNtoskrnlBase();
    auto* desc = (const IMAGE_IMPORT_DESCRIPTOR*)(ctx.LocalCopy + dir.VirtualAddress);

    while (desc->Name) {
        auto* thunk  = (IMAGE_THUNK_DATA64*)(ctx.LocalCopy + desc->FirstThunk);
        auto* oThunk = (const IMAGE_THUNK_DATA64*)(ctx.LocalCopy +
            (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));

        while (oThunk->u1.AddressOfData) {
            uint64_t addr = 0;
            if (!(oThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                auto* ibn = (const IMAGE_IMPORT_BY_NAME*)
                    (ctx.LocalCopy + oThunk->u1.AddressOfData);
                addr = GetKernelProcAddress(ntoskrnl, ibn->Name);
                if (!addr) {
                    std::cerr << "[mapper] unresolved: " << ibn->Name << "\n";
                    return false;
                }
            }
            thunk->u1.Function = addr;
            thunk++; oThunk++;
        }
        desc++;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Alloc kernel memory
// ─────────────────────────────────────────────────────────────────────────────
bool AllocateKernelMemory(MapContext& ctx)
{
    HANDLE h = IntelDriver::GetHandle();
    ctx.KernelBase = IntelDriver::AllocatePool(h, ctx.KernelSize);
    if (!ctx.KernelBase) {
        std::cerr << "[mapper] AllocPool failed\n";
        return false;
    }
    std::cout << "[mapper] kernel alloc @ 0x" << std::hex << ctx.KernelBase
              << " (" << std::dec << ctx.KernelSize << " bytes)\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Write to kernel
// ─────────────────────────────────────────────────────────────────────────────
bool WriteToKernel(MapContext& ctx)
{
    return IntelDriver::WriteKernelMemory(
        IntelDriver::GetHandle(), ctx.KernelBase, ctx.LocalCopy, ctx.LocalSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scrub PE header — EAC pool scan bypass
// Écrase MZ + PE header avec des zéros après mapping
// ─────────────────────────────────────────────────────────────────────────────
static void ScrubPeHeader(MapContext& ctx)
{
    auto* dos = (const IMAGE_DOS_HEADER*)ctx.LocalCopy;
    auto* nt  = (const IMAGE_NT_HEADERS64*)(ctx.LocalCopy + dos->e_lfanew);
    size_t headerSize = nt->OptionalHeader.SizeOfHeaders;

    std::vector<uint8_t> zeros(headerSize, 0);
    IntelDriver::WriteKernelMemory(
        IntelDriver::GetHandle(), ctx.KernelBase, zeros.data(), headerSize);
    std::cout << "[mapper] PE header scrubbed (" << headerSize << " bytes)\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// CallDriverEntry via NtQueryIntervalProfile hook
//
// Technique classique (kdmapper) :
//  1. Trouver NtQueryIntervalProfile dans ntoskrnl (syscall, peu surveillé)
//  2. Sauvegarder les premiers 20 bytes
//  3. Patcher avec un shellcode JMP → notre DriverEntry
//  4. Déclencher depuis user-mode via ntdll!NtQueryIntervalProfile
//  5. Restaurer les bytes originaux
// ─────────────────────────────────────────────────────────────────────────────
NTSTATUS CallDriverEntry(MapContext& ctx)
{
    HANDLE h = IntelDriver::GetHandle();
    if (h == INVALID_HANDLE_VALUE) return STATUS_INVALID_HANDLE;

    uint64_t ntoskrnl   = GetNtoskrnlBase();
    uint64_t entryPoint = ctx.KernelBase + ctx.EntryPointRVA;

    // Adresse kernel de NtQueryIntervalProfile
    uint64_t ntqipKernel = GetKernelProcAddress(ntoskrnl, "NtQueryIntervalProfile");
    if (!ntqipKernel) {
        std::cerr << "[mapper] NtQueryIntervalProfile not found\n";
        return STATUS_UNSUCCESSFUL;
    }
    std::cout << "[mapper] NtQueryIntervalProfile @ 0x" << std::hex << ntqipKernel << "\n";
    std::cout << "[mapper] DriverEntry           @ 0x" << std::hex << entryPoint   << "\n";

    // Shellcode : xor rcx,rcx | xor rdx,rdx | mov rax,<EP> | call rax | ret
    uint8_t shellcode[] = {
        0x48, 0x31, 0xC9,                                       // xor rcx, rcx
        0x48, 0x31, 0xD2,                                       // xor rdx, rdx
        0x48, 0xB8,                                              // mov rax, imm64
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,                // <- entryPoint
        0xFF, 0xD0,                                              // call rax
        0xC3                                                     // ret
    };
    *reinterpret_cast<uint64_t*>(shellcode + 8) = entryPoint;

    // Sauvegarder les bytes originaux de NtQueryIntervalProfile
    uint8_t origBytes[sizeof(shellcode)] = {};
    IntelDriver::ReadKernelMemory(h, origBytes, ntqipKernel, sizeof(shellcode));

    // Patcher
    IntelDriver::WriteKernelMemory(h, ntqipKernel, shellcode, sizeof(shellcode));

    // Déclencher depuis user-mode
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        using NtQIP_t = NTSTATUS(NTAPI*)(ULONG, PULONG);
        auto fn = (NtQIP_t)GetProcAddress(hNtdll, "NtQueryIntervalProfile");
        if (fn) {
            ULONG interval = 0;
            fn(0, &interval);
        }
    }

    // Restaurer les bytes originaux
    IntelDriver::WriteKernelMemory(h, ntqipKernel, origBytes, sizeof(shellcode));
    std::cout << "[mapper] DriverEntry called, NtQueryIntervalProfile restored\n";

    return STATUS_SUCCESS;
}

// ─────────────────────────────────────────────────────────────────────────────
void FreeMapContext(MapContext& ctx)
{
    if (ctx.LocalCopy) {
        VirtualFree(ctx.LocalCopy, 0, MEM_RELEASE);
        ctx.LocalCopy = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MapDriver — séquence complète
// ─────────────────────────────────────────────────────────────────────────────
NTSTATUS MapDriver(const std::wstring& driverPath)
{
    std::ifstream file(driverPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return STATUS_OBJECT_NAME_NOT_FOUND;

    size_t fsz = (size_t)file.tellg();
    file.seekg(0);
    std::vector<uint8_t> raw(fsz);
    file.read((char*)raw.data(), fsz);
    file.close();

    MapContext ctx{};
    const uint8_t* rawPtr = raw.data();

    std::cout << "[mapper] parsing PE...\n";
    if (!ParsePeHeaders(rawPtr, fsz, ctx)) return STATUS_INVALID_IMAGE_FORMAT;

    std::cout << "[mapper] copying sections...\n";
    if (!CopySections(rawPtr, ctx)) { FreeMapContext(ctx); return STATUS_INVALID_IMAGE_FORMAT; }

    std::cout << "[mapper] allocating kernel memory...\n";
    if (!AllocateKernelMemory(ctx)) { FreeMapContext(ctx); return STATUS_INSUFFICIENT_RESOURCES; }

    std::cout << "[mapper] fixing relocations...\n";
    if (!FixRelocations(rawPtr, ctx)) {
        IntelDriver::FreePool(IntelDriver::GetHandle(), ctx.KernelBase);
        FreeMapContext(ctx); return STATUS_INVALID_IMAGE_FORMAT;
    }

    std::cout << "[mapper] resolving imports...\n";
    if (!ResolveImports(rawPtr, ctx)) {
        IntelDriver::FreePool(IntelDriver::GetHandle(), ctx.KernelBase);
        FreeMapContext(ctx); return STATUS_DRIVER_ENTRYPOINT_NOT_FOUND;
    }

    std::cout << "[mapper] writing to kernel...\n";
    if (!WriteToKernel(ctx)) {
        IntelDriver::FreePool(IntelDriver::GetHandle(), ctx.KernelBase);
        FreeMapContext(ctx); return STATUS_UNSUCCESSFUL;
    }

    // Scrub PE header avant d'appeler DriverEntry — EAC pool scan
    std::cout << "[mapper] scrubbing PE header...\n";
    ScrubPeHeader(ctx);

    std::cout << "[mapper] calling DriverEntry...\n";
    NTSTATUS status = CallDriverEntry(ctx);

    FreeMapContext(ctx);

    if (NT_SUCCESS(status))
        std::cout << "[mapper] mapped successfully\n";
    else
        std::cerr << "[mapper] DriverEntry failed: 0x" << std::hex << status << "\n";

    return status;
}
