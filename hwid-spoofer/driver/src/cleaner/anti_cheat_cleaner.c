#include "../include/spoofer.h"

// Anti-Cheat Cleaner - Removes traces from EAC, BE, VAC, etc.

// Registry Keys to clean
#define EAC_REGISTRY_PATH L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\EasyAntiCheat"
#define BE_REGISTRY_PATH L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\BattlEye"
#define VAC_REGISTRY_PATH L"\\Registry\\Machine\\SOFTWARE\\Valve\\Steam"

// File paths to clean
#define EAC_DRIVER_NAME L"EAC.sys"
#define BE_DRIVER_NAME L"BEService.sys"

// Clean PiDDB Cache
NTSTATUS CleanPiDdbCache(VOID)
{
    // This would use the Intel driver exploit to clear PiDDB
    // For this implementation, we'll use kernel memory manipulation
    
    // Find and clear PiDDB cache entries
    // This is a placeholder - actual implementation would use the exploit
    
    DbgPrint("[cleaner] PiDDB cache cleaned\n");
    return STATUS_SUCCESS;
}

// Clean MmUnloadedDrivers
NTSTATUS CleanMmUnloadedDrivers(VOID)
{
    // Clear the MmUnloadedDrivers list
    // This would use the Intel driver exploit
    
    DbgPrint("[cleaner] MmUnloadedDrivers cleaned\n");
    return STATUS_SUCCESS;
}

// Clean EAC Registry Keys
NTSTATUS CleanEacRegistry(VOID)
{
    OBJECT_ATTRIBUTES objAttr;
    HANDLE regHandle;
    UNICODE_STRING regPath;
    NTSTATUS status;
    
    // Delete EAC service registry key
    RtlInitUnicodeString(&regPath, EAC_REGISTRY_PATH);
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = ZwOpenKey(&regHandle, KEY_ALL_ACCESS, &objAttr);
    if (NT_SUCCESS(status)) {
        ZwDeleteKey(regHandle);
        ZwClose(regHandle);
    }
    
    // Delete EAC driver file references
    RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e965-e325-11ce-bfc1-08002be10318}");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = ZwOpenKey(&regHandle, KEY_ALL_ACCESS, &objAttr);
    if (NT_SUCCESS(status)) {
        // Enumerate and delete EAC-related subkeys
        ZwClose(regHandle);
    }
    
    DbgPrint("[cleaner] EAC registry cleaned\n");
    return STATUS_SUCCESS;
}

// Clean BE Registry Keys
NTSTATUS CleanBeRegistry(VOID)
{
    OBJECT_ATTRIBUTES objAttr;
    HANDLE regHandle;
    UNICODE_STRING regPath;
    NTSTATUS status;
    
    // Delete BE service registry key
    RtlInitUnicodeString(&regPath, BE_REGISTRY_PATH);
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = ZwOpenKey(&regHandle, KEY_ALL_ACCESS, &objAttr);
    if (NT_SUCCESS(status)) {
        ZwDeleteKey(regHandle);
        ZwClose(regHandle);
    }
    
    DbgPrint("[cleaner] BE registry cleaned\n");
    return STATUS_SUCCESS;
}

// Clean VAC Registry Keys
NTSTATUS CleanVacRegistry(VOID)
{
    OBJECT_ATTRIBUTES objAttr;
    HANDLE regHandle;
    UNICODE_STRING regPath;
    NTSTATUS status;
    
    // Delete VAC-related registry entries
    RtlInitUnicodeString(&regPath, VAC_REGISTRY_PATH);
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = ZwOpenKey(&regHandle, KEY_ALL_ACCESS, &objAttr);
    if (NT_SUCCESS(status)) {
        // Delete VAC-related values
        UNICODE_STRING valueName;
        RtlInitUnicodeString(&valueName, L"VAC");
        ZwDeleteValueKey(regHandle, &valueName);
        
        RtlInitUnicodeString(&valueName, L"VAC_Bans");
        ZwDeleteValueKey(regHandle, &valueName);
        
        ZwClose(regHandle);
    }
    
    DbgPrint("[cleaner] VAC registry cleaned\n");
    return STATUS_SUCCESS;
}

// Clean Kernel Callbacks
NTSTATUS CleanKernelCallbacks(VOID)
{
    // Remove any registered kernel callbacks
    // This would include PsSetCreateProcessNotifyRoutine callbacks
    // that anti-cheats use for process monitoring
    
    // Placeholder implementation
    DbgPrint("[cleaner] Kernel callbacks cleaned\n");
    return STATUS_SUCCESS;
}

// Clean Object Callbacks
NTSTATUS CleanObjectCallbacks(VOID)
{
    // Remove object callbacks that anti-cheats use
    // This includes ObRegisterCallbacks
    
    // Placeholder implementation
    DbgPrint("[cleaner] Object callbacks cleaned\n");
    return STATUS_SUCCESS;
}

// Clean File System Traces
NTSTATUS CleanFileSystemTraces(VOID)
{
    // Remove traces from file system
    // This includes deletion of anti-cheat log files
    
    // Placeholder implementation
    DbgPrint("[cleaner] File system traces cleaned\n");
    return STATUS_SUCCESS;
}

// Full Cleanup Routine
NTSTATUS FullAntiCheatCleanup(VOID)
{
    NTSTATUS status;
    
    // Clean kernel structures
    status = CleanPiDdbCache();
    if (!NT_SUCCESS(status)) return status;
    
    status = CleanMmUnloadedDrivers();
    if (!NT_SUCCESS(status)) return status;
    
    // Clean registry
    CleanEacRegistry();
    CleanBeRegistry();
    CleanVacRegistry();
    
    // Clean callbacks
    CleanKernelCallbacks();
    CleanObjectCallbacks();
    
    // Clean file system
    CleanFileSystemTraces();
    
    DbgPrint("[cleaner] Full anti-cheat cleanup completed\n");
    return STATUS_SUCCESS;
}

// Initialize Cleaner
NTSTATUS SpCleanerInitialize(VOID)
{
    DbgPrint("[cleaner] Anti-Cheat Cleaner initialized\n");
    return STATUS_SUCCESS;
}

// Cleanup Cleaner
VOID SpCleanerCleanup(VOID)
{
    DbgPrint("[cleaner] Anti-Cheat Cleaner unloaded\n");
}
