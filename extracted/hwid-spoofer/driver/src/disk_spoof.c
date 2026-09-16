#include "../include/spoofer.h"

//
// Prototypes internes
//
static NTSTATUS DiskFilterDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS DiskFilterPass(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS DiskQueryCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
static NTSTATUS AttachToDisk(PCWSTR DiskPath, PDRIVER_OBJECT DriverObject);


//
// Patch le STORAGE_DEVICE_DESCRIPTOR retourné après completion
//
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
        PSTORAGE_DEVICE_DESCRIPTOR desc =
            (PSTORAGE_DEVICE_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;

        if (desc && desc->Size >= sizeof(STORAGE_DEVICE_DESCRIPTOR) &&
            desc->SerialNumberOffset != 0 &&
            desc->SerialNumberOffset < desc->Size)
        {
            PCHAR pSerial = (PCHAR)desc + desc->SerialNumberOffset;

            // Sauvegarder le serial original si pas encore fait
            if (ext->OrigSerial[0] == '\0') {
                strncpy(ext->OrigSerial, pSerial, sizeof(ext->OrigSerial));
            }

            // Générer un fake serial si pas encore fait pour ce filtre
            if (ext->FakeSerial[0] == '\0') {
                SpGenerateSerial(ext->FakeSerial, FAKE_SERIAL_LEN);
            }

            // Overwrite le serial dans le buffer de sortie
            SIZE_T fakeLen  = strlen(ext->FakeSerial);
            SIZE_T origLen  = strlen(pSerial);
            SIZE_T copyLen  = min(fakeLen, origLen); // on reste dans la taille originale

            memset(pSerial, 0, origLen + 1);
            memcpy(pSerial, ext->FakeSerial, copyLen);
        }
    }

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }
    return STATUS_CONTINUE_COMPLETION;
}


//
// Dispatch principal du filter device : IRP_MJ_DEVICE_CONTROL
//
static NTSTATUS
DiskFilterDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDISK_FILTER_EXTENSION  ext     = (PDISK_FILTER_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION      stack   = IoGetCurrentIrpStackLocation(Irp);

    // On intercepte uniquement IOCTL_STORAGE_QUERY_PROPERTY/StorageDeviceProperty
    if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
        stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_STORAGE_QUERY_PROPERTY &&
        stack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(STORAGE_PROPERTY_QUERY))
    {
        PSTORAGE_PROPERTY_QUERY query =
            (PSTORAGE_PROPERTY_QUERY)stack->Parameters.DeviceIoControl.Type3InputBuffer;

        // Pour METHOD_BUFFERED, l'input est dans AssociatedIrp.SystemBuffer
        if (query == NULL)
            query = (PSTORAGE_PROPERTY_QUERY)Irp->AssociatedIrp.SystemBuffer;

        if (query && query->PropertyId == StorageDeviceProperty)
        {
            // Copier la stack location vers le next driver (le disk FDO en dessous)
            IoCopyCurrentIrpStackLocationToNext(Irp);

            // Poser une completion routine pour modifier la réponse
            IoSetCompletionRoutine(
                Irp,
                DiskQueryCompletion,
                ext,        // contexte = notre extension
                TRUE,       // sur succès
                TRUE,       // sur erreur
                TRUE        // sur cancel
            );

            return IoCallDriver(ext->LowerDevice, Irp);
        }
    }

    // Tout le reste : passer directement sans completion
    return DiskFilterPass(DeviceObject, Irp);
}


//
// Passe-plat pour tous les IRPs non interceptés
//
static NTSTATUS
DiskFilterPass(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)DeviceObject->DeviceExtension;
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->LowerDevice, Irp);
}


//
// PnP pass-through
//
static NTSTATUS
DiskFilterPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    if (stack->MinorFunction == IRP_MN_REMOVE_DEVICE)
    {
        // Le device part : on détache notre filtre
        IoSkipCurrentIrpStackLocation(Irp);
        NTSTATUS status = IoCallDriver(ext->LowerDevice, Irp);
        IoDetachDevice(ext->LowerDevice);
        IoDeleteDevice(DeviceObject);
        return status;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->LowerDevice, Irp);
}


//
// Attache un filter device au-dessus du disk device identifié par DiskPath
// ex : L"/Device/Harddisk0/DR0"
//
static NTSTATUS
AttachToDisk(PCWSTR DiskPath, PDRIVER_OBJECT DriverObject)
{
    NTSTATUS        status;
    UNICODE_STRING  uPath;
    PFILE_OBJECT    fileObj     = NULL;
    PDEVICE_OBJECT  targetDev   = NULL;
    PDEVICE_OBJECT  filterDev   = NULL;
    PDEVICE_OBJECT  lowerDev    = NULL;

    RtlInitUnicodeString(&uPath, DiskPath);

    // Obtenir le device object du disk
    status = IoGetDeviceObjectPointer(&uPath, FILE_READ_DATA, &fileObj, &targetDev);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[spoof] IoGetDeviceObjectPointer(%wZ) failed: 0x%X/n", &uPath, status);
        return status;
    }
    ObDereferenceObject(fileObj);

    if (g_DiskFilterCount >= MAX_DISK_HOOKS) {
        DbgPrint("[spoof] max disk hooks reached/n");
        return STATUS_TOO_MANY_COMMANDS;
    }

    // Créer notre filter device avec une extension
    status = IoCreateDevice(
        DriverObject,
        sizeof(DISK_FILTER_EXTENSION),
        NULL,           // pas de nom (filter)
        targetDev->DeviceType,
        0,
        FALSE,
        &filterDev
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("[spoof] IoCreateDevice filter failed: 0x%X/n", status);
        return status;
    }

    // Recopier les flags importants
    filterDev->Flags |= targetDev->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);
    filterDev->Characteristics = targetDev->Characteristics;

    // Attacher au-dessus du target
    lowerDev = IoAttachDeviceToDeviceStack(filterDev, targetDev);
    if (!lowerDev) {
        DbgPrint("[spoof] IoAttachDeviceToDeviceStack failed/n");
        IoDeleteDevice(filterDev);
        return STATUS_UNSUCCESSFUL;
    }

    // Initialiser l'extension
    PDISK_FILTER_EXTENSION ext = (PDISK_FILTER_EXTENSION)filterDev->DeviceExtension;
    memset(ext, 0, sizeof(*ext));
    ext->LowerDevice = lowerDev;
    ext->Active      = TRUE;
    SpGenerateSerial(ext->FakeSerial, FAKE_SERIAL_LEN);

    filterDev->Flags &= ~DO_DEVICE_INITIALIZING;

    // Enregistrer pour cleanup
    g_DiskFilters[g_DiskFilterCount++] = filterDev;

    DbgPrint("[spoof] attached to %wZ | fake serial: %s/n", &uPath, ext->FakeSerial);
    return STATUS_SUCCESS;
}


//
// Initialisation : parcourt les disks connus et attache des filtres
//
NTSTATUS SpDiskInitialize(VOID)
{
    // On a besoin du DriverObject pour créer des devices —
    // récupéré via le driver /Driver/Spoofer lui-même.
    // On obtient le DriverObject de /Driver/Disk pour la référence de type,
    // mais on crée nos devices sous notre propre DriverObject.

    UNICODE_STRING  ourDriverName = RTL_CONSTANT_STRING(L"/Driver/Spoofer");
    PDRIVER_OBJECT  ourDriver     = NULL;
    NTSTATUS        status;

    // Obtenir notre propre DriverObject
    status = ObReferenceObjectByName(
        &ourDriverName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        0,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        (PVOID*)&ourDriver
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("[spoof] ObReferenceObjectByName(our driver) failed: 0x%X/n", status);
        return status;
    }

    // Hooker les dispatch routines de notre DriverObject pour les filter devices
    ourDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL]   = DiskFilterDispatch;
    ourDriver->MajorFunction[IRP_MJ_READ]              = DiskFilterPass;
    ourDriver->MajorFunction[IRP_MJ_WRITE]             = DiskFilterPass;
    ourDriver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = DiskFilterPass;
    ourDriver->MajorFunction[IRP_MJ_SCSI]             = DiskFilterPass;
    ourDriver->MajorFunction[IRP_MJ_PNP]              = DiskFilterPnp;
    ourDriver->MajorFunction[IRP_MJ_POWER]            = DiskFilterPass;
    ourDriver->MajorFunction[IRP_MJ_FLUSH_BUFFERS]    = DiskFilterPass;

    ObDereferenceObject(ourDriver);

    // Tenter d'attacher sur les premiers 4 disks physiques
    WCHAR pathBuf[64];
    for (ULONG i = 0; i < 4; i++)
    {
        swprintf(pathBuf, sizeof(pathBuf), L"/Device/Harddisk%u/DR%u", i, i);
        NTSTATUS s = AttachToDisk(pathBuf, ourDriver);
        if (!NT_SUCCESS(s) && s != STATUS_OBJECT_NAME_NOT_FOUND)
            DbgPrint("[spoof] disk%u attach: 0x%X/n", i, s);
    }

    return (g_DiskFilterCount > 0) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_CONNECTED;
}


//
// Cleanup : détacher tous les filtres disk
//
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
    DbgPrint("[spoof] disk filters cleaned/n");
}
