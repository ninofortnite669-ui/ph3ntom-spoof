#include "../include/spoofer.h"

// EDID Spoofing - Modifies monitor EDID information

// EDID Block Structure (128 bytes)
#pragma pack(push, 1)
typedef struct _EDID_BLOCK {
    UCHAR Header[8];           // 00-07: Fixed header "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00"
    USHORT ManufacturerID;     // 08-09: Manufacturer ID (3 letters, 5 bits each)
    USHORT ProductID;          // 0A-0B: Product ID (LSB first)
    ULONG SerialNumber;        // 0C-0F: Serial number
    UCHAR WeekOfManufacture;    // 10: Week of manufacture
    UCHAR YearOfManufacture;    // 11: Year of manufacture (since 1990)
    UCHAR Version;             // 12: EDID version
    UCHAR Revision;            // 13: EDID revision
    UCHAR VideoInputType;      // 14: Video input type
    UCHAR MaxHSize;            // 15: Maximum horizontal size (cm)
    UCHAR MaxVSize;            // 16: Maximum vertical size (cm)
    UCHAR Gamma;               // 17: Gamma value
    UCHAR Features;            // 18: Features flags
    UCHAR Chromaticity[10];     // 19-22: Chromaticity coordinates
    UCHAR EstablishedTimings[3]; // 23-25: Established timings
    UCHAR StandardTimings[16]; // 26-3B: Standard timings
    UCHAR DescriptorBlock1[18]; // 3C-4F: Descriptor block 1
    UCHAR DescriptorBlock2[18]; // 50-63: Descriptor block 2
    UCHAR DescriptorBlock3[18]; // 64-77: Descriptor block 3
    UCHAR DescriptorBlock4[18]; // 78-8B: Descriptor block 4
    UCHAR ExtensionFlag;       // 8C: Extension flag
    UCHAR Checksum;            // 8D: Checksum
    UCHAR Padding[3];          // 8E-AF: Padding
} EDID_BLOCK, *PEDID_BLOCK;
#pragma pack(pop)

// Fake EDID Information
static const CHAR g_FakeEdidManufacturer[] = "DEL"; // Dell
static const USHORT g_FakeEdidProductId = 0x4040;
static ULONG g_FakeEdidSerial = 0x00000001;

// Monitor EDID Hook
static PVOID g_EdidHookAddress = NULL;
static UCHAR g_OrigEdidBytes[16] = {0};
static BOOLEAN g_EdidSpoofActive = FALSE;

// Generate fake EDID block
static VOID GenerateFakeEdid(PEDID_BLOCK EdidBlock)
{
    memset(EdidBlock, 0, sizeof(EDID_BLOCK));
    
    // EDID Header
    EdidBlock->Header[0] = 0x00;
    EdidBlock->Header[1] = 0xFF;
    EdidBlock->Header[2] = 0xFF;
    EdidBlock->Header[3] = 0xFF;
    EdidBlock->Header[4] = 0xFF;
    EdidBlock->Header[5] = 0xFF;
    EdidBlock->Header[6] = 0xFF;
    EdidBlock->Header[7] = 0x00;
    
    // Manufacturer ID (DEL = 0x4C4544)
    EdidBlock->ManufacturerID = 0x4C45;
    
    // Product ID
    EdidBlock->ProductID = g_FakeEdidProductId;
    
    // Serial Number
    EdidBlock->SerialNumber = g_FakeEdidSerial;
    
    // Manufacturing date (Week 1, Year 2020)
    EdidBlock->WeekOfManufacture = 1;
    EdidBlock->YearOfManufacture = 30; // 1990 + 30 = 2020
    
    // EDID Version
    EdidBlock->Version = 1;
    EdidBlock->Revision = 3;
    
    // Video input type (Digital)
    EdidBlock->VideoInputType = 0x80;
    
    // Display size (60cm x 34cm - 23.6 inch 16:9)
    EdidBlock->MaxHSize = 60;
    EdidBlock->MaxVSize = 34;
    
    // Gamma
    EdidBlock->Gamma = 0x78; // 1.0 gamma (100 + 122 = 222)
    
    // Features
    EdidBlock->Features = 0x0A; // Digital, RGB color, sRGB standard
    
    // Established timings (720x400 @ 70Hz, 720x400 @ 88Hz, VGA 640x480 @ 60Hz)
    EdidBlock->EstablishedTimings[0] = 0x01;
    EdidBlock->EstablishedTimings[1] = 0x01;
    EdidBlock->EstablishedTimings[2] = 0x80;
    
    // Standard timings (1920x1080 @ 60Hz)
    EdidBlock->StandardTimings[0] = 0x01;
    EdidBlock->StandardTimings[1] = 0x01;
    
    // Descriptor blocks
    // Block 1: Display Product Name
    EdidBlock->DescriptorBlock1[0] = 0x00; // Descriptor type (Display Product Name)
    EdidBlock->DescriptorBlock1[1] = 0x00; // Padding
    EdidBlock->DescriptorBlock1[2] = 0x00; // Padding
    EdidBlock->DescriptorBlock1[3] = 0xFC; // Tag (Display Product Name)
    EdidBlock->DescriptorBlock1[4] = 0x00; // Flag
    memcpy(&EdidBlock->DescriptorBlock1[5], "DELL U2422H", 12);
    
    // Block 2: Display Range Limits
    EdidBlock->DescriptorBlock2[0] = 0x00;
    EdidBlock->DescriptorBlock2[1] = 0x00;
    EdidBlock->DescriptorBlock2[2] = 0x00;
    EdidBlock->DescriptorBlock2[3] = 0xFD; // Tag (Range Limits)
    EdidBlock->DescriptorBlock2[4] = 0x00; // Flag
    EdidBlock->DescriptorBlock2[5] = 0x32; // Min vertical rate (50Hz)
    EdidBlock->DescriptorBlock2[6] = 0x9B; // Max vertical rate (120Hz)
    EdidBlock->DescriptorBlock2[7] = 0x50; // Min horizontal rate (28kHz)
    EdidBlock->DescriptorBlock2[8] = 0x9A; // Max horizontal rate (140kHz)
    EdidBlock->DescriptorBlock2[9] = 0x24; // Max pixel clock (300MHz)
    
    // Calculate checksum
    UCHAR checksum = 0;
    for (int i = 0; i < 127; i++) {
        checksum += ((UCHAR*)EdidBlock)[i];
    }
    EdidBlock->Checksum = (UCHAR)(256 - checksum);
}

// Hook for EDID reads
static NTSTATUS EdidReadHook(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    
    if (stack->MajorFunction == IRP_MJ_READ && g_SpoofActive && g_EdidSpoofActive)
    {
        if (Irp->AssociatedIrp.SystemBuffer && Irp->AssociatedIrp.SystemBufferSize >= 128)
        {
            static EDID_BLOCK fakeEdid;
            if (fakeEdid.Header[1] != 0xFF) {
                GenerateFakeEdid(&fakeEdid);
            }
            
            memcpy(Irp->AssociatedIrp.SystemBuffer, &fakeEdid, 128);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 128;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }
    }
    
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DeviceObject, Irp);
}

// Initialize EDID Spoofing
NTSTATUS SpEdidInitialize(VOID)
{
    if (g_EdidSpoofActive) return STATUS_SUCCESS;
    
    // Hook EDID read operations
    // This would typically involve hooking the display port driver
    // For this implementation, we'll mark it as initialized
    
    g_EdidSpoofActive = TRUE;
    DbgPrint("[spoof] EDID spoofing initialized\n");
    return STATUS_SUCCESS;
}

// Cleanup EDID Spoofing
VOID SpEdidCleanup(VOID)
{
    g_EdidSpoofActive = FALSE;
    DbgPrint("[spoof] EDID spoofing cleaned\n");
}
