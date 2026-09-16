# HWID Spoofer — Kernel Driver + Manual Mapper

## Ce que ça fait

| Surface        | Méthode                                              |
|----------------|------------------------------------------------------|
| Disk serials   | Filter device attaché sur chaque \Device\HarddiskX  |
| SMBIOS serials | Patch direct en physical memory (scan `_SM_`/`_SM3_`)|
| NIC MAC        | Hook dispatch \Driver\ndis                           |
| UUID système   | Patch dans la table SMBIOS type 1                    |

## Bypass EAC intégrés

| Check EAC                  | Bypass                                               |
|----------------------------|------------------------------------------------------|
| PiDdbCacheTable            | Pattern scan + unlinking de l'entrée Intel           |
| MmUnloadedDrivers          | Zero de l'entrée iqvw64e                             |
| Pool scan (MZ/PE header)   | Scrub des headers après mapping                      |
| DosDevices enumeration     | Symlink supprimé immédiatement après création        |
| Device name statique       | Nom généré aléatoirement au runtime                  |
| NtQueryIntervalProfile     | Hook temporaire restauré après exécution             |

## Prérequis build

- Windows Driver Kit (WDK) — même génération que ta target
- Visual Studio 2022 + workload "Desktop C++"
- Windows SDK correspondant
- x64 uniquement

## Build

```
# Driver (spoofer.sys)
msbuild driver\spoofer.vcxproj /p:Configuration=Release /p:Platform=x64

# Mapper (mapper.exe)
msbuild mapper\mapper.vcxproj /p:Configuration=Release /p:Platform=x64
```

## Déploiement

```
# En admin — PAS de test signing nécessaire (DSE bypassé par mapper)
mapper.exe spoofer.sys [iqvw64e.sys]
```

iqvw64e.sys doit être à côté du mapper.exe ou passé en argument.
Il n'est pas fourni dans ce zip — CVE-2015-2291, dispo publiquement.

## Communication user-mode

Après chargement, le driver expose un device avec un nom GUID aléatoire.
Le nom exact est loggé via DbgPrint (visible avec DebugView ou WinDbg).

```c
// Ouvrir via NtCreateFile (pas CreateFile — le symlink DOS est supprimé)
UNICODE_STRING devPath;
RtlInitUnicodeString(&devPath, L"\\Device\\{GUID_ICI}");

OBJECT_ATTRIBUTES oa;
InitializeObjectAttributes(&oa, &devPath, OBJ_CASE_INSENSITIVE, NULL, NULL);

HANDLE h;
IO_STATUS_BLOCK isb;
NtCreateFile(&h, GENERIC_READ | GENERIC_WRITE, &oa, &isb,
    NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, 0, NULL, 0);

// Randomiser tous les HWIDs
DWORD bytes;
DeviceIoControl(h, 0x80002000, NULL, 0, NULL, 0, &bytes, NULL);  // RANDOMIZE
DeviceIoControl(h, 0x80002004, NULL, 0, NULL, 0, &bytes, NULL);  // RESTORE
```

## Notes importantes

- **PiDdb patterns** : les offsets dans `intel_driver.cpp` sont valides pour
  W10 20H2–22H2 et W11 21H2–23H2. Si tu target une autre build, update les
  signatures. Outil de référence : github.com/TheCruZ/kdmapper

- **Dispatch table NDIS** : EAC fait des integrity checks sur les MajorFunction[]
  de \Driver\ndis. Le hook NIC peut flag selon la version d'EAC. Fix avancé :
  enregistrer un miniport NDIS filter proprement signé.

- **Test en VM AVANT ta machine principale.**

- **Big pool tracking** : si EAC scanne les BigPool entries, les allocations
  NonPagedPool de grande taille peuvent être tracées. Pour un bypass complet,
  utilise des allocations MDL ou contiguous memory.
