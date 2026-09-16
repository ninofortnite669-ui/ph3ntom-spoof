#include "../include/spoofer.h"

// Globals
PDEVICE_OBJECT   g_ControlDevice      = NULL;
PDEVICE_OBJECT   g_DiskFilters[MAX_DISK_HOOKS] = { 0 };
ULONG            g_DiskFilterCount    = 0;
BOOLEAN          g_SpoofActive        = FALSE;
BOOLEAN          g_TpmSpoofActive     = FALSE;
BOOLEAN          g_EkSpoofActive      = FALSE;
BOOLEAN          g_CpuSpoofActive     = FALSE;
BOOLEAN          g_UsbSpoofActive     = FALSE;
BOOLEAN          g_EdidSpoofActive    = FALSE;
BOOLEAN          g_VolumeSpoofActive  = FALSE;
BOOLEAN          g_CleanerActive      = FALSE;

// Device name obfuscation
static WCHAR g_DeviceName[64]  = { 0 };
static WCHAR g_DeviceDos[64]   = { 0 };
static BOOLEAN g_SymlinkExists = FALSE;

DRIVER_UNLOAD   SpDriverUnload;
DRIVER_DISPATCH SpDispatchCreateClose;
DRIVER_DISPATCH SpDispatchControl;

// Generate random device name
static VOID GenerateDeviceName(VOID)
{
    LARGE_INTEGER tick;
    KeQuerySystemTime(&tick);
    ULONG rnd = (ULONG)(tick.LowPart ^ tick.HighPart ^ (ULONG)(ULONG_PTR)&tick);
    swprintf(g_DeviceName, ARRAYSIZE(g_DeviceName), L"\\Device\\Ph3ntom_%08X", rnd);
    swprintf(g_DeviceDos, ARRAYSIZE(g_DeviceDos), L"\\DosDevices\\Ph3ntom_%08X", rnd);
}

VOID SpDriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    g_SpoofActive = FALSE;
    g_TpmSpoofActive = FALSE;
    g_EkSpoofActive = FALSE;
    g_CpuSpoofActive = FALSE;
    g_UsbSpoofActive = FALSE;
    g_EdidSpoofActive = FALSE;
    g_VolumeSpoofActive = FALSE;
    g_CleanerActive = FALSE;

    SpTpmCleanup();
    SpTpmEkFullCleanup();
    SpNicCleanup();
    SpDiskCleanup();
    SpSmbiosRestore();
    SpCpuCleanupFull();
    SpUsbCleanup();
    SpEdidCleanup();
    SpVolumeCleanup();
    SpCleanerCleanup();

    if (g_SymlinkExists) {
        UNICODE_STRING dos;
        RtlInitUnicodeString(&dos, g_DeviceDos);
        IoDeleteSymbolicLink(&dos);
        g_SymlinkExists = FALSE;
    }

    if (g_ControlDevice) {
        IoDeleteDevice(g_ControlDevice);
        g_ControlDevice = NULL;
    }
}

NTSTATUS SpDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS SpDispatchControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION stack  = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctl = stack->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS status = STATUS_SUCCESS;
    
    switch (ioctl) {
    case IOCTL_SPOOFER_SPOOF:
        g_SpoofActive = TRUE;
        g_TpmSpoofActive = TRUE;
        g_EkSpoofActive = TRUE;
        g_CpuSpoofActive = TRUE;
        g_UsbSpoofActive = TRUE;
        g_EdidSpoofActive = TRUE;
        g_VolumeSpoofActive = TRUE;
        
        SpSmbiosRestore();
        status = SpSmbiosInitialize();
        if (NT_SUCCESS(status)) {
            SpTpmInitialize();
            SpTpmEkFullInitialize();
            ModifyTpmRegistry();
            SpCpuInitialize();
            SpUsbInitialize();
            SpEdidInitialize();
            SpVolumeInitialize();
            SpNicInitialize();
            SpDiskInitialize();
        }
        break;

    case IOCTL_SPOOFER_RESTORE:
        g_SpoofActive = FALSE;
        g_TpmSpoofActive = FALSE;
        g_EkSpoofActive = FALSE;
        g_CpuSpoofActive = FALSE;
        g_UsbSpoofActive = FALSE;
        g_EdidSpoofActive = FALSE;
        g_VolumeSpoofActive = FALSE;
        
        SpSmbiosRestore();
        SpTpmCleanup();
        SpTpmEkFullCleanup();
        SpCpuCleanupFull();
        SpUsbCleanup();
        SpEdidCleanup();
        SpVolumeCleanup();
        SpNicCleanup();
        SpDiskCleanup();
        break;

    case IOCTL_SPOOFER_STATUS:
        {
            CHAR statusBuf[256];
            snprintf(statusBuf, sizeof(statusBuf), 
                "Spoof: %s, TPM: %s, EK: %s, CPU: %s, USB: %s, EDID: %s, Volume: %s, Disks: %u",
                g_SpoofActive ? "ON" : "OFF",
                g_TpmSpoofActive ? "ON" : "OFF",
                g_EkSpoofActive ? "ON" : "OFF",
                g_CpuSpoofActive ? "ON" : "OFF",
                g_UsbSpoofActive ? "ON" : "OFF",
                g_EdidSpoofActive ? "ON" : "OFF",
                g_VolumeSpoofActive ? "ON" : "OFF",
                g_DiskFilterCount);
            
            if (Irp->AssociatedIrp.SystemBuffer && Irp->AssociatedIrp.SystemBufferSize >= sizeof(statusBuf)) {
                memcpy(Irp->AssociatedIrp.SystemBuffer, statusBuf, sizeof(statusBuf));
                Irp->IoStatus.Information = (ULONG)sizeof(statusBuf);
            }
        }
        break;

    case IOCTL_SPOOFER_CLEAN:
        g_CleanerActive = TRUE;
        status = FullAntiCheatCleanup();
        g_CleanerActive = FALSE;
        break;

    case IOCTL_SPOOFER_FULL:
        // Full cleanup + spoof
        g_CleanerActive = TRUE;
        FullAntiCheatCleanup();
        g_CleanerActive = FALSE;
        
        g_SpoofActive = TRUE;
        g_TpmSpoofActive = TRUE;
        g_EkSpoofActive = TRUE;
        g_CpuSpoofActive = TRUE;
        g_UsbSpoofActive = TRUE;
        g_EdidSpoofActive = TRUE;
        g_VolumeSpoofActive = TRUE;
        
        SpSmbiosRestore();
        status = SpSmbiosInitialize();
        if (NT_SUCCESS(status)) {
            SpTpmInitialize();
            SpTpmEkFullInitialize();
            ModifyTpmRegistry();
            SpCpuInitialize();
            SpUsbInitialize();
            SpEdidInitialize();
            SpVolumeInitialize();
            SpNicInitialize();
            SpDiskInitialize();
        }
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS SpInit(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status;

    GenerateDeviceName();

    UNICODE_STRING devName, dosName;
    RtlInitUnicodeString(&devName, g_DeviceName);
    RtlInitUnicodeString(&dosName, g_DeviceDos);

    DriverObject->DriverUnload = SpDriverUnload;

    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = SpDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SpDispatchControl;

    // Create control device
    status = IoCreateDevice(DriverObject, 0, &devName,
        FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &g_ControlDevice);
    if (!NT_SUCCESS(status)) return status;

    g_ControlDevice->Flags &= ~DO_DEVICE_INITIALIZING;

    // Create DOS symlink
    status = IoCreateSymbolicLink(&dosName, &devName);
    if (NT_SUCCESS(status))
        g_SymlinkExists = TRUE;

    // Initialize all spoofing modules
    SpCleanerInitialize();
    SpDiskInitialize();
    SpSmbiosInitialize();
    SpNicInitialize();
    SpTpmInitialize();
    SpTpmEkFullInitialize();
    SpCpuInitialize();
    SpUsbInitialize();
    SpEdidInitialize();
    SpVolumeInitialize();

    // Remove symlink for EAC bypass
    if (g_SymlinkExists) {
        IoDeleteSymbolicLink(&dosName);
        g_SymlinkExists = FALSE;
    }

    g_SpoofActive = TRUE;
    g_TpmSpoofActive = TRUE;
    g_EkSpoofActive = TRUE;
    g_CpuSpoofActive = TRUE;
    g_UsbSpoofActive = TRUE;
    g_EdidSpoofActive = TRUE;
    g_VolumeSpoofActive = TRUE;

    DbgPrint("[spoof] Ph3ntom Spoofer Ultimate ready | device: %wZ\n", &devName);
    DbgPrint("[spoof] All modules initialized: Disk, SMBIOS, NIC, TPM, EK, CPU, USB, EDID, Volume, Cleaner\n");

    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    if (!DriverObject)
        return IoCreateDriver(NULL, SpInit);
    return SpInit(DriverObject, RegistryPath);
}
