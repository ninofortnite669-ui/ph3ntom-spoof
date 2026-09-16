#include "../include/spoofer.h"

// TPM Device Structures
#define TPM_BASE_NAME L"\\Device\\TPM"
#define TPM_DOS_NAME L"\\DosDevices\\TPM"

// TPM Command Codes for spoofing
#define IOCTL_TPM_SEND_COMMAND 0x222000

// TPM Registry Paths
#define TPM_REGISTRY_PATH L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Cryptography\\Calais\\TPM"

// TPM Spoof Data
static UCHAR g_FakeTpmData[256] = {0};
static BOOLEAN g_TpmInitialized = FALSE;

// Fake TPM Properties
static const CHAR g_FakeTpmManufacturer[] = "INTC";
static const CHAR g_FakeTpmVersion[] = "7.0";
static const CHAR g_FakeTpmDescription[] = "Intel(R) Trusted Platform Module 2.0";

// TPM Capabilities Structure
#pragma pack(push, 1)
typedef struct _FAKE_TPM_CAPS {
    ULONG Version;
    ULONG ManufacturerId;
    ULONG TpmVersion;
    UCHAR ManufacturerString[64];
    UCHAR ProductString[64];
    UCHAR TpmVersionString[16];
    UCHAR DescriptionString[256];
} FAKE_TPM_CAPS, *PFAKE_TPM_CAPS;
#pragma pack(pop)

// Initialize Fake TPM Data
NTSTATUS SpTpmInitialize(VOID)
{
    PFAKE_TPM_CAPS caps = (PFAKE_TPM_CAPS)g_FakeTpmData;
    
    if (g_TpmInitialized) return STATUS_SUCCESS;
    
    // Fill TPM Capabilities
    caps->Version = 0x00020000; // TPM 2.0
    caps->ManufacturerId = 0x494E5443; // "INTC"
    caps->TpmVersion = 0x00070000; // Version 7.0
    
    size_t mfgLen = strlen(g_FakeTpmManufacturer);
    memcpy(caps->ManufacturerString, g_FakeTpmManufacturer, min(mfgLen, sizeof(caps->ManufacturerString) - 1));
    caps->ManufacturerString[sizeof(caps->ManufacturerString) - 1] = '\0';
    
    size_t prodLen = strlen("PTM");
    memcpy(caps->ProductString, "PTM", min(prodLen, sizeof(caps->ProductString) - 1));
    caps->ProductString[sizeof(caps->ProductString) - 1] = '\0';
    
    size_t verLen = strlen(g_FakeTpmVersion);
    memcpy(caps->TpmVersionString, g_FakeTpmVersion, min(verLen, sizeof(caps->TpmVersionString) - 1));
    caps->TpmVersionString[sizeof(caps->TpmVersionString) - 1] = '\0';
    
    size_t descLen = strlen(g_FakeTpmDescription);
    memcpy(caps->DescriptionString, g_FakeTpmDescription, min(descLen, sizeof(caps->DescriptionString) - 1));
    caps->DescriptionString[sizeof(caps->DescriptionString) - 1] = '\0';
    
    g_TpmInitialized = TRUE;
    DbgPrint("[spoof] TPM spoofing initialized\n");
    return STATUS_SUCCESS;
}

// Hook TPM Device Control
NTSTATUS TpmDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctl = stack->Parameters.DeviceIoControl.IoControlCode;
    
    if (ioctl == IOCTL_TPM_SEND_COMMAND && g_SpoofActive && g_TpmSpoofActive)
    {
        // Intercept TPM commands and return fake responses
        PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
        PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
        
        if (inputBuffer && outputBuffer)
        {
            // Return our fake TPM data
            SIZE_T copySize = min(Irp->AssociatedIrp.SystemBufferSize, sizeof(g_FakeTpmData));
            memcpy(outputBuffer, g_FakeTpmData, (ULONG)copySize);
            
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = (ULONG)copySize;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }
    }
    
    // Pass through for other IOCTLs
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DeviceObject, Irp);
}

// Registry Modification for TPM
NTSTATUS ModifyTpmRegistry(VOID)
{
    OBJECT_ATTRIBUTES objAttr;
    HANDLE regHandle;
    UNICODE_STRING regPath;
    NTSTATUS status;
    
    RtlInitUnicodeString(&regPath, TPM_REGISTRY_PATH);
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = ZwCreateKey(&regHandle, KEY_WRITE, &objAttr, 0, NULL, REG_OPTION_NON_VOLATILE, NULL);
    if (NT_SUCCESS(status))
    {
        // Set fake TPM registry values
        UNICODE_STRING valueName;
        UNICODE_STRING valueData;
        WCHAR fakeData[256];
        
        RtlInitUnicodeString(&valueName, L"Manufacturer");
        swprintf(fakeData, ARRAYSIZE(fakeData), L"%S", g_FakeTpmManufacturer);
        RtlInitUnicodeString(&valueData, fakeData);
        ZwSetValueKey(regHandle, &valueName, 0, REG_SZ, valueData.Buffer, (ULONG)valueData.Length + sizeof(WCHAR));
        
        RtlInitUnicodeString(&valueName, L"Version");
        swprintf(fakeData, ARRAYSIZE(fakeData), L"%S", g_FakeTpmVersion);
        RtlInitUnicodeString(&valueData, fakeData);
        ZwSetValueKey(regHandle, &valueName, 0, REG_SZ, valueData.Buffer, (ULONG)valueData.Length + sizeof(WCHAR));
        
        ZwClose(regHandle);
    }
    
    return status;
}

VOID SpTpmCleanup(VOID)
{
    g_TpmInitialized = FALSE;
    memset(g_FakeTpmData, 0, sizeof(g_FakeTpmData));
    DbgPrint("[spoof] TPM spoofing cleaned\n");
}
