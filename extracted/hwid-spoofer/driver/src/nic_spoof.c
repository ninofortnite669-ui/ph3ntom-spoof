#include "../include/spoofer.h"

//
// OID constants NDIS (définis normalement dans ndis.h — pas dispo en WDK simple)
//
#define OID_802_3_CURRENT_ADDRESS       0x01010102
#define OID_802_3_PERMANENT_ADDRESS     0x01010101
#define OID_GEN_CURRENT_PACKET_FILTER   0x0001010E

// IOCTL interne NDIS pour les requêtes OID


#define IOCTL_NDIS_QUERY_GLOBAL_STATS    0x0019C000
//
// Sauvegarde du dispatch original de /Driver/ndis
//
extern PDRIVER_DISPATCH g_OrigNdisDispatch;
extern PDRIVER_OBJECT   g_NdisDriverObject;

//
// Hooked IRP_MJ_DEVICE_CONTROL sur le driver NDIS
// Intercepte IOCTL_NDIS_QUERY_GLOBAL_STATS pour OID_802_3_CURRENT_ADDRESS
//
static NTSTATUS
NicDispatchHook(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    // On vérifie que c'est bien une query OID
    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_NDIS_QUERY_GLOBAL_STATS &&
        stack->Parameters.DeviceIoControl.InputBufferLength == sizeof(ULONG) &&
        stack->Parameters.DeviceIoControl.OutputBufferLength >= 6 &&
        Irp->AssociatedIrp.SystemBuffer)
    {
        ULONG oid = *(PULONG)Irp->AssociatedIrp.SystemBuffer;

        if ((oid == OID_802_3_CURRENT_ADDRESS || oid == OID_802_3_PERMANENT_ADDRESS)
            && g_SpoofActive)
        {
            // Compléter l'IRP nous-mêmes avec le fake MAC
            memcpy(Irp->AssociatedIrp.SystemBuffer, g_FakeMac, 6);
            Irp->IoStatus.Status      = STATUS_SUCCESS;
            Irp->IoStatus.Information = 6;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }
    }

    // Passer au dispatch original
    return g_OrigNdisDispatch(DeviceObject, Irp);
}


//
// SpNicInitialize : hook le dispatch de /Driver/ndis
//
NTSTATUS SpNicInitialize(VOID)
{
    NTSTATUS        status;
    UNICODE_STRING  ndisName = RTL_CONSTANT_STRING(L"/Driver/ndis");

    status = ObReferenceObjectByName(
        &ndisName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        0,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        (PVOID*)&g_NdisDriverObject
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("[spoof] ObReferenceObjectByName(ndis) failed: 0x%X/n", status);
        return status;
    }

    // Sauvegarder et remplacer le dispatch DEVICE_CONTROL
    g_OrigNdisDispatch = g_NdisDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    InterlockedExchangePointer(
        (PVOID*)&g_NdisDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL],
        (PVOID)NicDispatchHook
    );

    ObDereferenceObject(g_NdisDriverObject);

    DbgPrint("[spoof] NIC dispatch hooked | fake MAC: %02X:%02X:%02X:%02X:%02X:%02X/n",
        g_FakeMac[0], g_FakeMac[1], g_FakeMac[2],
        g_FakeMac[3], g_FakeMac[4], g_FakeMac[5]);

    return STATUS_SUCCESS;
}


//
// SpNicCleanup : restaure le dispatch original
//
VOID SpNicCleanup(VOID)
{
    if (!g_NdisDriverObject || !g_OrigNdisDispatch) return;

    // Re-référencer pour restaurer proprement
    UNICODE_STRING ndisName = RTL_CONSTANT_STRING(L"/Driver/ndis");
    PDRIVER_OBJECT drvObj   = NULL;

    NTSTATUS status = ObReferenceObjectByName(
        &ndisName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, 0,
        *IoDriverObjectType,
        KernelMode, NULL,
        (PVOID*)&drvObj
    );

    if (NT_SUCCESS(status) && drvObj) {
        InterlockedExchangePointer(
            (PVOID*)&drvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL],
            (PVOID)g_OrigNdisDispatch
        );
        ObDereferenceObject(drvObj);
    }

    g_OrigNdisDispatch  = NULL;
    g_NdisDriverObject  = NULL;
    DbgPrint("[spoof] NIC dispatch restored/n");
}
