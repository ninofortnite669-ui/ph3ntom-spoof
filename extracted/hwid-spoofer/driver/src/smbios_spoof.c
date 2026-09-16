#include "../include/spoofer.h"

//
// SMBIOS 2.x Entry Point Structure (à chercher dans 0xF0000 - 0xFFFFF)
//
#pragma pack(1)
typedef struct _SMBIOS_ENTRY_POINT {
    CHAR    AnchorString[4];        // "_SM_"
    UCHAR   Checksum;
    UCHAR   EntryPointLength;
    UCHAR   MajorVersion;
    UCHAR   MinorVersion;
    USHORT  MaxStructureSize;
    UCHAR   EntryPointRevision;
    UCHAR   FormattedArea[5];
    CHAR    AnchorString2[5];       // "_DMI_"
    UCHAR   IntermediateChecksum;
    USHORT  StructureTableLength;
    ULONG   StructureTableAddress;  // Physical address de la table
    USHORT  NumberOfStructures;
    UCHAR   BCDRevision;
} SMBIOS_ENTRY_POINT, *PSMBIOS_ENTRY_POINT;

// SMBIOS 3.x Entry Point Structure
typedef struct _SMBIOS3_ENTRY_POINT {
    CHAR    AnchorString[5];        // "_SM3_"
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

// Header commun des structures SMBIOS
typedef struct _SMBIOS_HEADER {
    UCHAR   Type;
    UCHAR   Length;
    USHORT  Handle;
} SMBIOS_HEADER, *PSMBIOS_HEADER;

// Type 1 : System Information
#define SMBIOS_TYPE_SYSTEM  1
// Type 2 : Base Board
#define SMBIOS_TYPE_BOARD   2
// Type 3 : System Enclosure/Chassis
#define SMBIOS_TYPE_CHASSIS 3

#pragma pack()

//
// Sauvegarde pour la restauration
//
typedef struct _SMBIOS_PATCH_ENTRY {
    PVOID   KernelVA;       // adresse virtuelle dans la table mappée
    UCHAR   Original[64];
    UCHAR   Patched[64];
    ULONG   Length;
    BOOLEAN Applied;
} SMBIOS_PATCH_ENTRY, *PSMBIOS_PATCH_ENTRY;

#define MAX_SMBIOS_PATCHES 16

static SMBIOS_PATCH_ENTRY  g_Patches[MAX_SMBIOS_PATCHES];
static ULONG               g_PatchCount = 0;
static PVOID               g_MappedTable    = NULL;
static ULONG               g_TableSize      = 0;
static PHYSICAL_ADDRESS    g_TablePhysAddr  = { 0 };

//
// Calcule le checksum d'un buffer SMBIOS
//
static UCHAR CalcChecksum(PUCHAR buf, ULONG len)
{
    UCHAR sum = 0;
    for (ULONG i = 0; i < len; i++) sum += buf[i];
    return (UCHAR)(0x100 - sum);
}

//
// Récupère la n-ième string dans la zone de strings qui suit un header SMBIOS
// (strings séparées par /0, terminées par /0/0)
// Retourne un pointeur vers la string dans le buffer mappé, ou NULL
//
static PCHAR GetSmbiosString(PSMBIOS_HEADER header, UCHAR index)
{
    if (index == 0) return NULL;

    PCHAR p = (PCHAR)header + header->Length;
    UCHAR cur = 1;

    while (*p || *(p + 1))
    {
        if (cur == index) return p;
        p += strlen(p) + 1;
        cur++;
    }
    return NULL;
}

//
// Patch une string SMBIOS en place avec un fake serial
// Garde la même longueur pour ne pas casser les offsets
//
static VOID PatchSmbiosString(PSMBIOS_HEADER header, UCHAR strIndex, PCHAR fakeBuf, ULONG fakeLen)
{
    PCHAR target = GetSmbiosString(header, strIndex);
    if (!target) return;

    SIZE_T origLen = strlen(target);
    if (origLen == 0) return;

    if (g_PatchCount >= MAX_SMBIOS_PATCHES) return;

    PSMBIOS_PATCH_ENTRY patch = &g_Patches[g_PatchCount++];
    patch->KernelVA = target;
    patch->Length   = (ULONG)min(origLen, sizeof(patch->Original) - 1);

    memcpy(patch->Original, target, patch->Length);
    patch->Original[patch->Length] = '\0';

    // Remplir avec le fake, tronqué à la longueur originale
    ULONG copyLen = (ULONG)min(fakeLen, origLen);
    memset(target, 0, origLen);
    memcpy(target, fakeBuf, copyLen);
    memcpy(patch->Patched, target, patch->Length);
    patch->Patched[patch->Length] = '\0';

    DbgPrint("[spoof] SMBIOS patch: '%s' -> '%s'/n", patch->Original, target);
}


//
// Parcourt la table SMBIOS et patche les serials des structures type 1, 2, 3
//
static VOID WalkAndPatchTable(PVOID tableVA, ULONG tableSize)
{
    PUCHAR p    = (PUCHAR)tableVA;
    PUCHAR end  = p + tableSize;

    // Fake serials différents par type
    CHAR serial1[FAKE_SERIAL_LEN + 1]; SpGenerateSerial(serial1, FAKE_SERIAL_LEN);
    CHAR serial2[FAKE_SERIAL_LEN + 1]; SpGenerateSerial(serial2, FAKE_SERIAL_LEN);
    CHAR serial3[FAKE_SERIAL_LEN + 1]; SpGenerateSerial(serial3, FAKE_SERIAL_LEN);
    CHAR uuid[36 + 1];
    // Générer un UUID bidon
    SpGenerateSerial(uuid, 8);  uuid[8] = '-';
    SpGenerateSerial(uuid + 9, 4);  uuid[13] = '-';
    SpGenerateSerial(uuid + 14, 4); uuid[18] = '-';
    SpGenerateSerial(uuid + 19, 4); uuid[23] = '-';
    SpGenerateSerial(uuid + 24, 12); uuid[36] = '\0';

    while (p < end - sizeof(SMBIOS_HEADER))
    {
        PSMBIOS_HEADER hdr = (PSMBIOS_HEADER)p;

        // End-of-table sentinel
        if (hdr->Type == 127) break;
        if (hdr->Length < sizeof(SMBIOS_HEADER)) break;

        switch (hdr->Type)
        {
        case SMBIOS_TYPE_SYSTEM:
        {
            // Type 1 layout (SMBIOS spec):
            // offset 04h = Manufacturer (string index)
            // offset 05h = Product Name
            // offset 06h = Version
            // offset 07h = Serial Number  <- on patch ça
            // offset 08h-17h = UUID       <- et ça
            if (hdr->Length >= 0x18)
            {
                PUCHAR fields = (PUCHAR)hdr;
                UCHAR  serialIdx = fields[0x07];
                PatchSmbiosString(hdr, serialIdx, serial1, FAKE_SERIAL_LEN);

                // UUID est dans les champs fixes (pas une string), offset 8, 16 bytes
                PUCHAR uuidField = fields + 8;
                // Sauvegarder + patch avec bytes random
                if (g_PatchCount < MAX_SMBIOS_PATCHES) {
                    PSMBIOS_PATCH_ENTRY pe = &g_Patches[g_PatchCount++];
                    pe->KernelVA = uuidField;
                    pe->Length   = 16;
                    memcpy(pe->Original, uuidField, 16);
                    for (int i = 0; i < 16; i++) {
                        uuidField[i] = (UCHAR)(serial1[i % FAKE_SERIAL_LEN] ^ i);
                    }
                    memcpy(pe->Patched, uuidField, 16);
                    pe->Patched[16] = '\0';
                }
            }
            break;
        }

        case SMBIOS_TYPE_BOARD:
        {
            // Type 2 : Base Board
            // offset 07h = Serial Number string index
            if (hdr->Length >= 0x08) {
                PUCHAR fields  = (PUCHAR)hdr;
                UCHAR  sIdx    = fields[0x07];
                PatchSmbiosString(hdr, sIdx, serial2, FAKE_SERIAL_LEN);
            }
            break;
        }

        case SMBIOS_TYPE_CHASSIS:
        {
            // Type 3 : Chassis
            // offset 07h = Serial Number string index
            if (hdr->Length >= 0x08) {
                PUCHAR fields  = (PUCHAR)hdr;
                UCHAR  sIdx    = fields[0x07];
                PatchSmbiosString(hdr, sIdx, serial3, FAKE_SERIAL_LEN);
            }
            break;
        }
        }

        // Avancer vers la structure suivante
        // La zone strings suit immédiatement le header formaté
        PUCHAR strSection = p + hdr->Length;
        while (strSection < end - 1 && (*strSection || *(strSection + 1)))
            strSection++;
        p = strSection + 2; // sauter le double /0
    }
}


//
// SpSmbiosInitialize : trouve la table SMBIOS en physical memory, la mappe, la patche
//
NTSTATUS SpSmbiosInitialize(VOID)
{
    // Chercher le entry point dans la zone 0xF0000 - 0xFFFFF (BIOS data area)
    PHYSICAL_ADDRESS scanStart;
    scanStart.QuadPart = 0xF0000ULL;
    ULONG scanSize = 0x10000; // 64 Ko

    PVOID scanVA = MmMapIoSpace(scanStart, scanSize, MmNonCached);
    if (!scanVA) {
        DbgPrint("[spoof] MmMapIoSpace scan area failed/n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    PSMBIOS_ENTRY_POINT ep2  = NULL;
    PSMBIOS3_ENTRY_POINT ep3 = NULL;

    // Chercher "_SM3_" (SMBIOS 3.x) en premier, puis "_SM_" (2.x)
    // Les entry points sont alignés sur 16 bytes
    for (ULONG i = 0; i < scanSize - 5; i += 16)
    {
        PUCHAR p = (PUCHAR)scanVA + i;

        if (!ep3 && p[0] == '_' && p[1] == 'S' && p[2] == 'M' && p[3] == '3' && p[4] == '_') {
            ep3 = (PSMBIOS3_ENTRY_POINT)p;
            DbgPrint("[spoof] SMBIOS 3.x entry point at offset 0x%X/n", i);
        }
        if (!ep2 && p[0] == '_' && p[1] == 'S' && p[2] == 'M' && p[3] == '_') {
            ep2 = (PSMBIOS_ENTRY_POINT)p;
            DbgPrint("[spoof] SMBIOS 2.x entry point at offset 0x%X/n", i);
        }
        if (ep2 || ep3) break;
    }

    PHYSICAL_ADDRESS tablePhys = { 0 };
    ULONG            tableLen  = 0;

    if (ep3) {
        tablePhys.QuadPart = (LONGLONG)ep3->StructureTableAddress;
        tableLen           = ep3->MaxStructureSize;
        DbgPrint("[spoof] SMBIOS 3.x table at phys 0x%llX, size 0x%X/n",
            tablePhys.QuadPart, tableLen);
    } else if (ep2) {
        tablePhys.LowPart  = ep2->StructureTableAddress;
        tablePhys.HighPart = 0;
        tableLen           = ep2->StructureTableLength;
        DbgPrint("[spoof] SMBIOS 2.x table at phys 0x%X, size 0x%X/n",
            tablePhys.LowPart, tableLen);
    }

    MmUnmapIoSpace(scanVA, scanSize);

    if (tableLen == 0 || tablePhys.QuadPart == 0) {
        DbgPrint("[spoof] SMBIOS table not found/n");
        return STATUS_NOT_FOUND;
    }

    // Mapper la table SMBIOS en virtual space
    PVOID tableVA = MmMapIoSpace(tablePhys, tableLen, MmNonCached);
    if (!tableVA) {
        DbgPrint("[spoof] MmMapIoSpace SMBIOS table failed/n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    g_MappedTable   = tableVA;
    g_TableSize     = tableLen;
    g_TablePhysAddr = tablePhys;
    g_PatchCount    = 0;

    // Parcourir et patcher
    WalkAndPatchTable(tableVA, tableLen);

    DbgPrint("[spoof] SMBIOS initialized, %u patches applied/n", g_PatchCount);
    return STATUS_SUCCESS;
}


//
// SpSmbiosRestore : restaure les valeurs originales et unmappe
//
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
    g_TableSize   = 0;

    DbgPrint("[spoof] SMBIOS restored/n");
}
