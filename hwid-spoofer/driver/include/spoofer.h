#pragma once

#include <ntddk.h>
#include <wdm.h>
#include <ntddstor.h>

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

// Device / IOCTL definitions
#define SPOOFER_DEVICE_NAME     L"\\Device\\Ph3ntomSpoof"
#define SPOOFER_DOS_LINK        L"\\DosDevices\\Ph3ntomSpoof"
#define SPOOFER_POOL_TAG        'PHS3'

// IOCTL Codes
#define IOCTL_SPOOFER_SPOOF     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPOOFER_RESTORE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPOOFER_STATUS    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPOOFER_CLEAN     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPOOFER_FULL      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)

// USB IOCTLs
#define IOCTL_USB_GET_DESCRIPTOR CTL_CODE(FILE_DEVICE_USB, 0x0220, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Volume IOCTLs
#define IOCTL_VOLUME_GET_VOLUME_INFO CTL_CODE(FILE_DEVICE_DISK, 0x0006, METHOD_BUFFERED, FILE_ANY_ACCESS)

// TPM IOCTLs
#define IOCTL_TPM_SEND_COMMAND 0x222000

// Limits
#define MAX_DISK_HOOKS      8
#define MAX_NIC_HOOKS       8
#define MAX_USB_HOOKS       16
#define MAX_VOLUME_HOOKS    16
#define FAKE_SERIAL_LEN     32
#define FAKE_UUID_LEN       36

// Globals
extern PDEVICE_OBJECT   g_ControlDevice;
extern PDEVICE_OBJECT   g_DiskFilters[MAX_DISK_HOOKS];
extern ULONG            g_DiskFilterCount;
extern BOOLEAN          g_SpoofActive;

// TPM Spoofing
extern BOOLEAN          g_TpmSpoofActive;
extern BOOLEAN          g_EkSpoofActive;

// CPU Spoofing
extern BOOLEAN          g_CpuSpoofActive;

// USB Spoofing
extern BOOLEAN          g_UsbSpoofActive;

// EDID Spoofing
extern BOOLEAN          g_EdidSpoofActive;

// Volume Spoofing
extern BOOLEAN          g_VolumeSpoofActive;

// Cleaner
extern BOOLEAN          g_CleanerActive;

// Filter device extension
typedef struct _DISK_FILTER_EXTENSION {
    PDEVICE_OBJECT  LowerDevice;
    CHAR            FakeSerial[FAKE_SERIAL_LEN + 1];
    CHAR            OrigSerial[64];
    BOOLEAN         Active;
} DISK_FILTER_EXTENSION, *PDISK_FILTER_EXTENSION;

// NIC Spoofing
typedef struct _NIC_HOOK {
    PDRIVER_DISPATCH OrigDispatch;
    BOOLEAN         Active;
    UCHAR            FakeMac[6];
} NIC_HOOK, *PNIC_HOOK;

// USB Spoofing
typedef struct _USB_HOOK {
    PDRIVER_DISPATCH OrigDispatch;
    BOOLEAN Active;
    USHORT FakeVendorId;
    USHORT FakeProductId;
    WCHAR FakeSerial[64];
} USB_HOOK, *PUSB_HOOK;

// Volume Spoofing
typedef struct _VOLUME_HOOK {
    PDRIVER_DISPATCH OrigDispatch;
    BOOLEAN Active;
    ULONG FakeVolumeId;
    WCHAR FakeVolumeLabel[32];
} VOLUME_HOOK, *PVOLUME_HOOK;

// Prototypes - disk_spoof.c
NTSTATUS SpDiskInitialize(VOID);
VOID     SpDiskCleanup(VOID);

// Prototypes - smbios_spoof.c
NTSTATUS SpSmbiosInitialize(VOID);
VOID     SpSmbiosRestore(VOID);

// Prototypes - nic_spoof.c
NTSTATUS SpNicInitialize(VOID);
VOID     SpNicCleanup(VOID);

// Prototypes - tpm_spoof.c
NTSTATUS SpTpmInitialize(VOID);
VOID     SpTpmCleanup(VOID);

// Prototypes - cpu_spoof.c
NTSTATUS SpCpuInitialize(VOID);
VOID     SpCpuCleanupFull(VOID);

// Prototypes - usb_spoof.c
NTSTATUS SpUsbInitialize(VOID);
VOID     SpUsbCleanup(VOID);

// Prototypes - edid_spoof.c
NTSTATUS SpEdidInitialize(VOID);
VOID     SpEdidCleanup(VOID);

// Prototypes - volume_spoof.c
NTSTATUS SpVolumeInitialize(VOID);
VOID     SpVolumeCleanup(VOID);

// Prototypes - tpm_ek_spoof.c
NTSTATUS SpTpmEkFullInitialize(VOID);
VOID     SpTpmEkFullCleanup(VOID);

// Prototypes - cleaner
NTSTATUS SpCleanerInitialize(VOID);
VOID     SpCleanerCleanup(VOID);
NTSTATUS FullAntiCheatCleanup(VOID);

// Helpers
static FORCEINLINE VOID
SpGenerateSerial(PCHAR Buffer, ULONG Length)
{
    static const CHAR charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    LARGE_INTEGER seed;
    KeQuerySystemTime(&seed);
    for (ULONG i = 0; i < Length; i++) {
        seed.QuadPart = seed.QuadPart * 6364136223846793005LL + 1442695040888963407LL;
        Buffer[i] = charset[(ULONG)((seed.HighPart ^ seed.LowPart) % (sizeof(charset) - 1))];
    }
    Buffer[Length] = '\0';
}

static FORCEINLINE VOID
SpGenerateMac(PUCHAR Mac)
{
    LARGE_INTEGER seed;
    KeQuerySystemTime(&seed);
    for (ULONG i = 0; i < 6; i++) {
        seed.QuadPart = seed.QuadPart * 6364136223846793005LL + 1442695040888963407LL;
        Mac[i] = (UCHAR)(seed.LowPart ^ seed.HighPart);
    }
    Mac[0] = (Mac[0] & 0xFE) | 0x02;
}

static FORCEINLINE VOID
SpGenerateUUID(PCHAR Buffer)
{
    SpGenerateSerial(Buffer, 8);  Buffer[8] = '-';
    SpGenerateSerial(Buffer + 9, 4);  Buffer[13] = '-';
    SpGenerateSerial(Buffer + 14, 4); Buffer[18] = '-';
    SpGenerateSerial(Buffer + 19, 4); Buffer[23] = '-';
    SpGenerateSerial(Buffer + 24, 12); Buffer[36] = '\0';
}
