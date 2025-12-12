/*
 * WBOX USER Window Class Management Implementation
 */
#include "user_class.h"
#include "user_shared.h"
#include "guest_cls.h"
#include "desktop_heap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wctype.h>

/* Linked list of all registered classes */
static WBOX_CLS *g_class_list = NULL;

/* System classes array (indexed by ICLS_*) */
static WBOX_CLS *g_system_classes[ICLS_CTL_MAX] = {0};

/* Atom table for class names */
#define MAX_ATOMS   1024
static struct {
    wchar_t name[MAX_CLASSNAME];
    bool used;
} g_atom_table[MAX_ATOMS];
static uint16_t g_next_atom = 0xC000;  /* User atoms start at 0xC000 */

/* System class definitions */
static const struct {
    const wchar_t *name;
    int icls;
    uint32_t fnid;
    uint32_t style;
    int cbWndExtra;
} system_class_defs[] = {
    { L"Button",      ICLS_BUTTON,    FNID_BUTTON,    CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS | CS_PARENTDC, 0 },
    { L"Edit",        ICLS_EDIT,      FNID_EDIT,      CS_DBLCLKS | CS_PARENTDC | CS_GLOBALCLASS, 6 },
    { L"Static",      ICLS_STATIC,    FNID_STATIC,    CS_DBLCLKS | CS_PARENTDC | CS_GLOBALCLASS, 0 },
    { L"ListBox",     ICLS_LISTBOX,   FNID_LISTBOX,   CS_DBLCLKS | CS_PARENTDC | CS_GLOBALCLASS, 0 },
    { L"ScrollBar",   ICLS_SCROLLBAR, FNID_SCROLLBAR, CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS | CS_PARENTDC, 0 },
    { L"ComboBox",    ICLS_COMBOBOX,  FNID_COMBOBOX,  CS_DBLCLKS | CS_PARENTDC | CS_GLOBALCLASS, 0 },
    { L"MDIClient",   ICLS_MDICLIENT, FNID_MDICLIENT, CS_GLOBALCLASS, 0 },
    { L"ComboLBox",   ICLS_COMBOLBOX, FNID_COMBOLBOX, CS_DBLCLKS | CS_SAVEBITS | CS_GLOBALCLASS, 0 },
    { NULL, 0, 0, 0, 0 }
};

/* Case-insensitive wide string compare */
static int wcsicmp_local(const wchar_t *s1, const wchar_t *s2)
{
    while (*s1 && *s2) {
        wchar_t c1 = towlower(*s1);
        wchar_t c2 = towlower(*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return towlower(*s1) - towlower(*s2);
}

int user_class_init(void)
{
    g_class_list = NULL;
    memset(g_system_classes, 0, sizeof(g_system_classes));
    memset(g_atom_table, 0, sizeof(g_atom_table));
    g_next_atom = 0xC000;

    /* Register system classes */
    if (user_class_register_system_classes() < 0) {
        return -1;
    }

    printf("USER: Class subsystem initialized\n");
    return 0;
}

void user_class_shutdown(void)
{
    /* Free all classes */
    WBOX_CLS *cls = g_class_list;
    while (cls) {
        WBOX_CLS *next = cls->pclsNext;
        if (cls->lpszMenuName) {
            free(cls->lpszMenuName);
        }
        if (cls->extraBytes) {
            free(cls->extraBytes);
        }
        free(cls);
        cls = next;
    }

    g_class_list = NULL;
    memset(g_system_classes, 0, sizeof(g_system_classes));
}

uint16_t user_class_add_atom(const wchar_t *name)
{
    if (!name || !name[0]) {
        return 0;
    }

    /* Check if atom already exists */
    for (int i = 0; i < MAX_ATOMS; i++) {
        if (g_atom_table[i].used && wcsicmp_local(g_atom_table[i].name, name) == 0) {
            return 0xC000 + i;
        }
    }

    /* Allocate new atom */
    for (int i = 0; i < MAX_ATOMS; i++) {
        if (!g_atom_table[i].used) {
            wcsncpy(g_atom_table[i].name, name, MAX_CLASSNAME - 1);
            g_atom_table[i].name[MAX_CLASSNAME - 1] = 0;
            g_atom_table[i].used = true;
            return 0xC000 + i;
        }
    }

    return 0;  /* Table full */
}

const wchar_t *user_class_get_atom_name(uint16_t atom)
{
    if (atom < 0xC000) {
        return NULL;  /* Not a user atom */
    }

    int index = atom - 0xC000;
    if (index < 0 || index >= MAX_ATOMS || !g_atom_table[index].used) {
        return NULL;
    }

    return g_atom_table[index].name;
}

uint16_t user_class_register(WBOX_CLS *cls)
{
    if (!cls || !cls->szClassName[0]) {
        return 0;
    }

    /* Check for duplicate */
    if (user_class_find(cls->szClassName, cls->hModule)) {
        printf("USER: Class '%ls' already registered\n", cls->szClassName);
        return 0;
    }

    /* Allocate atom if not already set */
    if (cls->atomClassName == 0) {
        cls->atomClassName = user_class_add_atom(cls->szClassName);
        if (cls->atomClassName == 0) {
            printf("USER: Failed to allocate atom for class '%ls'\n", cls->szClassName);
            return 0;
        }
    }

    /* Allocate extra bytes if needed */
    if (cls->cbClsExtra > 0 && !cls->extraBytes) {
        cls->extraBytes = calloc(1, cls->cbClsExtra);
    }

    /* Add to list */
    cls->pclsNext = g_class_list;
    g_class_list = cls;

    /* Create guest CLS in desktop heap if initialized */
    if (desktop_heap_get()) {
        cls->guest_cls_va = guest_cls_create(cls);
    }

    printf("USER: Registered class '%ls' (atom 0x%04X, fnid 0x%04X)\n",
           cls->szClassName, cls->atomClassName, cls->fnid);

    return cls->atomClassName;
}

bool user_class_unregister(const wchar_t *className, uint32_t hInstance)
{
    WBOX_CLS **pp = &g_class_list;

    while (*pp) {
        WBOX_CLS *cls = *pp;

        if (wcsicmp_local(cls->szClassName, className) == 0) {
            /* System classes can't be unregistered */
            if (cls->flags & CSF_SYSTEMCLASS) {
                return false;
            }

            /* Check instance match for non-global classes */
            if (!(cls->style & CS_GLOBALCLASS) && cls->hModule != hInstance) {
                pp = &cls->pclsNext;
                continue;
            }

            /* Can't unregister if windows still using it */
            if (cls->cWndReferenceCount > 0) {
                return false;
            }

            /* Remove from list */
            *pp = cls->pclsNext;

            /* Destroy guest CLS */
            if (cls->guest_cls_va) {
                guest_cls_destroy(cls->guest_cls_va);
            }

            /* Free */
            if (cls->lpszMenuName) free(cls->lpszMenuName);
            if (cls->extraBytes) free(cls->extraBytes);
            free(cls);

            return true;
        }

        pp = &cls->pclsNext;
    }

    return false;
}

WBOX_CLS *user_class_find(const wchar_t *className, uint32_t hInstance)
{
    if (!className) {
        return NULL;
    }

    /* Check if className is actually an atom (low word) */
    if (((uintptr_t)className & 0xFFFF0000) == 0) {
        return user_class_find_by_atom((uint16_t)(uintptr_t)className);
    }

    for (WBOX_CLS *cls = g_class_list; cls; cls = cls->pclsNext) {
        if (wcsicmp_local(cls->szClassName, className) == 0) {
            /* Global classes match any instance */
            if (cls->style & CS_GLOBALCLASS) {
                return cls;
            }
            /* System classes match any instance */
            if (cls->flags & CSF_SYSTEMCLASS) {
                return cls;
            }
            /* Otherwise must match instance */
            if (cls->hModule == hInstance || hInstance == 0) {
                return cls;
            }
        }
    }

    return NULL;
}

WBOX_CLS *user_class_find_by_atom(uint16_t atom)
{
    if (atom == 0) {
        return NULL;
    }

    for (WBOX_CLS *cls = g_class_list; cls; cls = cls->pclsNext) {
        if (cls->atomClassName == atom) {
            return cls;
        }
    }

    return NULL;
}

uint16_t user_class_get_info(const wchar_t *className, uint32_t hInstance,
                             WBOX_WNDCLASSEXW *wcx)
{
    WBOX_CLS *cls = user_class_find(className, hInstance);
    if (!cls) {
        return 0;
    }

    if (wcx) {
        memset(wcx, 0, sizeof(WBOX_WNDCLASSEXW));
        wcx->cbSize = sizeof(WBOX_WNDCLASSEXW);
        wcx->style = cls->style;
        wcx->lpfnWndProc = cls->lpfnWndProc;
        wcx->cbClsExtra = cls->cbClsExtra;
        wcx->cbWndExtra = cls->cbWndExtra;
        wcx->hInstance = cls->hModule;
        wcx->hIcon = cls->hIcon;
        wcx->hCursor = cls->hCursor;
        wcx->hbrBackground = cls->hbrBackground;
        wcx->lpszMenuName = 0;  /* Guest pointer - set separately if needed */
        wcx->lpszClassName = 0; /* Guest pointer - set separately if needed */
        wcx->hIconSm = cls->hIconSm;
    }

    return cls->atomClassName;
}

void user_class_add_ref(WBOX_CLS *cls)
{
    if (cls) {
        cls->cWndReferenceCount++;
    }
}

void user_class_release(WBOX_CLS *cls)
{
    if (cls && cls->cWndReferenceCount > 0) {
        cls->cWndReferenceCount--;
    }
}

bool user_class_is_system_class(const wchar_t *className)
{
    if (!className) {
        return false;
    }

    for (int i = 0; system_class_defs[i].name != NULL; i++) {
        if (wcsicmp_local(className, system_class_defs[i].name) == 0) {
            return true;
        }
    }

    return false;
}

WBOX_CLS *user_class_get_system_class(int icls)
{
    if (icls < 0 || icls >= ICLS_CTL_MAX) {
        return NULL;
    }
    return g_system_classes[icls];
}

int user_class_register_system_classes(void)
{
    WBOX_SERVERINFO *psi = user_get_serverinfo();

    for (int i = 0; system_class_defs[i].name != NULL; i++) {
        WBOX_CLS *cls = calloc(1, sizeof(WBOX_CLS));
        if (!cls) {
            return -1;
        }

        /* Copy class info */
        wcsncpy(cls->szClassName, system_class_defs[i].name, MAX_CLASSNAME - 1);
        cls->style = system_class_defs[i].style | CS_GLOBALCLASS;
        cls->fnid = system_class_defs[i].fnid;
        cls->cbWndExtra = system_class_defs[i].cbWndExtra;
        cls->flags = CSF_SYSTEMCLASS;

        /* Default resources */
        cls->hCursor = 0;   /* Will use default arrow cursor */
        cls->hbrBackground = 0;  /* Will use default */

        /* Window procedure - these are handled specially by USER
         * The actual proc is provided by the client via InitializeClientPfnArrays
         * For now, set to 0 - will be patched when client calls init */
        cls->lpfnWndProc = 0;

        /* Register with atom */
        cls->atomClassName = user_class_add_atom(cls->szClassName);

        /* Add to class list */
        cls->pclsNext = g_class_list;
        g_class_list = cls;

        /* Store in system classes array */
        int icls = system_class_defs[i].icls;
        if (icls >= 0 && icls < ICLS_CTL_MAX) {
            g_system_classes[icls] = cls;
        }

        /* Store atom in SERVERINFO */
        if (psi && icls >= 0 && icls < WBOX_FNID_NUM) {
            psi->atomSysClass[icls] = cls->atomClassName;
        }

        printf("USER: System class '%ls' registered (atom 0x%04X)\n",
               cls->szClassName, cls->atomClassName);
    }

    return 0;
}
