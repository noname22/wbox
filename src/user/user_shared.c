/*
 * WBOX USER Shared Structures Implementation
 */
#include "user_shared.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../cpu/mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global server info */
static WBOX_SERVERINFO *g_serverinfo = NULL;
static uint32_t g_serverinfo_guest_va = 0;
static bool g_shared_initialized = false;

/* Guest handle table */
static uint32_t g_handle_table_guest_va = 0;
static uint32_t g_handle_entries_guest_va = 0;

/* Guest-visible handle table structure */
typedef struct _GUEST_USER_HANDLE_TABLE {
    uint32_t handles;           /* Guest pointer to entries array */
    uint32_t freelist;          /* Guest pointer to first free entry */
    int32_t nb_handles;         /* Current number of handles */
    int32_t allocated_handles;  /* Total allocated */
} GUEST_USER_HANDLE_TABLE;

/* Guest-visible handle entry structure - matches ReactOS USER_HANDLE_ENTRY */
typedef struct _GUEST_USER_HANDLE_ENTRY {
    uint32_t ptr;               /* Guest pointer to object */
    uint32_t owner;             /* Guest pointer to owner */
    uint8_t type;               /* object type (0 if free) */
    uint8_t flags;
    uint16_t generation;        /* generation counter */
} GUEST_USER_HANDLE_ENTRY;

#define GUEST_MAX_HANDLES 4096

/* Default system colors (Windows classic theme) */
static const uint32_t default_colors[WBOX_NUM_SYSCOLORS] = {
    [COLOR_SCROLLBAR]       = 0x00C8C8C8,
    [COLOR_BACKGROUND]      = 0x00004E98,  /* Desktop */
    [COLOR_ACTIVECAPTION]   = 0x00D1B499,
    [COLOR_INACTIVECAPTION] = 0x00DBCDBF,
    [COLOR_MENU]            = 0x00F0F0F0,
    [COLOR_WINDOW]          = 0x00FFFFFF,
    [COLOR_WINDOWFRAME]     = 0x00646464,
    [COLOR_MENUTEXT]        = 0x00000000,
    [COLOR_WINDOWTEXT]      = 0x00000000,
    [COLOR_CAPTIONTEXT]     = 0x00000000,
    [COLOR_ACTIVEBORDER]    = 0x00B4B4B4,
    [COLOR_INACTIVEBORDER]  = 0x00F4F7FC,
    [COLOR_APPWORKSPACE]    = 0x00ABABAB,
    [COLOR_HIGHLIGHT]       = 0x00FF9933,
    [COLOR_HIGHLIGHTTEXT]   = 0x00FFFFFF,
    [COLOR_BTNFACE]         = 0x00F0F0F0,
    [COLOR_BTNSHADOW]       = 0x00A0A0A0,
    [COLOR_GRAYTEXT]        = 0x006D6D6D,
    [COLOR_BTNTEXT]         = 0x00000000,
    [COLOR_INACTIVECAPTIONTEXT] = 0x00000000,
    [COLOR_BTNHIGHLIGHT]    = 0x00FFFFFF,
    [COLOR_3DDKSHADOW]      = 0x00696969,
    [COLOR_3DLIGHT]         = 0x00E3E3E3,
    [COLOR_INFOTEXT]        = 0x00000000,
    [COLOR_INFOBK]          = 0x00FFFFE1,
    [COLOR_HOTLIGHT]        = 0x00CC6600,
    [COLOR_GRADIENTACTIVECAPTION]   = 0x00EAD1B9,
    [COLOR_GRADIENTINACTIVECAPTION] = 0x00F2E4D7,
    [COLOR_MENUHILIGHT]     = 0x00FF9933,
    [COLOR_MENUBAR]         = 0x00F0F0F0,
};

/*
 * Initialize shared info with default values
 */
int user_shared_init(void)
{
    if (g_shared_initialized) {
        return 0;
    }

    /* Allocate server info */
    g_serverinfo = calloc(1, sizeof(WBOX_SERVERINFO));
    if (!g_serverinfo) {
        return -1;
    }

    /* Set version flags */
    g_serverinfo->dwSRVIFlags = 0;
    g_serverinfo->cHandleEntries = 4096;  /* Match GDI handle table size */

    /* Initialize system metrics with reasonable defaults */
    g_serverinfo->aiSysMet[SM_CXSCREEN] = 800;
    g_serverinfo->aiSysMet[SM_CYSCREEN] = 600;
    g_serverinfo->aiSysMet[SM_CXVSCROLL] = 17;
    g_serverinfo->aiSysMet[SM_CYHSCROLL] = 17;
    g_serverinfo->aiSysMet[SM_CYCAPTION] = 22;
    g_serverinfo->aiSysMet[SM_CXBORDER] = 1;
    g_serverinfo->aiSysMet[SM_CYBORDER] = 1;
    g_serverinfo->aiSysMet[SM_CXDLGFRAME] = 3;
    g_serverinfo->aiSysMet[SM_CYDLGFRAME] = 3;
    g_serverinfo->aiSysMet[SM_CYVTHUMB] = 17;
    g_serverinfo->aiSysMet[SM_CXHTHUMB] = 17;
    g_serverinfo->aiSysMet[SM_CXICON] = 32;
    g_serverinfo->aiSysMet[SM_CYICON] = 32;
    g_serverinfo->aiSysMet[SM_CXCURSOR] = 32;
    g_serverinfo->aiSysMet[SM_CYCURSOR] = 32;
    g_serverinfo->aiSysMet[SM_CYMENU] = 19;
    g_serverinfo->aiSysMet[SM_CXFULLSCREEN] = 800;
    g_serverinfo->aiSysMet[SM_CYFULLSCREEN] = 578;  /* Screen - caption - menu */
    g_serverinfo->aiSysMet[SM_MOUSEPRESENT] = 1;
    g_serverinfo->aiSysMet[SM_CYVSCROLL] = 17;
    g_serverinfo->aiSysMet[SM_CXHSCROLL] = 17;
    g_serverinfo->aiSysMet[SM_DEBUG] = 0;
    g_serverinfo->aiSysMet[SM_SWAPBUTTON] = 0;
    g_serverinfo->aiSysMet[SM_CXMIN] = 112;
    g_serverinfo->aiSysMet[SM_CYMIN] = 27;
    g_serverinfo->aiSysMet[SM_CXSIZE] = 18;
    g_serverinfo->aiSysMet[SM_CYSIZE] = 18;
    g_serverinfo->aiSysMet[SM_CXFRAME] = 4;
    g_serverinfo->aiSysMet[SM_CYFRAME] = 4;
    g_serverinfo->aiSysMet[SM_CXMINTRACK] = 112;
    g_serverinfo->aiSysMet[SM_CYMINTRACK] = 27;
    g_serverinfo->aiSysMet[SM_CXDOUBLECLK] = 4;
    g_serverinfo->aiSysMet[SM_CYDOUBLECLK] = 4;
    g_serverinfo->aiSysMet[SM_CXICONSPACING] = 75;
    g_serverinfo->aiSysMet[SM_CYICONSPACING] = 75;
    g_serverinfo->aiSysMet[SM_MENUDROPALIGNMENT] = 0;
    g_serverinfo->aiSysMet[SM_CXSMICON] = 16;
    g_serverinfo->aiSysMet[SM_CYSMICON] = 16;
    g_serverinfo->aiSysMet[SM_CYSMCAPTION] = 17;
    g_serverinfo->aiSysMet[SM_CXSMSIZE] = 13;
    g_serverinfo->aiSysMet[SM_CYSMSIZE] = 13;
    g_serverinfo->aiSysMet[SM_CXMENUSIZE] = 18;
    g_serverinfo->aiSysMet[SM_CYMENUSIZE] = 18;
    g_serverinfo->aiSysMet[SM_CXMINIMIZED] = 160;
    g_serverinfo->aiSysMet[SM_CYMINIMIZED] = 24;

    /* Copy default colors */
    memcpy(g_serverinfo->argbSystem, default_colors, sizeof(default_colors));

    /* Font metrics */
    g_serverinfo->cxSysFontChar = 8;
    g_serverinfo->cySysFontChar = 16;

    g_shared_initialized = true;
    printf("USER: Shared info initialized\n");

    return 0;
}

void user_shared_shutdown(void)
{
    if (g_serverinfo) {
        free(g_serverinfo);
        g_serverinfo = NULL;
    }
    g_serverinfo_guest_va = 0;
    g_handle_table_guest_va = 0;
    g_handle_entries_guest_va = 0;
    g_shared_initialized = false;
}

WBOX_SERVERINFO *user_get_serverinfo(void)
{
    return g_serverinfo;
}

int user_get_system_metric(int index)
{
    if (!g_serverinfo || index < 0 || index >= WBOX_SM_CMETRICS) {
        return 0;
    }
    return g_serverinfo->aiSysMet[index];
}

void user_set_system_metric(int index, int value)
{
    if (g_serverinfo && index >= 0 && index < WBOX_SM_CMETRICS) {
        g_serverinfo->aiSysMet[index] = value;
    }
}

uint32_t user_get_system_color(int index)
{
    if (!g_serverinfo || index < 0 || index >= WBOX_NUM_SYSCOLORS) {
        return 0;
    }
    return g_serverinfo->argbSystem[index];
}

void user_set_system_color(int index, uint32_t color)
{
    if (g_serverinfo && index >= 0 && index < WBOX_NUM_SYSCOLORS) {
        g_serverinfo->argbSystem[index] = color;
    }
}

/*
 * Allocate guest memory for handle table and entries
 */
static int user_alloc_guest_handle_table(vm_context_t *vm)
{
    if (g_handle_table_guest_va != 0) {
        return 0;  /* Already allocated */
    }

    /* Use fixed addresses after SERVERINFO region */
    g_handle_table_guest_va = 0x7F030000;
    g_handle_entries_guest_va = 0x7F031000;

    /* Allocate physical memory for handle table structure (1 page) */
    uint32_t table_phys = paging_alloc_phys(&vm->paging, PAGE_SIZE);
    if (table_phys == 0) {
        printf("USER: Failed to allocate memory for handle table\n");
        return -1;
    }

    /* Map handle table */
    if (paging_map_page(&vm->paging, g_handle_table_guest_va, table_phys,
                        PTE_PRESENT | PTE_USER | PTE_WRITABLE) < 0) {
        printf("USER: Failed to map handle table\n");
        return -1;
    }

    /* Allocate physical memory for handle entries array
     * Size: GUEST_MAX_HANDLES * sizeof(GUEST_USER_HANDLE_ENTRY) = 4096 * 12 = 48KB
     * Round up to 12 pages (49152 bytes) */
    uint32_t entries_size = GUEST_MAX_HANDLES * sizeof(GUEST_USER_HANDLE_ENTRY);
    uint32_t entries_alloc = (entries_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t entries_phys = paging_alloc_phys(&vm->paging, entries_alloc);
    if (entries_phys == 0) {
        printf("USER: Failed to allocate memory for handle entries\n");
        return -1;
    }

    /* Map handle entries */
    if (paging_map_range(&vm->paging, g_handle_entries_guest_va, entries_phys,
                         entries_alloc, PTE_PRESENT | PTE_USER | PTE_WRITABLE) < 0) {
        printf("USER: Failed to map handle entries\n");
        return -1;
    }

    /* Initialize handle table structure in guest memory */
    GUEST_USER_HANDLE_TABLE table;
    table.handles = g_handle_entries_guest_va;
    table.freelist = g_handle_entries_guest_va;  /* First entry is head of free list */
    table.nb_handles = 0;
    table.allocated_handles = GUEST_MAX_HANDLES;

    /* Write handle table to guest */
    const uint8_t *table_src = (const uint8_t *)&table;
    for (uint32_t i = 0; i < sizeof(GUEST_USER_HANDLE_TABLE); i++) {
        mem_writeb_phys(table_phys + i, table_src[i]);
    }

    /* Zero out handle entries */
    for (uint32_t i = 0; i < entries_alloc; i++) {
        mem_writeb_phys(entries_phys + i, 0);
    }

    printf("USER: Handle table mapped at guest VA 0x%08X\n", g_handle_table_guest_va);
    printf("USER: Handle entries mapped at guest VA 0x%08X (size %u)\n",
           g_handle_entries_guest_va, entries_size);

    return 0;
}

/*
 * Map SERVERINFO into guest address space and fill USERCONNECT
 * For simplicity, we allocate a page in guest memory and copy the data there
 */
uint32_t user_fill_userconnect(WBOX_USERCONNECT *uc)
{
    vm_context_t *vm = vm_get_context();
    if (!vm || !g_serverinfo) {
        return 0;
    }

    /* Allocate guest memory for SERVERINFO if not already done */
    if (g_serverinfo_guest_va == 0) {
        /* Use a fixed address in shared user space */
        g_serverinfo_guest_va = 0x7F020000;  /* After loader heap region */

        /* Allocate physical memory for SERVERINFO */
        uint32_t si_size = sizeof(WBOX_SERVERINFO);
        uint32_t alloc_size = (si_size + 0xFFF) & ~0xFFF;  /* Round up to page */
        uint32_t phys = paging_alloc_phys(&vm->paging, alloc_size);
        if (phys == 0) {
            printf("USER: Failed to allocate memory for SERVERINFO\n");
            return 0;
        }

        /* Map pages to guest virtual address */
        if (paging_map_range(&vm->paging, g_serverinfo_guest_va, phys,
                             alloc_size, PTE_PRESENT | PTE_USER | PTE_WRITABLE) < 0) {
            printf("USER: Failed to map SERVERINFO pages\n");
            return 0;
        }

        /* Copy SERVERINFO to guest memory via physical addresses */
        const uint8_t *src = (const uint8_t *)g_serverinfo;
        for (uint32_t i = 0; i < sizeof(WBOX_SERVERINFO); i++) {
            mem_writeb_phys(phys + i, src[i]);
        }

        printf("USER: SERVERINFO mapped at guest VA 0x%08X\n", g_serverinfo_guest_va);
    }

    /* Allocate guest handle table if not already done */
    if (user_alloc_guest_handle_table(vm) < 0) {
        return 0;
    }

    /* Fill USERCONNECT structure */
    memset(uc, 0, sizeof(WBOX_USERCONNECT));
    uc->ulVersion = USER_VERSION;
    uc->ulCurrentVersion = USER_VERSION;
    uc->dwDispatchCount = 0;

    /* SHAREDINFO points to our mapped guest structures */
    uc->siClient.psi = g_serverinfo_guest_va;
    uc->siClient.aheList = g_handle_table_guest_va;
    uc->siClient.pDispInfo = 0;  /* NULL display info for now */
    uc->siClient.ulSharedDelta = 0;

    /* WNDMSG arrays are zeroed - no special message handling needed initially */
    /* awmControl[], DefWindowMsgs, DefWindowSpecMsgs all zeroed by memset */

    return g_serverinfo_guest_va;
}
