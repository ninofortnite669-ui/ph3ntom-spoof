#include "../include/spoofer.h"

// USB Device Spoofing - Intercepts USB device enumeration

// USB Device Structures
#define USB_CONFIGURATION_DESCRIPTOR_TYPE  0x02
#define USB_STRING_DESCRIPTOR_TYPE         0x03

// USB Descriptor Types
#pragma pack(push, 1)
typedef struct _USB_DEVICE_DESCRIPTOR {
    UCHAR bLength;
    UCHAR bDescriptorType;
    USHORT bcdUSB;
    UCHAR bDeviceClass;
    UCHAR bDeviceSubClass;
    UCHAR bDeviceProtocol;
    UCHAR bMaxPacketSize0;
    USHORT idVendor;
    USHORT idProduct;
    USHORT bcdDevice;
    UCHAR iManufacturer;
    UCHAR iProduct;
    UCHAR iSerialNumber;
    UCHAR bNumConfigurations;
} USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;

typedef struct _USB_STRING_DESCRIPTOR {
    UCHAR bLength;
    UCHAR bDescriptorType;
    WCHAR bString[1];
} USB_STRING_DESCRIPTOR, *PUSB_STRING_DESCRIPTOR;
#pragma pack(pop)

// USB Hook Structures
#define MAX_USB_HOOKS 16

typedef struct _USB_HOOK {
    PDRIVER_DISPATCH OrigDispatch;
    BOOLEAN Active;
    USHORT FakeVendorId;
    USHORT FakeProductId;
    WCHAR FakeSerial[64];
} USB_HOOK, *PUSB_HOOK;

static USB_HOOK g_UsbHooks[MAX_USB_HOOKS];
static ULONG g_UsbHookCount = 0;

// Fake USB Information
static const USHORT g_FakeVendorId = 0x046D;  // Logitech
static const USHORT g_FakeProductId = 0xC52B; // USB Receiver

// Generate fake USB serial
static VOID GenerateUsbSerial(PWCHAR Buffer, ULONG Length)
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

// USB Device Control Hook
static NTSTATUS UsbDeviceControlHook(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    
    if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
        stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_USB_GET_DESCRIPTOR &&
        g_SpoofActive)
    {
        PUSB_DEVICE_DESCRIPTOR desc = (PUSB_DEVICE_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;
        if (desc) {
            // Spoof Vendor and Product IDs
            desc->idVendor = g_FakeVendorId;
            desc->idProduct = g_FakeProductId;
            
            // Generate fake serial number string
            static WCHAR fakeSerial[32];
            if (fakeSerial[0] == 0) {
                GenerateUsbSerial(fakeSerial, 16);
            }
            desc->iSerialNumber = 0x01; // Point to first string descriptor
        }
    }
    
    // Call original handler
    for (ULONG i = 0; i < g_UsbHookCount; i++) {
        if (g_UsbHooks[i].OrigDispatch) {
            return g_UsbHooks[i].OrigDispatch(DeviceObject, Irp);
        }
    }
    
    return STATUS_UNSUCCESSFUL;
}

// Initialize USB Spoofing
NTSTATUS SpUsbInitialize(VOID)
{
    NTSTATUS status;
    UNICODE_STRING usbHubName = RTL_CONSTANT_STRING(L"\\Driver\\usbhub");
    PDRIVER_OBJECT usbHubDriver = NULL;

    status = ObReferenceObjectByName(&usbHubName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&usbHubDriver);
    
    if (NT_SUCCESS(status) && usbHubDriver) {
        // Save original dispatch
        if (g_UsbHookCount < MAX_USB_HOOKS) {
            g_UsbHooks[g_UsbHookCount].OrigDispatch = usbHubDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL];
            g_UsbHooks[g_UsbHookCount].Active = TRUE;
            g_UsbHooks[g_UsbHookCount].FakeVendorId = g_FakeVendorId;
            g_UsbHooks[g_UsbHookCount].FakeProductId = g_FakeProductId;
            GenerateUsbSerial(g_UsbHooks[g_UsbHookCount].FakeSerial, 16);
            g_UsbHookCount++;
            
            // Hook the dispatch
            usbHubDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = UsbDeviceControlHook;
        }
        ObDereferenceObject(usbHubDriver);
    }
    
    DbgPrint("[spoof] USB spoofing initialized, %u hooks installed\n", g_UsbHookCount);
    return status;
}

// Cleanup USB Spoofing
VOID SpUsbCleanup(VOID)
{
    UNICODE_STRING usbHubName = RTL_CONSTANT_STRING(L"\\Driver\\usbhub");
    PDRIVER_OBJECT usbHubDriver = NULL;

    NTSTATUS status = ObReferenceObjectByName(&usbHubName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&usbHubDriver);
    
    if (NT_SUCCESS(status) && usbHubDriver) {
        // Restore original dispatch
        for (ULONG i = 0; i < g_UsbHookCount; i++) {
            if (g_UsbHooks[i].OrigDispatch) {
                usbHubDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = g_UsbHooks[i].OrigDispatch;
            }
        }
        ObDereferenceObject(usbHubDriver);
    }
    
    memset(g_UsbHooks, 0, sizeof(g_UsbHooks));
    g_UsbHookCount = 0;
    DbgPrint("[spoof] USB spoofing cleaned\n");
}
