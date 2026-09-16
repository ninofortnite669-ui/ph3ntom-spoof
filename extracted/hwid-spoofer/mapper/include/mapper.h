#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <cstdint>

//
// Retourne d'un DriverEntry kernel
//
typedef NTSTATUS(NTAPI* DRIVER_ENTRY)(
    PVOID DriverObject,
    PVOID RegistryPath
);

//
// Contexte d'un mapping en cours
//
struct MapContext {
    uint64_t    KernelBase;         // Base allouée en kernel (via Intel driver)
    uint64_t    KernelSize;         // Taille allouée
    uint8_t*    LocalCopy;          // Copie locale modifiable
    size_t      LocalSize;
    uint64_t    EntryPointRVA;
    std::string DriverPath;
};

//
// Résultat de la résolution d'une import
//
struct ResolvedImport {
    uint64_t    ThunkRVA;           // RVA dans l'IAT
    uint64_t    FunctionAddr;       // Adresse dans ntoskrnl
};

//
// Mapper API
//

// Mappe le driver en kernel et appelle DriverEntry
// Retourne STATUS_SUCCESS ou un NTSTATUS d'erreur
NTSTATUS MapDriver(const std::wstring& driverPath);

// Etapes internes
bool ParsePeHeaders(const uint8_t* raw, size_t rawSize, MapContext& ctx);
bool AllocateKernelMemory(MapContext& ctx);
bool CopySections(const uint8_t* raw, MapContext& ctx);
bool FixRelocations(const uint8_t* raw, MapContext& ctx);
bool ResolveImports(const uint8_t* raw, MapContext& ctx);
bool WriteToKernel(MapContext& ctx);
NTSTATUS CallDriverEntry(MapContext& ctx);
void FreeMapContext(MapContext& ctx);

//
// Helpers
//
uint64_t GetNtoskrnlBase();
uint64_t GetKernelProcAddress(uint64_t ntoskrnlBase, const char* funcName);
uint64_t GetExportAddress(uint64_t moduleBase, const char* funcName);
