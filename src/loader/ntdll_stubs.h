/*
 * WBOX NTDLL Stub Definitions
 * Maps known ntdll.dll functions to syscall stubs
 */
#ifndef WBOX_NTDLL_STUBS_H
#define WBOX_NTDLL_STUBS_H

#include <string.h>
#include "stubs.h"
#include "../nt/syscalls.h"

/*
 * Known ntdll functions with their stub definitions
 *
 * Note: The num_args field is for stdcall cleanup (ret N*4)
 * These are the number of DWORD arguments on the stack,
 * not including the return address.
 */
static const stub_def_t ntdll_known_stubs[] = {
    /* File operations */
    { "NtClose",            STUB_TYPE_SYSCALL, NtClose,            0, 1 },
    { "NtCreateFile",       STUB_TYPE_SYSCALL, NtCreateFile,       0, 11 },
    { "NtOpenFile",         STUB_TYPE_SYSCALL, NtOpenFile,         0, 6 },
    { "NtReadFile",         STUB_TYPE_SYSCALL, NtReadFile,         0, 9 },
    { "NtWriteFile",        STUB_TYPE_SYSCALL, NtWriteFile,        0, 9 },

    /* Process/thread */
    { "NtTerminateProcess", STUB_TYPE_SYSCALL, NtTerminateProcess, 0, 2 },
    { "NtTerminateThread",  STUB_TYPE_SYSCALL, NtTerminateThread,  0, 2 },

    /* Memory management */
    { "NtAllocateVirtualMemory",  STUB_TYPE_SYSCALL, NtAllocateVirtualMemory,  0, 6 },
    { "NtFreeVirtualMemory",      STUB_TYPE_SYSCALL, NtFreeVirtualMemory,      0, 4 },
    { "NtProtectVirtualMemory",   STUB_TYPE_SYSCALL, NtProtectVirtualMemory,   0, 5 },
    { "NtQueryVirtualMemory",     STUB_TYPE_SYSCALL, NtQueryVirtualMemory,     0, 6 },

    /* Query information */
    { "NtQueryInformationProcess", STUB_TYPE_SYSCALL, NtQueryInformationProcess, 0, 5 },
    { "NtQueryInformationThread",  STUB_TYPE_SYSCALL, NtQueryInformationThread,  0, 5 },
    { "NtQuerySystemInformation",  STUB_TYPE_SYSCALL, NtQuerySystemInformation,  0, 4 },

    /* Synchronization */
    { "NtCreateEvent",      STUB_TYPE_SYSCALL, NtCreateEvent,      0, 5 },
    { "NtSetEvent",         STUB_TYPE_SYSCALL, NtSetEvent,         0, 2 },
    { "NtClearEvent",       STUB_TYPE_SYSCALL, NtClearEvent,       0, 1 },
    { "NtWaitForSingleObject",    STUB_TYPE_SYSCALL, NtWaitForSingleObject,    0, 3 },
    { "NtWaitForMultipleObjects", STUB_TYPE_SYSCALL, NtWaitForMultipleObjects, 0, 5 },
    { "NtDelayExecution",   STUB_TYPE_SYSCALL, NtDelayExecution,   0, 2 },

    /* Registry (stubbed as errors for now) */
    { "NtOpenKey",          STUB_TYPE_SYSCALL, NtOpenKey,          0, 3 },
    { "NtCreateKey",        STUB_TYPE_SYSCALL, NtCreateKey,        0, 7 },
    { "NtQueryValueKey",    STUB_TYPE_SYSCALL, NtQueryValueKey,    0, 6 },
    { "NtSetValueKey",      STUB_TYPE_SYSCALL, NtSetValueKey,      0, 6 },

    /* Section/mapping */
    { "NtCreateSection",    STUB_TYPE_SYSCALL, NtCreateSection,    0, 7 },
    { "NtMapViewOfSection", STUB_TYPE_SYSCALL, NtMapViewOfSection, 0, 10 },
    { "NtUnmapViewOfSection", STUB_TYPE_SYSCALL, NtUnmapViewOfSection, 0, 2 },

    /* Sentinel - end of list */
    { NULL, 0, 0, 0, 0 }
};

/*
 * Lookup a stub definition by function name
 * Returns pointer to stub_def_t if found, NULL otherwise
 */
static inline const stub_def_t *ntdll_lookup_stub(const char *name)
{
    for (const stub_def_t *def = ntdll_known_stubs; def->name != NULL; def++) {
        if (strcmp(def->name, name) == 0) {
            return def;
        }
    }
    return NULL;
}

/*
 * Check if a function is a known ntdll function
 */
static inline bool ntdll_is_known_function(const char *name)
{
    return ntdll_lookup_stub(name) != NULL;
}

#endif /* WBOX_NTDLL_STUBS_H */
