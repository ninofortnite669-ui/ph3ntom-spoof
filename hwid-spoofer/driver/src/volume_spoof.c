#include "../include/spoofer.h"

// Volume ID Spoofing - Modifies disk volume serial numbers

// Volume Information Structures
#pragma pack(push, 1)
typedef struct _VOLUME_INFO {
    ULONG VolumeSerialNumber;
    ULONG VolumeCreationTime;
    USHORT VolumeLabelLength;
    WCHAR VolumeLabel[32];
    UCHAR FileSystemType[8];
} VOLUME_INFO, *PVOLUME_INFO;
#pragma pack(pop)

// Volume Hook Structures
#define MAX_VOLUME_HOOKS 16

typedef struct _VOLUME_HOOK {
    PDRIVER_DISPATCH OrigDispatch;
    BOOLEAN Active;
    ULONG FakeVolumeId;
    WCHAR FakeVolumeLabel[32];
} VOLUME_HOOK, *PVOLUME_HOOK;

static VOLUME_HOOK g_VolumeHooks[MAX_VOLUME_HOOKS];
static ULONG g_VolumeHookCount = 0;

// Generate random volume ID
static ULONG GenerateVolumeId(VOID)
{
    LARGE_INTEGER seed;
    KeQuerySystemTime(&seed);
    
    ULONG id = 0;
    for (int i = 0; i < 4; i++) {
        seed.QuadPart = seed.QuadPart * 6364136223846793005LL + 1442695040888963407LL;
        id = (id << 8) | (UCHAR)(seed.LowPart ^ seed.HighPart);
    }
    
    return id;
}

// Generate random volume label
static VOID GenerateVolumeLabel(PWCHAR Buffer, ULONG Length)
{
    static const WCHAR charset[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    LARGE_INTEGER seed;
    KeQuerySystemTime(&seed);
    for (ULONG i = 0; i < Length; i++) {
        seed.QuadPart = seed.QuadPart * 6364136223846793005LL + 1442695040888963407LL;
        Buffer[i] = charset[(ULONG)((seed.HighPart ^ seed.LowPart) % (ARRAYSIZE(charset) - 1))];
    }
    Buffer[Length] = L'\0';
}

// Volume Device Control Hook
static NTSTATUS VolumeDeviceControlHook(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    
    if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
        stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_VOLUME_GET_VOLUME_INFO &&
        g_SpoofActive)
    {
        PVOLUME_INFO volInfo = (PVOLUME_INFO)Irp->AssociatedIrp.SystemBuffer;
        if (volInfo) {
            // Generate fake volume ID
            static ULONG fakeVolumeId = 0;
            if (fakeVolumeId == 0) {
                fakeVolumeId = GenerateVolumeId();
            }
            volInfo->VolumeSerialNumber = fakeVolumeId;
            
            // Generate fake volume label
            static WCHAR fakeLabel[32];
            if (fakeLabel[0] == 0) {
                GenerateVolumeLabel(fakeLabel, 11);
                swprintf(fakeLabel, ARRAYSIZE(fakeLabel), L"VOL-%s", fakeLabel);
            }
            
            volInfo->VolumeLabelLength = (USHORT)wcslen(fakeLabel);
            memcpy(volInfo->VolumeLabel, fakeLabel, volInfo->VolumeLabelLength * sizeof(WCHAR));
            
            // Set filesystem type
            memcpy(volInfo->FileSystemType, "NTFS", 4);
        }
    }
    
    // Call original handler
    for (ULONG i = 0; i < g_VolumeHookCount; i++) {
        if (g_VolumeHooks[i].OrigDispatch) {
            return g_VolumeHooks[i].OrigDispatch(DeviceObject, Irp);
        }
    }
    
    return STATUS_UNSUCCESSFUL;
}

// Initialize Volume Spoofing
NTSTATUS SpVolumeInitialize(VOID)
{
    NTSTATUS status;
    UNICODE_STRING volMgrName = RTL_CONSTANT_STRING(L"\\Driver\\volmgr");
    PDRIVER_OBJECT volMgrDriver = NULL;

    status = ObReferenceObjectByName(&volMgrName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&volMgrDriver);
    
    if (NT_SUCCESS(status) && volMgrDriver) {
        // Save original dispatch
        if (g_VolumeHookCount < MAX_VOLUME_HOOKS) {
            g_VolumeHooks[g_VolumeHookCount].OrigDispatch = volMgrDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL];
            g_VolumeHooks[g_VolumeHookCount].Active = TRUE;
            g_VolumeHooks[g_VolumeHookCount].FakeVolumeId = GenerateVolumeId();
            GenerateVolumeLabel(g_VolumeHooks[g_VolumeHookCount].FakeVolumeLabel, 11);
            g_VolumeHookCount++;
            
            // Hook the dispatch
            volMgrDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = VolumeDeviceControlHook;
        }
        ObDereferenceObject(volMgrDriver);
    }
    
    DbgPrint("[spoof] Volume spoofing initialized, %u hooks installed\n", g_VolumeHookCount);
    return status;
}

// Cleanup Volume Spoofing
VOID SpVolumeCleanup(VOID)
{
    UNICODE_STRING volMgrName = RTL_CONSTANT_STRING(L"\\Driver\\volmgr");
    PDRIVER_OBJECT volMgrDriver = NULL;

    NTSTATUS status = ObReferenceObjectByName(&volMgrName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&volMgrDriver);
    
    if (NT_SUCCESS(status) && volMgrDriver) {
        // Restore original dispatch
        for (ULONG i = 0; i < g_VolumeHookCount; i++) {
            if (g_VolumeHooks[i].OrigDispatch) {
                volMgrDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = g_VolumeHooks[i].OrigDispatch;
            }
        }
        ObDereferenceObject(volMgrDriver);
    }
    
    memset(g_VolumeHooks, 0, sizeof(g_VolumeHooks));
    g_VolumeHookCount = 0;
    DbgPrint("[spoof] Volume spoofing cleaned\n");
}

// Direct Volume ID modification for all drives
NTSTATUS ModifyAllVolumeIds(VOID)
{
    // This function would enumerate all volumes and modify their IDs
    // For this implementation, we'll use the hook approach above
    return STATUS_SUCCESS;
}
