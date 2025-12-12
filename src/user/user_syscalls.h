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

/* NtUserMessageCall - message passing and default window procedure */
ntstatus_t sys_NtUserMessageCall(void);

/* NtUserDefSetText - set window text */
ntstatus_t sys_NtUserDefSetText(void);

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
 * Display syscalls
 */

/* NtUserEnumDisplayDevices - enumerate display devices */
ntstatus_t sys_NtUserEnumDisplayDevices(void);

/*
 * Window query syscalls
 */

/* NtUserGetAncestor - get ancestor window */
ntstatus_t sys_NtUserGetAncestor(void);

/* NtUserFindWindowEx - find window by class/title */
ntstatus_t sys_NtUserFindWindowEx(void);

/* NtUserQuerySendMessage - query pending sent message */
ntstatus_t sys_NtUserQuerySendMessage(void);

/* NtUserCountClipboardFormats - count clipboard formats */
ntstatus_t sys_NtUserCountClipboardFormats(void);

/* NtUserGetComboBoxInfo - get combobox info */
ntstatus_t sys_NtUserGetComboBoxInfo(void);

/* NtUserCallHwndLock - misc window operations */
ntstatus_t sys_NtUserCallHwndLock(void);

/* NtGdiGetTextMetricsW - get text metrics */
ntstatus_t sys_NtGdiGetTextMetricsW(void);

/* NtUserShowWindowAsync - show window asynchronously */
ntstatus_t sys_NtUserShowWindowAsync(void);

/* NtUserDeferWindowPos - defer window positioning */
ntstatus_t sys_NtUserDeferWindowPos(void);

/* NtUserGetWOWClass - get WOW16 class */
ntstatus_t sys_NtUserGetWOWClass(void);

/* NtUserOpenWindowStation - open window station object */
ntstatus_t sys_NtUserOpenWindowStation(void);

/* NtUserOpenDesktop - open desktop object */
ntstatus_t sys_NtUserOpenDesktop(void);

/* NtUserOpenInputDesktop - open input desktop */
ntstatus_t sys_NtUserOpenInputDesktop(void);

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
