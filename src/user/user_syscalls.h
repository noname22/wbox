/*
 * WBOX USER Syscall Declarations
 */
#ifndef WBOX_USER_SYSCALLS_H
#define WBOX_USER_SYSCALLS_H

#include <stdint.h>
#include <stdbool.h>
#include "../nt/syscalls.h"

/*
 * Bootstrap syscalls (needed for DLL initialization)
 */

/* NtUserProcessConnect - establish shared memory with win32k */
ntstatus_t sys_NtUserProcessConnect(void);

/* NtUserInitializeClientPfnArrays - register client callback functions */
ntstatus_t sys_NtUserInitializeClientPfnArrays(void);

/* NtUserGetClassInfo - get window class information */
ntstatus_t sys_NtUserGetClassInfo(void);

/* NtUserGetClassInfoEx - extended version */
ntstatus_t sys_NtUserGetClassInfoEx(void);

/* NtUserRegisterClassExWOW - register window class */
ntstatus_t sys_NtUserRegisterClassExWOW(void);

/*
 * Window management syscalls
 */

/* NtUserCreateWindowEx - create a window */
ntstatus_t sys_NtUserCreateWindowEx(void);

/*
 * Helper functions
 */

/* Get client callback pointers */
uint32_t user_get_pfn_client_a(void);
uint32_t user_get_pfn_client_w(void);
uint32_t user_get_hmod_user32(void);
bool user_is_client_pfn_init(void);

/* Check if USER subsystem is initialized */
bool user_is_initialized(void);

#endif /* WBOX_USER_SYSCALLS_H */
