#include "../include/spoofer.h"

// SMBIOS Entry Point Structures
#pragma pack(1)
typedef struct _SMBIOS_ENTRY_POINT {
    CHAR    AnchorString[4];
    UCHAR   Checksum;
    UCHAR   EntryPointLength;
    UCHAR   MajorVersion;
    UCHAR   MinorVersion;
    USHORT  MaxStructureSize;
    UCHAR   EntryPointRevision;
    UCHAR   FormattedArea[5];
    CHAR    AnchorString2[5];
    UCHAR   IntermediateChecksum;
    USHORT  StructureTableLength;
    ULONG   StructureTableAddress;
    USHORT  NumberOfStructures;
    UCHAR   BCDRevision;
} SMBIOS_ENTRY_POINT, *PSMBIOS_ENTRY_POINT;

typedef struct _SMBIOS3_ENTRY_POINT {
    CHAR    AnchorString[5];
    UCHAR   Checksum;
    UCHAR   EntryPointLength;
    UCHAR   MajorVersion;
    UCHAR   MinorVersion;
    UCHAR   DocRev;
    UCHAR   EntryPointRevision;
    UCHAR   Reserved;
    ULONG   MaxStructureSize;
    ULONGLONG StructureTableAddress;
} SMBIOS3_ENTRY_POINT, *PSMBIOS3_ENTRY_POINT;

typedef struct _SMBIOS_HEADER {
    UCHAR   Type;
    UCHAR   Length;
    USHORT  Handle;
} SMBIOS_HEADER, *PSMBIOS_HEADER;
#pragma pack()

// Type definitions
#define SMBIOS_TYPE_SYSTEM  1
#define SMBIOS_TYPE_BOARD   2
#define SMBIOS_TYPE_CHASSIS 3

// Patch Entry
typedef struct _SMBIOS_PATCH_ENTRY {
    PVOID   KernelVA;
    UCHAR   Original[64];
    UCHAR   Patched[64];
    ULONG   Length;
    BOOLEAN Applied;
} SMBIOS_PATCH_ENTRY, *PSMBIOS_PATCH_ENTRY;

#define MAX_SMBIOS_PATCHES 16

static SMBIOS_PATCH_ENTRY  g_Patches[MAX_SMBIOS_PATCHES];
static ULONG               g_PatchCount = 0;
static PVOID               g_MappedTable = NULL;
static ULONG               g_TableSize = 0;
static PHYSICAL_ADDRESS    g_TablePhysAddr = { 0 };

// Get SMBIOS String
static PCHAR GetSmbiosString(PSMBIOS_HEADER header, UCHAR index)
{
    if (index == 0) return NULL;
    PCHAR p = (PCHAR)header + header->Length;
    UCHAR cur = 1;
    while (*p || *(p + 1)) {
        if (cur == index) return p;
        p += strlen(p) + 1;
        cur++;
    }
    return NULL;
}

// Patch SMBIOS String
static VOID PatchSmbiosString(PSMBIOS_HEADER header, UCHAR strIndex, PCHAR fakeBuf, ULONG fakeLen)
{
    PCHAR target = GetSmbiosString(header, strIndex);
    if (!target) return;

    SIZE_T origLen = strlen(target);
    if (origLen == 0) return;

    if (g_PatchCount >= MAX_SMBIOS_PATCHES) return;

    PSMBIOS_PATCH_ENTRY patch = &g_Patches[g_PatchCount++];
    patch->KernelVA = target;
    patch->Length = (ULONG)min(origLen, sizeof(patch->Original) - 1);

    memcpy(patch->Original, target, patch->Length);
    patch->Original[patch->Length] = '\0';

    ULONG copyLen = (ULONG)min(fakeLen, origLen);
    memset(target, 0, origLen);
    memcpy(target, fakeBuf, copyLen);
    memcpy(patch->Patched, target, patch->Length);
    patch->Patched[patch->Length] = '\0';

    DbgPrint("[spoof] SMBIOS patch: '%s' -> '%s'\n", patch->Original, target);
}

// Walk and Patch Table
static VOID WalkAndPatchTable(PVOID tableVA, ULONG tableSize)
{
    PUCHAR p = (PUCHAR)tableVA;
    PUCHAR end = p + tableSize;

    CHAR serial1[FAKE_SERIAL_LEN + 1]; SpGenerateSerial(serial1, FAKE_SERIAL_LEN);
    CHAR serial2[FAKE_SERIAL_LEN + 1]; SpGenerateSerial(serial2, FAKE_SERIAL_LEN);
    CHAR serial3[FAKE_SERIAL_LEN + 1]; SpGenerateSerial(serial3, FAKE_SERIAL_LEN);
    CHAR uuid[37]; SpGenerateUUID(uuid);

    while (p < end - sizeof(SMBIOS_HEADER))
    {
        PSMBIOS_HEADER hdr = (PSMBIOS_HEADER)p;
        if (hdr->Type == 127) break;
        if (hdr->Length < sizeof(SMBIOS_HEADER)) break;

        switch (hdr->Type)
        {
        case SMBIOS_TYPE_SYSTEM:
            if (hdr->Length >= 0x18)
            {
                PUCHAR fields = (PUCHAR)hdr;
                UCHAR serialIdx = fields[0x07];
                PatchSmbiosString(hdr, serialIdx, serial1, FAKE_SERIAL_LEN);

                PUCHAR uuidField = fields + 8;
                if (g_PatchCount < MAX_SMBIOS_PATCHES) {
                    PSMBIOS_PATCH_ENTRY pe = &g_Patches[g_PatchCount++];
                    pe->KernelVA = uuidField;
                    pe->Length = 16;
                    memcpy(pe->Original, uuidField, 16);
                    for (int i = 0; i < 16; i++) {
                        uuidField[i] = (UCHAR)(serial1[i % FAKE_SERIAL_LEN] ^ i);
                    }
                    memcpy(pe->Patched, uuidField, 16);
                }
            }
            break;

        case SMBIOS_TYPE_BOARD:
            if (hdr->Length >= 0x08) {
                PUCHAR fields = (PUCHAR)hdr;
                UCHAR sIdx = fields[0x07];
                PatchSmbiosString(hdr, sIdx, serial2, FAKE_SERIAL_LEN);
            }
            break;

        case SMBIOS_TYPE_CHASSIS:
            if (hdr->Length >= 0x08) {
                PUCHAR fields = (PUCHAR)hdr;
                UCHAR sIdx = fields[0x07];
                PatchSmbiosString(hdr, sIdx, serial3, FAKE_SERIAL_LEN);
            }
            break;
        }

        PUCHAR strSection = p + hdr->Length;
        while (strSection < end - 1 && (*strSection || *(strSection + 1)))
            strSection++;
        p = strSection + 2;
    }
}

// Initialize SMBIOS Spoofing
NTSTATUS SpSmbiosInitialize(VOID)
{
    PHYSICAL_ADDRESS scanStart;
    scanStart.QuadPart = 0xF0000ULL;
    ULONG scanSize = 0x10000;

    PVOID scanVA = MmMapIoSpace(scanStart, scanSize, MmNonCached);
    if (!scanVA) {
        DbgPrint("[spoof] MmMapIoSpace scan area failed\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    PSMBIOS_ENTRY_POINT ep2 = NULL;
    PSMBIOS3_ENTRY_POINT ep3 = NULL;

    for (ULONG i = 0; i < scanSize - 5; i += 16)
    {
        PUCHAR p = (PUCHAR)scanVA + i;
        if (!ep3 && p[0] == '_' && p[1] == 'S' && p[2] == 'M' && p[3] == '3' && p[4] == '_') {
            ep3 = (PSMBIOS3_ENTRY_POINT)p;
        }
        if (!ep2 && p[0] == '_' && p[1] == 'S' && p[2] == 'M' && p[3] == '_') {
            ep2 = (PSMBIOS_ENTRY_POINT)p;
        }
        if (ep2 || ep3) break;
    }

    PHYSICAL_ADDRESS tablePhys = { 0 };
    ULONG tableLen = 0;

    if (ep3) {
        tablePhys.QuadPart = (LONGLONG)ep3->StructureTableAddress;
        tableLen = ep3->MaxStructureSize;
    } else if (ep2) {
        tablePhys.LowPart = ep2->StructureTableAddress;
        tablePhys.HighPart = 0;
        tableLen = ep2->StructureTableLength;
    }

    MmUnmapIoSpace(scanVA, scanSize);

    if (tableLen == 0 || tablePhys.QuadPart == 0) {
        DbgPrint("[spoof] SMBIOS table not found\n");
        return STATUS_NOT_FOUND;
    }

    PVOID tableVA = MmMapIoSpace(tablePhys, tableLen, MmNonCached);
    if (!tableVA) {
        DbgPrint("[spoof] MmMapIoSpace SMBIOS table failed\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    g_MappedTable = tableVA;
    g_TableSize = tableLen;
    g_TablePhysAddr = tablePhys;
    g_PatchCount = 0;

    WalkAndPatchTable(tableVA, tableLen);
    DbgPrint("[spoof] SMBIOS initialized, %u patches applied\n", g_PatchCount);
    return STATUS_SUCCESS;
}

VOID SpSmbiosRestore(VOID)
{
    if (!g_MappedTable) return;

    for (ULONG i = 0; i < g_PatchCount; i++) {
        PSMBIOS_PATCH_ENTRY pe = &g_Patches[i];
        if (pe->KernelVA && pe->Length > 0) {
            memcpy(pe->KernelVA, pe->Original, pe->Length);
        }
    }
    g_PatchCount = 0;

    MmUnmapIoSpace(g_MappedTable, g_TableSize);
    g_MappedTable = NULL;
    g_TableSize = 0;

    DbgPrint("[spoof] SMBIOS restored\n");
}
