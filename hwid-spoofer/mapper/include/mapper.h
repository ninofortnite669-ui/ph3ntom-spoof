#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <cstdint>

typedef NTSTATUS(NTAPI* DRIVER_ENTRY)(
    PVOID DriverObject,
    PVOID RegistryPath
);

struct MapContext {
    uint64_t    KernelBase;
    uint64_t    KernelSize;
    uint8_t*    LocalCopy;
    size_t      LocalSize;
    uint64_t    EntryPointRVA;
    std::string DriverPath;
};

struct ResolvedImport {
    uint64_t    ThunkRVA;
    uint64_t    FunctionAddr;
};

NTSTATUS MapDriver(const std::wstring& driverPath);

bool ParsePeHeaders(const uint8_t* raw, size_t rawSize, MapContext& ctx);
bool AllocateKernelMemory(MapContext& ctx);
bool CopySections(const uint8_t* raw, MapContext& ctx);
bool FixRelocations(const uint8_t* raw, MapContext& ctx);
bool ResolveImports(const uint8_t* raw, MapContext& ctx);
bool WriteToKernel(MapContext& ctx);
NTSTATUS CallDriverEntry(MapContext& ctx);
void FreeMapContext(MapContext& ctx);

uint64_t GetNtoskrnlBase();
uint64_t GetKernelProcAddress(uint64_t ntoskrnlBase, const char* funcName);
uint64_t GetExportAddress(uint64_t moduleBase, const char* funcName);
