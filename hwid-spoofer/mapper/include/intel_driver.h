#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>

#define IOCTL_IQVW64_COPY_MEMORY        0x80862007
#define IOCTL_IQVW64_UNMAP_MEMORY       0x8086200B
#define IOCTL_IQVW64_ALLOC_NONPAGED     0x80862003
#define IOCTL_IQVW64_FREE_POOL          0x80862005
#define IOCTL_IQVW64_GET_PHYS_ADDR      0x80862009
#define IOCTL_IQVW64_READ_PHYS          0x80862013
#define IOCTL_IQVW64_WRITE_PHYS         0x8086200F

#define INTEL_DEVICE_NAME   L"\\\\.\\Nal"
#define INTEL_SERVICE_NAME  L"iqvw64e"
#define INTEL_DRIVER_NAME   L"iqvw64e.sys"

struct IntelCopyRequest {
    uint64_t Destination;
    uint64_t Source;
    uint64_t Length;
    uint32_t Padding1;
    uint64_t Padding2;
};

struct IntelAllocRequest {
    uint64_t    Size;
    uint64_t    OutAddress;
};

struct IntelFreeRequest {
    uint64_t    Address;
};

namespace IntelDriver {

    HANDLE Load(const std::wstring& driverPath);
    void   Unload();
    HANDLE GetHandle();

    uint64_t AllocatePool(HANDLE hDriver, size_t size);
    bool FreePool(HANDLE hDriver, uint64_t address);
    bool WriteKernelMemory(HANDLE hDriver, uint64_t kernelDst, const void* src, size_t size);
    bool ReadKernelMemory(HANDLE hDriver, void* dst, uint64_t kernelSrc, size_t size);
    bool WriteKernelQWord(HANDLE hDriver, uint64_t kernelAddr, uint64_t value);
    uint64_t ReadKernelQWord(HANDLE hDriver, uint64_t kernelAddr);

    bool ClearPiDdbCache(HANDLE hDriver, const std::wstring& driverName, uint32_t timestamp);
    bool ClearMmUnloadedDrivers(HANDLE hDriver, const std::wstring& driverName);

}
