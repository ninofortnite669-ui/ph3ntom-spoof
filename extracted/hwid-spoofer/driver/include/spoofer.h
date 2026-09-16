#pragma once

#include <ntddk.h>
#include <wdm.h>
#include <ntddstor.h>
#include <wdmsec.h>
#include <ntstrsafe.h>

NTKERNELAPI NTSTATUS IoCreateDriver(
    PUNICODE_STRING DriverName,
    PDRIVER_INITIALIZE InitializationFunction
);

NTKERNELAPI NTSTATUS ObReferenceObjectByName(
    PUNICODE_STRING ObjectName,
    ULONG Attributes,
    PACCESS_STATE AccessState,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID ParseContext,
    PVOID *Object
);

extern POBJECT_TYPE *IoDriverObjectType;

//
// Device / IOCTL definitions
//
#define SPOOFER_DEVICE_NAME     L"\\Device\\Spoofer"
#define SPOOFER_DOS_LINK        L"\\DosDevices\\Spoofer"
#define SPOOFER_POOL_TAG        'fpSH'
#define SPOOFER_DISK_TAG        'ksDH'
#define SPOOFER_SMBIOS_TAG      'bmSH'

#define IOCTL_SPOOFER_RANDOMIZE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPOOFER_RESTORE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// Limits
//
#define MAX_DISK_HOOKS      8
#define FAKE_SERIAL_LEN     20

//
// Filter device extension : attaché au-dessus de chaque disk device
//
typedef struct _DISK_FILTER_EXTENSION {
    PDEVICE_OBJECT  LowerDevice;        // device en dessous dans la stack
    CHAR            FakeSerial[FAKE_SERIAL_LEN + 1];
    CHAR            OrigSerial[64];
    BOOLEAN         Active;
} DISK_FILTER_EXTENSION, *PDISK_FILTER_EXTENSION;

//
// Context passé à la completion routine disk
//
typedef struct _DISK_COMPLETION_CONTEXT {
    CHAR    FakeSerial[FAKE_SERIAL_LEN + 1];
    KEVENT  Event;
} DISK_COMPLETION_CONTEXT, *PDISK_COMPLETION_CONTEXT;

//
// Globals
//
extern PDEVICE_OBJECT   g_ControlDevice;
extern PDEVICE_OBJECT   g_DiskFilters[MAX_DISK_HOOKS];
extern ULONG            g_DiskFilterCount;

// Saved NIC dispatch
extern PDRIVER_DISPATCH g_OrigNdisDispatch;
extern PDRIVER_OBJECT   g_NdisDriverObject;
extern UCHAR            g_FakeMac[6];
extern BOOLEAN          g_SpoofActive;

//
// Prototypes — disk_spoof.c
//
NTSTATUS SpDiskInitialize(VOID);
VOID     SpDiskCleanup(VOID);

//
// Prototypes — smbios_spoof.c
//
NTSTATUS SpSmbiosInitialize(VOID);
VOID     SpSmbiosRestore(VOID);

//
// Prototypes — nic_spoof.c
//
NTSTATUS SpNicInitialize(VOID);
VOID     SpNicCleanup(VOID);

//
// Helpers inline
//
static inline VOID
SpGenerateSerial(PCHAR Buffer, ULONG Length)
{
    static const CHAR charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    LARGE_INTEGER seed;
    KeQuerySystemTime(&seed);
    for (ULONG i = 0; i < Length; i++) {
        seed.QuadPart = seed.QuadPart * 6364136223846793005LL + 1442695040888963407LL;
        Buffer[i] = charset[(ULONG)((seed.HighPart ^ seed.LowPart) % (sizeof(charset) - 1))];
        seed.QuadPart ^= (LONGLONG)KeQueryPerformanceCounter(NULL).QuadPart;
    }
    Buffer[Length] = '\0';
}

static inline VOID
SpGenerateMac(PUCHAR Mac)
{
    LARGE_INTEGER seed;
    KeQuerySystemTime(&seed);
    for (ULONG i = 0; i < 6; i++) {
        seed.QuadPart = seed.QuadPart * 6364136223846793005LL + 1442695040888963407LL;
        Mac[i] = (UCHAR)(seed.LowPart ^ seed.HighPart);
    }
    // Bit 0 du premier octet = 0 (unicast), bit 1 = 1 (locally administered)
    Mac[0] = (Mac[0] & 0xFE) | 0x02;
}
