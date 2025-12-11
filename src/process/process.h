/*
 * WBOX Process Structures
 * TEB (Thread Environment Block) and PEB (Process Environment Block) initialization
 */
#ifndef WBOX_PROCESS_H
#define WBOX_PROCESS_H

#include <stdint.h>
#include "../vm/vm.h"

/* TEB (Thread Environment Block) offsets */
#define TEB_EXCEPTION_LIST      0x00  /* Pointer to exception handler chain */
#define TEB_STACK_BASE          0x04  /* Stack base (high address) */
#define TEB_STACK_LIMIT         0x08  /* Stack limit (low address) */
#define TEB_SUB_SYSTEM_TIB      0x0C  /* SubSystemTib */
#define TEB_FIBER_DATA          0x10  /* Fiber data / Version (union) */
#define TEB_ARBITRARY_USER_PTR  0x14  /* Arbitrary user pointer */
#define TEB_SELF                0x18  /* Linear address of TEB */
#define TEB_ENVIRONMENT_PTR     0x1C  /* Environment pointer */
#define TEB_PROCESS_ID          0x20  /* Process ID */
#define TEB_THREAD_ID           0x24  /* Thread ID */
#define TEB_ACTIVE_RPC_HANDLE   0x28  /* Active RPC handle */
#define TEB_TLS_POINTER         0x2C  /* Thread local storage pointer */
#define TEB_PEB_POINTER         0x30  /* Pointer to PEB */
#define TEB_LAST_ERROR          0x34  /* Last error value */
#define TEB_TLS_SLOTS           0xE10 /* TLS slots array (64 slots) */
#define TEB_SIZE                0x1000 /* Minimum TEB size */

/* PEB (Process Environment Block) offsets */
#define PEB_INHERITED_ADDR_SPACE     0x00  /* InheritedAddressSpace */
#define PEB_READ_IMAGE_FILE_EXEC_OPT 0x01  /* ReadImageFileExecOptions */
#define PEB_BEING_DEBUGGED           0x02  /* BeingDebugged flag */
#define PEB_SPARE                    0x03  /* Spare byte */
#define PEB_MUTANT                   0x04  /* Mutant */
#define PEB_IMAGE_BASE_ADDRESS       0x08  /* ImageBaseAddress */
#define PEB_LDR                      0x0C  /* Ldr (PEB_LDR_DATA pointer) */
#define PEB_PROCESS_PARAMETERS       0x10  /* ProcessParameters */
#define PEB_SUB_SYSTEM_DATA          0x14  /* SubSystemData */
#define PEB_PROCESS_HEAP             0x18  /* ProcessHeap */
#define PEB_FAST_PEB_LOCK            0x1C  /* FastPebLock */
#define PEB_FAST_PEB_LOCK_ROUTINE    0x20  /* FastPebLockRoutine */
#define PEB_FAST_PEB_UNLOCK_ROUTINE  0x24  /* FastPebUnlockRoutine */
#define PEB_ENVIRONMENT_UPDATE_COUNT 0x28  /* EnvironmentUpdateCount */
#define PEB_KERNEL_CALLBACK_TABLE    0x2C  /* KernelCallbackTable */
#define PEB_OS_MAJOR_VERSION         0x0A4 /* OSMajorVersion */
#define PEB_OS_MINOR_VERSION         0x0A8 /* OSMinorVersion */
#define PEB_OS_BUILD_NUMBER          0x0AC /* OSBuildNumber */
#define PEB_OS_PLATFORM_ID           0x0B0 /* OSPlatformId */
#define PEB_IMAGE_SUBSYSTEM          0x0B4 /* ImageSubsystem */
#define PEB_IMAGE_SUBSYSTEM_MAJOR    0x0B8 /* ImageSubsystemMajorVersion */
#define PEB_IMAGE_SUBSYSTEM_MINOR    0x0BC /* ImageSubsystemMinorVersion */
#define PEB_NUMBER_OF_PROCESSORS     0x64  /* NumberOfProcessors */
#define PEB_NT_GLOBAL_FLAG           0x68  /* NtGlobalFlag */
#define PEB_SESSION_ID               0x1D4 /* SessionId */
#define PEB_SIZE                     0x1000 /* Minimum PEB size */

/* OS version constants (Windows XP SP3) */
#define WBOX_OS_MAJOR_VERSION        5
#define WBOX_OS_MINOR_VERSION        1
#define WBOX_OS_BUILD_NUMBER         2600
#define WBOX_OS_PLATFORM_ID          2     /* VER_PLATFORM_WIN32_NT */

/* Fake process/thread IDs */
#define WBOX_PROCESS_ID              0x1000
#define WBOX_THREAD_ID               0x1004

/*
 * Initialize TEB structure in guest memory
 * vm: VM context with TEB already mapped
 */
void process_init_teb(vm_context_t *vm);

/*
 * Initialize PEB structure in guest memory
 * vm: VM context with PEB already mapped
 */
void process_init_peb(vm_context_t *vm);

/*
 * Get the TEB physical address
 */
uint32_t process_get_teb_phys(vm_context_t *vm);

/*
 * Get the PEB physical address
 */
uint32_t process_get_peb_phys(vm_context_t *vm);

#endif /* WBOX_PROCESS_H */
