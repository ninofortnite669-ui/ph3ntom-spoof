#include "../include/spoofer.h"

// Disk Filter Dispatch
static NTSTATUS DiskFilterDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS DiskFilterPass(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS DiskQueryCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

// Patch STORAGE_DEVICE_DESCRIPTOR
static NTSTATUS
DiskQueryCompletion(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp,
    PVOID           Context
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)Context;

    if (NT_SUCCESS(Irp->IoStatus.Status) && g_SpoofActive)
    {
        PSTORAGE_DEVICE_DESCRIPTOR desc = (PSTORAGE_DEVICE_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;

        if (desc && desc->Size >= sizeof(STORAGE_DEVICE_DESCRIPTOR) &&
            desc->SerialNumberOffset != 0 &&
            desc->SerialNumberOffset < desc->Size)
        {
            PCHAR pSerial = (PCHAR)desc + desc->SerialNumberOffset;

            if (ext->OrigSerial[0] == '\0') {
                memcpy(ext->OrigSerial, pSerial, min(strlen(pSerial), sizeof(ext->OrigSerial) - 1));
                ext->OrigSerial[sizeof(ext->OrigSerial) - 1] = '\0';
            }

            if (ext->FakeSerial[0] == '\0') {
                SpGenerateSerial(ext->FakeSerial, FAKE_SERIAL_LEN);
            }

            SIZE_T fakeLen = strlen(ext->FakeSerial);
            SIZE_T origLen = strlen(pSerial);
            SIZE_T copyLen = min(fakeLen, origLen);

            memset(pSerial, 0, origLen + 1);
            memcpy(pSerial, ext->FakeSerial, copyLen);
        }
    }

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }
    return STATUS_CONTINUE_COMPLETION;
}

// Main dispatch
static NTSTATUS
DiskFilterDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
        stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_STORAGE_QUERY_PROPERTY &&
        stack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(STORAGE_PROPERTY_QUERY))
    {
        PSTORAGE_PROPERTY_QUERY query = (PSTORAGE_PROPERTY_QUERY)stack->Parameters.DeviceIoControl.Type3InputBuffer;
        if (!query) query = (PSTORAGE_PROPERTY_QUERY)Irp->AssociatedIrp.SystemBuffer;

        if (query && query->PropertyId == StorageDeviceProperty)
        {
            IoCopyCurrentIrpStackLocationToNext(Irp);
            IoSetCompletionRoutine(Irp, DiskQueryCompletion, ext, TRUE, TRUE, TRUE);
            return IoCallDriver(ext->LowerDevice, Irp);
        }
    }

    return DiskFilterPass(DeviceObject, Irp);
}

// Pass-through for non-intercepted IRPs
static NTSTATUS
DiskFilterPass(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)DeviceObject->DeviceExtension;
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->LowerDevice, Irp);
}

// PnP handling
static NTSTATUS
DiskFilterPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    if (stack->MinorFunction == IRP_MN_REMOVE_DEVICE)
    {
        IoSkipCurrentIrpStackLocation(Irp);
        NTSTATUS status = IoCallDriver(ext->LowerDevice, Irp);
        IoDetachDevice(ext->LowerDevice);
        IoDeleteDevice(DeviceObject);
        return status;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->LowerDevice, Irp);
}

// Attach to disk device
static NTSTATUS
AttachToDisk(PCWSTR DiskPath, PDRIVER_OBJECT DriverObject)
{
    NTSTATUS status;
    UNICODE_STRING uPath;
    PFILE_OBJECT fileObj = NULL;
    PDEVICE_OBJECT targetDev = NULL;
    PDEVICE_OBJECT filterDev = NULL;
    PDEVICE_OBJECT lowerDev = NULL;

    RtlInitUnicodeString(&uPath, DiskPath);

    status = IoGetDeviceObjectPointer(&uPath, FILE_READ_DATA, &fileObj, &targetDev);
    if (!NT_SUCCESS(status)) return status;
    ObDereferenceObject(fileObj);

    if (g_DiskFilterCount >= MAX_DISK_HOOKS) return STATUS_TOO_MANY_COMMANDS;

    status = IoCreateDevice(DriverObject, sizeof(DISK_FILTER_EXTENSION), NULL,
        targetDev->DeviceType, 0, FALSE, &filterDev);
    if (!NT_SUCCESS(status)) return status;

    filterDev->Flags |= targetDev->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);
    filterDev->Characteristics = targetDev->Characteristics;

    lowerDev = IoAttachDeviceToDeviceStack(filterDev, targetDev);
    if (!lowerDev) {
        IoDeleteDevice(filterDev);
        return STATUS_UNSUCCESSFUL;
    }

    PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)filterDev->DeviceExtension;
    memset(ext, 0, sizeof(*ext));
    ext->LowerDevice = lowerDev;
    ext->Active = TRUE;
    SpGenerateSerial(ext->FakeSerial, FAKE_SERIAL_LEN);

    filterDev->Flags &= ~DO_DEVICE_INITIALIZING;
    g_DiskFilters[g_DiskFilterCount++] = filterDev;

    return STATUS_SUCCESS;
}

// Initialize disk spoofing
NTSTATUS SpDiskInitialize(VOID)
{
    PDRIVER_OBJECT ourDriver = NULL;
    UNICODE_STRING ourDriverName = RTL_CONSTANT_STRING(L"\\Driver\\Ph3ntomSpoof");
    NTSTATUS status;

    status = ObReferenceObjectByName(&ourDriverName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&ourDriver);
    if (!NT_SUCCESS(status)) return status;

    ourDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DiskFilterDispatch;
    ourDriver->MajorFunction[IRP_MJ_READ] = DiskFilterPass;
    ourDriver->MajorFunction[IRP_MJ_WRITE] = DiskFilterPass;
    ourDriver->MajorFunction[IRP_MJ_PNP] = DiskFilterPnp;
    ourDriver->MajorFunction[IRP_MJ_POWER] = DiskFilterPass;
    ourDriver->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = DiskFilterPass;

    ObDereferenceObject(ourDriver);

    for (ULONG i = 0; i < 4; i++)
    {
        WCHAR pathBuf[64];
        swprintf(pathBuf, ARRAYSIZE(pathBuf), L"\\Device\\Harddisk%u\\DR%u", i, i);
        AttachToDisk(pathBuf, ourDriver);
    }

    return (g_DiskFilterCount > 0) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_CONNECTED;
}

VOID SpDiskCleanup(VOID)
{
    for (ULONG i = 0; i < g_DiskFilterCount; i++)
    {
        PDEVICE_OBJECT filterDev = g_DiskFilters[i];
        if (!filterDev) continue;

        PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)filterDev->DeviceExtension;
        if (ext && ext->LowerDevice) {
            IoDetachDevice(ext->LowerDevice);
        }
        IoDeleteDevice(filterDev);
        g_DiskFilters[i] = NULL;
    }
    g_DiskFilterCount = 0;
}
