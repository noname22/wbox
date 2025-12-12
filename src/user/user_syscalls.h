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

/* NtUserShowWindow - show/hide a window */
ntstatus_t sys_NtUserShowWindow(void);

/*
 * Message queue syscalls
 */

/* NtUserPeekMessage - peek at message queue */
ntstatus_t sys_NtUserPeekMessage(void);

/* NtUserGetMessage - get message from queue (blocking) */
ntstatus_t sys_NtUserGetMessage(void);

/* NtUserTranslateMessage - translate key messages */
ntstatus_t sys_NtUserTranslateMessage(void);

/* NtUserDispatchMessage - dispatch message to window procedure */
ntstatus_t sys_NtUserDispatchMessage(void);

/* NtUserPostMessage - post message to window */
ntstatus_t sys_NtUserPostMessage(void);

/* NtUserPostQuitMessage - post WM_QUIT */
ntstatus_t sys_NtUserPostQuitMessage(void);

/*
 * Focus/activation syscalls
 */

/* NtUserSetFocus - set keyboard focus */
ntstatus_t sys_NtUserSetFocus(void);

/* NtUserGetForegroundWindow - get foreground window */
ntstatus_t sys_NtUserGetForegroundWindow(void);

/* NtUserSetActiveWindow - set active window */
ntstatus_t sys_NtUserSetActiveWindow(void);

/*
 * Input syscalls
 */

/* NtUserGetKeyState - get key state */
ntstatus_t sys_NtUserGetKeyState(void);

/* NtUserGetAsyncKeyState - get async key state */
ntstatus_t sys_NtUserGetAsyncKeyState(void);

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
