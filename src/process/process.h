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
#define TEB_WIN32_THREAD_INFO   0x40  /* Win32ThreadInfo (set by win32k) */
#define TEB_TLS_SLOTS           0xE10 /* TLS slots array (64 slots) */
#define TEB_ACTIVATION_CONTEXT_STACK_PTR 0x1A8 /* ActivationContextStackPointer */
#define TEB_WIN32_CLIENT_INFO   0x6CC /* Win32ClientInfo[62] - 248 bytes for CLIENTINFO */
#define TEB_SIZE                0x1000 /* Minimum TEB size */

/*
 * CLIENTINFO structure offsets (within Win32ClientInfo)
 * This is overlaid on TEB.Win32ClientInfo (at TEB+0x6CC)
 */
#define CI_FLAGS                0x00  /* ULONG_PTR CI_flags */
#define CI_CSPINS               0x04  /* ULONG_PTR cSpins */
#define CI_DWEXPWINVER          0x08  /* DWORD dwExpWinVer */
#define CI_DWCOMPATFLAGS        0x0C  /* DWORD dwCompatFlags */
#define CI_DWCOMPATFLAGS2       0x10  /* DWORD dwCompatFlags2 */
#define CI_DWTIFLAGS            0x14  /* DWORD dwTIFlags */
#define CI_PDESKINFO            0x18  /* PDESKTOPINFO pDeskInfo */
#define CI_ULCLIENTDELTA        0x1C  /* ULONG_PTR ulClientDelta */
/* +0x20: phkCurrent, +0x24: fsHooks */
#define CI_CALLBACKWND          0x28  /* CALLBACKWND structure */
#define CI_CALLBACKWND_HWND     0x28  /* HWND (4 bytes) */
#define CI_CALLBACKWND_PWND     0x2C  /* PWND - guest pointer to WND (4 bytes) */
#define CI_CALLBACKWND_PACTCTX  0x30  /* PVOID pActCtx (4 bytes) */

/* CI_flags bits */
#define CI_INITTHREAD           0x00000008  /* Thread has been initialized */

/*
 * DESKTOPINFO structure offsets
 * Minimal structure for user32 callback support
 */
#define DI_PVDESKTOPBASE        0x00  /* PVOID pvDesktopBase */
#define DI_PVDESKTOPLIMIT       0x04  /* PVOID pvDesktopLimit */
#define DI_SPWND                0x08  /* WND* spwnd (desktop window) */
#define DI_FSHOOKS              0x0C  /* DWORD fsHooks */
/* LIST_ENTRY aphkStart[16] = 16 * 8 = 128 bytes at offset 0x10 */
#define DI_HTASKMANWINDOW       0x90  /* HWND hTaskManWindow */
#define DI_HPROGMANWINDOW       0x94  /* HWND hProgmanWindow */
#define DI_HSHELLWINDOW         0x98  /* HWND hShellWindow */
#define DI_SIZE                 0xA0  /* Approximate size */

/* ACTIVATION_CONTEXT_STACK structure offsets */
#define ACTCTX_STACK_ACTIVE_FRAME        0x00  /* ActiveFrame pointer */
#define ACTCTX_STACK_FRAME_LIST_CACHE    0x04  /* LIST_ENTRY (8 bytes) */
#define ACTCTX_STACK_FLAGS               0x0C  /* Flags */
#define ACTCTX_STACK_NEXT_COOKIE_SEQ     0x10  /* NextCookieSequenceNumber */
#define ACTCTX_STACK_STACK_ID            0x14  /* StackId */
#define ACTCTX_STACK_SIZE                0x18  /* 24 bytes */

/* Address for ActivationContextStack in TEB page */
#define VM_ACTCTX_STACK_ADDR    (VM_TEB_ADDR + 0x800)  /* In TEB page, after main TEB data */

/* PEB (Process Environment Block) offsets */
#define PEB_INHERITED_ADDR_SPACE     0x00  /* InheritedAddressSpace */
#define PEB_READ_IMAGE_FILE_EXEC_OPT 0x01  /* ReadImageFileExecOptions */
#define PEB_BEING_DEBUGGED           0x02  /* BeingDebugged flag */
#define PEB_SPARE                    0x03  /* Spare byte */
#define PEB_MUTANT                   0x04  /* Mutant */
#define PEB_IMAGE_BASE_ADDRESS       0x08  /* ImageBaseAddress */
#define PEB_LDR                      0x0C  /* Ldr (PEB_LDR_DATA pointer) */
#define PEB_PROCESS_PARAMETERS       0x10  /* ProcessParameters */

/* RTL_USER_PROCESS_PARAMETERS offsets
 * Based on ReactOS ntdll structure layout */
#define RUPP_MAX_LENGTH              0x00
#define RUPP_LENGTH                  0x04
#define RUPP_FLAGS                   0x08
#define RUPP_DEBUG_FLAGS             0x0C
#define RUPP_CONSOLE_HANDLE          0x10
#define RUPP_CONSOLE_FLAGS           0x14
#define RUPP_STDIN_HANDLE            0x18
#define RUPP_STDOUT_HANDLE           0x1C
#define RUPP_STDERR_HANDLE           0x20
/* CurrentDirectory is CURDIR: UNICODE_STRING (8 bytes) + HANDLE (4 bytes) = 12 bytes */
#define RUPP_CURRENT_DIR             0x24  /* UNICODE_STRING CurrentDirectory.DosPath */
#define RUPP_CURRENT_DIR_HANDLE      0x2C  /* CurrentDirectory.Handle */
#define RUPP_DLL_PATH                0x30  /* UNICODE_STRING (8 bytes) */
#define RUPP_IMAGE_PATH_NAME         0x38  /* UNICODE_STRING (8 bytes) */
#define RUPP_COMMAND_LINE            0x40  /* UNICODE_STRING (8 bytes) */
#define RUPP_ENVIRONMENT             0x48  /* Pointer to environment block */
#define RUPP_STARTING_X              0x4C
#define RUPP_STARTING_Y              0x50
#define RUPP_COUNT_X                 0x54
#define RUPP_COUNT_Y                 0x58
#define RUPP_COUNT_CHARS_X           0x5C
#define RUPP_COUNT_CHARS_Y           0x60
#define RUPP_FILL_ATTRIBUTE          0x64
#define RUPP_WINDOW_FLAGS            0x68
#define RUPP_SHOW_WINDOW_FLAGS       0x6C
#define RUPP_WINDOW_TITLE            0x70  /* UNICODE_STRING (8 bytes) */
#define RUPP_DESKTOP_INFO            0x78  /* UNICODE_STRING (8 bytes) */
#define RUPP_SHELL_INFO              0x80  /* UNICODE_STRING (8 bytes) */
#define RUPP_RUNTIME_DATA            0x88  /* UNICODE_STRING (8 bytes) */
#define RUPP_SIZE                    0x200 /* Approximate size */
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
/* NLS Code Page Data (for string conversion functions) */
#define PEB_ANSI_CODE_PAGE_DATA      0x58  /* AnsiCodePageData */
#define PEB_OEM_CODE_PAGE_DATA       0x5C  /* OemCodePageData */
#define PEB_UNICODE_CASE_TABLE       0x60  /* UnicodeCaseTableData */

/* GDI */
#define PEB_GDI_SHARED_HANDLE_TABLE  0x94  /* GdiSharedHandleTable */

#define PEB_SIZE                     0x1000 /* Minimum PEB size */

/* TLS Bitmap */
#define PEB_TLS_EXPANSION_COUNTER    0x3C  /* TlsExpansionCounter */
#define PEB_TLS_BITMAP               0x40  /* TlsBitmap (RTL_BITMAP*) */
#define PEB_TLS_BITMAP_BITS          0x44  /* TlsBitmapBits[2] - 64 bits inline */
/* Note: PEB+0x4C is ReadOnlySharedMemoryBase */

/* RTL_BITMAP structure (8 bytes) */
#define RTL_BITMAP_SIZE_OF_BITMAP    0x00  /* Number of bits in bitmap */
#define RTL_BITMAP_BUFFER            0x04  /* Pointer to bitmap buffer */
#define RTL_BITMAP_SIZE              0x08

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

/*
 * Get host pointer to GDI shared handle table
 * Returns NULL if not yet allocated
 */
void *process_get_gdi_shared_table(void);

#endif /* WBOX_PROCESS_H */
