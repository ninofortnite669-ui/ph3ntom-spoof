#include "../include/spoofer.h"

// OID constants
define OID_802_3_CURRENT_ADDRESS       0x01010102
define OID_802_3_PERMANENT_ADDRESS     0x01010101

// NDIS Driver Hook
PDRIVER_DISPATCH g_OrigNdisDispatch   = NULL;
PDRIVER_OBJECT   g_NdisDriverObject   = NULL;

// Hooked dispatch
static NTSTATUS NicDispatchHook(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    if (stack->Parameters.DeviceIoControl.IoControlCode == 0x0019C000 &&
        stack->Parameters.DeviceIoControl.InputBufferLength == sizeof(ULONG) &&
        stack->Parameters.DeviceIoControl.OutputBufferLength >= 6 &&
        Irp->AssociatedIrp.SystemBuffer && g_SpoofActive)
    {
        ULONG oid = *(PULONG)Irp->AssociatedIrp.SystemBuffer;
        if (oid == OID_802_3_CURRENT_ADDRESS || oid == OID_802_3_PERMANENT_ADDRESS)
        {
            static UCHAR fakeMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
            if (fakeMac[1] == 0) {
                SpGenerateMac(fakeMac);
            }
            memcpy(Irp->AssociatedIrp.SystemBuffer, fakeMac, 6);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 6;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }
    }

    return g_OrigNdisDispatch ? g_OrigNdisDispatch(DeviceObject, Irp) : STATUS_UNSUCCESSFUL;
}

NTSTATUS SpNicInitialize(VOID)
{
    NTSTATUS status;
    UNICODE_STRING ndisName = RTL_CONSTANT_STRING(L"\\Driver\\ndis");

    status = ObReferenceObjectByName(&ndisName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&g_NdisDriverObject);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[spoof] ObReferenceObjectByName(ndis) failed: 0x%X\n", status);
        return status;
    }

    g_OrigNdisDispatch = g_NdisDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    InterlockedExchangePointer((PVOID*)&g_NdisDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL],
        (PVOID)NicDispatchHook);

    ObDereferenceObject(g_NdisDriverObject);
    DbgPrint("[spoof] NIC dispatch hooked\n");
    return STATUS_SUCCESS;
}

VOID SpNicCleanup(VOID)
{
    if (!g_NdisDriverObject || !g_OrigNdisDispatch) return;

    UNICODE_STRING ndisName = RTL_CONSTANT_STRING(L"\\Driver\\ndis");
    PDRIVER_OBJECT drvObj = NULL;

    NTSTATUS status = ObReferenceObjectByName(&ndisName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&drvObj);

    if (NT_SUCCESS(status) && drvObj) {
        InterlockedExchangePointer((PVOID*)&drvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL],
            (PVOID)g_OrigNdisDispatch);
        ObDereferenceObject(drvObj);
    }

    g_OrigNdisDispatch = NULL;
    g_NdisDriverObject = NULL;
    DbgPrint("[spoof] NIC dispatch restored\n");
}
