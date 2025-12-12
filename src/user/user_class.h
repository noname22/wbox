/*
 * WBOX USER Window Class Management
 * Handles window class registration and lookup
 */
#ifndef WBOX_USER_CLASS_H
#define WBOX_USER_CLASS_H

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include "user_shared.h"  /* For FNID_FIRST, FNID_LAST, FNID_NUM */

/* Maximum class name length */
#define MAX_CLASSNAME   256

/* Class style flags (CS_*) */
#define CS_VREDRAW          0x0001
#define CS_HREDRAW          0x0002
#define CS_KEYCVTWINDOW     0x0004
#define CS_DBLCLKS          0x0008
#define CS_OWNDC            0x0020
#define CS_CLASSDC          0x0040
#define CS_PARENTDC         0x0080
#define CS_NOKEYCVT         0x0100
#define CS_NOCLOSE          0x0200
#define CS_SAVEBITS         0x0800
#define CS_BYTEALIGNCLIENT  0x1000
#define CS_BYTEALIGNWINDOW  0x2000
#define CS_GLOBALCLASS      0x4000
#define CS_IME              0x00010000
#define CS_DROPSHADOW       0x00020000

/* System class function IDs (FNID) - specific class IDs */
#define FNID_BUTTON         0x029A
#define FNID_EDIT           0x029B
#define FNID_STATIC         0x029C
#define FNID_LISTBOX        0x029D
#define FNID_SCROLLBAR      0x029E
#define FNID_COMBOBOX       0x029F
#define FNID_MDICLIENT      0x02A0
#define FNID_COMBOLBOX      0x02A1
#define FNID_DIALOG         0x02A2
#define FNID_MENU           0x029C  /* Shares with static */
#define FNID_DESKTOP        0x02A3
#define FNID_DEFWINDOWPROC  0x02A4
#define FNID_MESSAGEWND     0x02A5
#define FNID_SWITCH         0x02A6
#define FNID_ICONTITLE      0x02A7
#define FNID_TOOLTIPS       0x02A8

/* Internal class flags (CSF_*) */
#define CSF_ANSIPROC        0x0001
#define CSF_SYSTEMCLASS     0x0002
#define CSF_WOWDEFERDESTROY 0x0004
#define CSF_CACHEDSMICON    0x0008
#define CSF_WIN40COMPAT     0x0010
#define CSF_VERSIONCLASS    0x0020

/* System class index (ICLS_*) */
#define ICLS_BUTTON         0
#define ICLS_EDIT           1
#define ICLS_STATIC         2
#define ICLS_LISTBOX        3
#define ICLS_SCROLLBAR      4
#define ICLS_COMBOBOX       5
#define ICLS_MDICLIENT      6
#define ICLS_COMBOLBOX      7
#define ICLS_DDEMLEVENT     8
#define ICLS_DDEMLMOTHER    9
#define ICLS_DDEML16BIT     10
#define ICLS_DDEMLCLIENTA   11
#define ICLS_DDEMLCLIENTW   12
#define ICLS_DDEMLSERVERA   13
#define ICLS_DDEMLSERVERW   14
#define ICLS_IME            15
#define ICLS_DIALOG         16
#define ICLS_CTL_MAX        17  /* Max control class */

/*
 * Window class structure
 * Based on Windows/ReactOS CLS structure
 */
typedef struct _WBOX_CLS {
    struct _WBOX_CLS *pclsNext;     /* Next class in list */

    /* Class info */
    uint16_t atomClassName;          /* Class atom */
    uint16_t atomNVClassName;        /* Non-versioned class atom */
    uint32_t style;                  /* CS_* flags */

    /* Callbacks */
    uint32_t lpfnWndProc;           /* Window procedure (guest VA) */
    uint32_t lpfnWndProcA;          /* ANSI window procedure */

    /* Sizes */
    int cbClsExtra;                 /* Extra class bytes */
    int cbWndExtra;                 /* Extra window bytes */

    /* Module */
    uint32_t hModule;               /* HINSTANCE of registering module */

    /* Resources */
    uint32_t hIcon;                 /* Class icon */
    uint32_t hIconSm;               /* Small icon */
    uint32_t hCursor;               /* Class cursor */
    uint32_t hbrBackground;         /* Background brush */
    wchar_t *lpszMenuName;          /* Menu name (heap allocated) */

    /* Name */
    wchar_t szClassName[MAX_CLASSNAME];

    /* System class info */
    uint32_t fnid;                  /* Function ID for system classes */
    uint32_t flags;                 /* CSF_* internal flags */

    /* Reference counting */
    int cWndReferenceCount;         /* Windows using this class */

    /* Extra bytes */
    uint8_t *extraBytes;            /* Extra class bytes if cbClsExtra > 0 */

    /* Guest CLS in desktop heap */
    uint32_t guest_cls_va;          /* Guest VA of CLS structure */
} WBOX_CLS;

/*
 * WNDCLASSEXW structure (matches Windows definition)
 * Used for NtUserGetClassInfo and NtUserRegisterClassExWOW
 */
typedef struct _WBOX_WNDCLASSEXW {
    uint32_t cbSize;
    uint32_t style;
    uint32_t lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    uint32_t hInstance;
    uint32_t hIcon;
    uint32_t hCursor;
    uint32_t hbrBackground;
    uint32_t lpszMenuName;      /* Guest pointer to menu name */
    uint32_t lpszClassName;     /* Guest pointer to class name */
    uint32_t hIconSm;
} WBOX_WNDCLASSEXW;

/*
 * Initialize the class subsystem
 * Registers system classes (Button, Edit, etc.)
 */
int user_class_init(void);

/*
 * Shutdown class subsystem
 */
void user_class_shutdown(void);

/*
 * Register a new window class
 * Returns the class atom, or 0 on failure
 */
uint16_t user_class_register(WBOX_CLS *cls);

/*
 * Unregister a window class
 */
bool user_class_unregister(const wchar_t *className, uint32_t hInstance);

/*
 * Find a window class by name and instance
 * Returns NULL if not found
 */
WBOX_CLS *user_class_find(const wchar_t *className, uint32_t hInstance);

/*
 * Find a window class by atom
 */
WBOX_CLS *user_class_find_by_atom(uint16_t atom);

/*
 * Get class info and fill WNDCLASSEXW
 * Returns class atom, or 0 if not found
 */
uint16_t user_class_get_info(const wchar_t *className, uint32_t hInstance,
                             WBOX_WNDCLASSEXW *wcx);

/*
 * Add a reference to a class (window using it)
 */
void user_class_add_ref(WBOX_CLS *cls);

/*
 * Release a reference to a class
 */
void user_class_release(WBOX_CLS *cls);

/*
 * Register system classes (called during init)
 */
int user_class_register_system_classes(void);

/*
 * Allocate a class atom for a name
 */
uint16_t user_class_add_atom(const wchar_t *name);

/*
 * Get atom name
 */
const wchar_t *user_class_get_atom_name(uint16_t atom);

/*
 * Check if a name is a system class
 */
bool user_class_is_system_class(const wchar_t *className);

/*
 * Get system class by ICLS index
 */
WBOX_CLS *user_class_get_system_class(int icls);

#endif /* WBOX_USER_CLASS_H */
