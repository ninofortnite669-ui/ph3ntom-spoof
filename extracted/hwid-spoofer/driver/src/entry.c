#include "../include/spoofer.h"

//
// Globals
//
PDEVICE_OBJECT   g_ControlDevice      = NULL;
PDEVICE_OBJECT   g_DiskFilters[MAX_DISK_HOOKS] = { 0 };
ULONG            g_DiskFilterCount    = 0;
PDRIVER_DISPATCH g_OrigNdisDispatch   = NULL;
PDRIVER_OBJECT   g_NdisDriverObject   = NULL;
UCHAR            g_FakeMac[6]         = { 0 };
BOOLEAN          g_SpoofActive        = FALSE;

// Nom obfusqué runtime — généré en mémoire, pas de string statique détectable
static WCHAR g_DeviceName[64]  = { 0 };
static WCHAR g_DeviceDos[64]   = { 0 };
static BOOLEAN g_SymlinkExists = FALSE;

DRIVER_UNLOAD   SpDriverUnload;
DRIVER_DISPATCH SpDispatchCreateClose;
DRIVER_DISPATCH SpDispatchControl;


//
// Génère un nom de device aléatoire au format /Device/{xxxxxxxx}
// pour éviter une détection par string statique ou enumération /Device/
//
static VOID GenerateDeviceName(VOID)
{
    LARGE_INTEGER tick;
    KeQuerySystemTime(&tick);

    ULONG rnd = (ULONG)(tick.LowPart ^ tick.HighPart ^ (ULONG)(ULONG_PTR)&tick);

    swprintf(g_DeviceName, sizeof(g_DeviceName),
        L"/Device/{%08X%08X}", rnd, rnd ^ 0xDEADBEEF);
    swprintf(g_DeviceDos, sizeof(g_DeviceDos),
        L"/DosDevices/{%08X%08X}", rnd, rnd ^ 0xDEADBEEF);
}


VOID SpDriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    g_SpoofActive = FALSE;

    SpNicCleanup();
    SpDiskCleanup();
    SpSmbiosRestore();

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
    ULONG              ioctl  = stack->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS           status = STATUS_SUCCESS;

    switch (ioctl) {
    case IOCTL_SPOOFER_RANDOMIZE:
        g_SpoofActive = TRUE;
        SpGenerateMac(g_FakeMac);
        SpSmbiosRestore();
        status = SpSmbiosInitialize();
        break;

    case IOCTL_SPOOFER_RESTORE:
        g_SpoofActive = FALSE;
        SpSmbiosRestore();
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
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

    DriverObject->DriverUnload = NULL;

    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = SpDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SpDispatchControl;

    // Créer le control device
    status = IoCreateDevice(DriverObject, 0, &devName,
        FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &g_ControlDevice);
    if (!NT_SUCCESS(status)) return status;

    g_ControlDevice->Flags &= ~DO_DEVICE_INITIALIZING;

    // Créer le symlink DOS temporairement
    status = IoCreateSymbolicLink(&dosName, &devName);
    if (NT_SUCCESS(status))
        g_SymlinkExists = TRUE;

    // Initialiser les hooks
    SpDiskInitialize();
    SpSmbiosInitialize();
    SpNicInitialize();
    SpGenerateMac(g_FakeMac);
    g_SpoofActive = TRUE;

    // ─────────────────────────────────────────────────────────────────────
    // EAC BYPASS : supprimer le symlink DOS immédiatement après création
    // Le control device reste accessible via /Device/{nom} par NtCreateFile
    // mais n'apparaît plus dans l'énumération /DosDevices/ qu'EAC check
    // ─────────────────────────────────────────────────────────────────────
    if (g_SymlinkExists) {
        IoDeleteSymbolicLink(&dosName);
        g_SymlinkExists = FALSE;
        DbgPrint("[spoof] DOS symlink removed (EAC bypass)/n");
    }

    DbgPrint("[spoof] ready | device: %wZ | disk filters: %u/n",
        &devName, g_DiskFilterCount);

    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    if (!DriverObject)
        return IoCreateDriver(NULL, SpInit);
    return SpInit(DriverObject, RegistryPath);
}
