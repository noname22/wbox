/*
 * WBOX USER Shared Structures
 * Definitions for SERVERINFO, SHAREDINFO, USERCONNECT used by NtUserProcessConnect
 */
#ifndef WBOX_USER_SHARED_H
#define WBOX_USER_SHARED_H

#include <stdint.h>
#include <stdbool.h>

/* System metrics count (SM_CMETRICS in Windows) */
#define WBOX_SM_CMETRICS        97

/* System colors count */
#define WBOX_NUM_SYSCOLORS      31

/* Function ID range for system classes */
#define WBOX_FNID_FIRST         0x029A
#define WBOX_FNID_NUM           32

/* Common system metrics indices */
#define SM_CXSCREEN             0
#define SM_CYSCREEN             1
#define SM_CXVSCROLL            2
#define SM_CYHSCROLL            3
#define SM_CYCAPTION            4
#define SM_CXBORDER             5
#define SM_CYBORDER             6
#define SM_CXDLGFRAME           7
#define SM_CYDLGFRAME           8
#define SM_CYVTHUMB             9
#define SM_CXHTHUMB             10
#define SM_CXICON               11
#define SM_CYICON               12
#define SM_CXCURSOR             13
#define SM_CYCURSOR             14
#define SM_CYMENU               15
#define SM_CXFULLSCREEN         16
#define SM_CYFULLSCREEN         17
#define SM_CYKANJIWINDOW        18
#define SM_MOUSEPRESENT         19
#define SM_CYVSCROLL            20
#define SM_CXHSCROLL            21
#define SM_DEBUG                22
#define SM_SWAPBUTTON           23
#define SM_CXMIN                28
#define SM_CYMIN                29
#define SM_CXSIZE               30
#define SM_CYSIZE               31
#define SM_CXFRAME              32
#define SM_CYFRAME              33
#define SM_CXMINTRACK           34
#define SM_CYMINTRACK           35
#define SM_CXDOUBLECLK          36
#define SM_CYDOUBLECLK          37
#define SM_CXICONSPACING        38
#define SM_CYICONSPACING        39
#define SM_MENUDROPALIGNMENT    40
#define SM_CXSMICON             49
#define SM_CYSMICON             50
#define SM_CYSMCAPTION          51
#define SM_CXSMSIZE             52
#define SM_CYSMSIZE             53
#define SM_CXMENUSIZE           54
#define SM_CYMENUSIZE           55
#define SM_CXMINIMIZED          57
#define SM_CYMINIMIZED          58

/* System color indices */
#define COLOR_SCROLLBAR         0
#define COLOR_BACKGROUND        1
#define COLOR_ACTIVECAPTION     2
#define COLOR_INACTIVECAPTION   3
#define COLOR_MENU              4
#define COLOR_WINDOW            5
#define COLOR_WINDOWFRAME       6
#define COLOR_MENUTEXT          7
#define COLOR_WINDOWTEXT        8
#define COLOR_CAPTIONTEXT       9
#define COLOR_ACTIVEBORDER      10
#define COLOR_INACTIVEBORDER    11
#define COLOR_APPWORKSPACE      12
#define COLOR_HIGHLIGHT         13
#define COLOR_HIGHLIGHTTEXT     14
#define COLOR_BTNFACE           15
#define COLOR_BTNSHADOW         16
#define COLOR_GRAYTEXT          17
#define COLOR_BTNTEXT           18
#define COLOR_INACTIVECAPTIONTEXT 19
#define COLOR_BTNHIGHLIGHT      20
#define COLOR_3DDKSHADOW        21
#define COLOR_3DLIGHT           22
#define COLOR_INFOTEXT          23
#define COLOR_INFOBK            24
#define COLOR_HOTLIGHT          26
#define COLOR_GRADIENTACTIVECAPTION  27
#define COLOR_GRADIENTINACTIVECAPTION 28
#define COLOR_MENUHILIGHT       29
#define COLOR_MENUBAR           30

/* Function ID range for control classes (FNID_xxx) */
#define FNID_FIRST              0x029A
#define FNID_LAST               0x02B8
#define FNID_NUM                (FNID_LAST - FNID_FIRST + 1)  /* 31 */

/* Window message info */
typedef struct _WBOX_WNDMSG {
    uint32_t maxMsgs;           /* Max message number */
    uint32_t abMsgs;            /* Pointer to message bitmap (guest VA) */
} WBOX_WNDMSG;

/*
 * SERVERINFO structure - shared with user mode
 * Contains system metrics, colors, and other global info
 */
typedef struct _WBOX_SERVERINFO {
    uint32_t dwSRVIFlags;       /* Server info flags */
    uint32_t cHandleEntries;    /* Handle table entry count */
    uint16_t wSRVIFlags;        /* Additional flags */
    uint16_t wRIPPID;           /* RIP process ID */
    uint16_t wRIPError;         /* RIP error code */

    /* System metrics */
    int aiSysMet[WBOX_SM_CMETRICS];

    /* System colors (COLORREF values) */
    uint32_t argbSystem[WBOX_NUM_SYSCOLORS];

    /* System brushes for colors */
    uint32_t ahbrSystem[WBOX_NUM_SYSCOLORS];

    /* System class atoms */
    uint16_t atomSysClass[WBOX_FNID_NUM];

    /* Font metrics */
    int cxSysFontChar;
    int cySysFontChar;

    /* Misc */
    uint32_t dwDefaultHeapBase;
    uint32_t dwDefaultHeapSize;
} WBOX_SERVERINFO;

/*
 * SHAREDINFO structure - returned to user mode
 * Must match ReactOS layout exactly (size = 0x118 on x86)
 */
typedef struct _WBOX_SHAREDINFO {
    uint32_t psi;               /* Guest pointer to SERVERINFO */
    uint32_t aheList;           /* Guest pointer to handle entry list */
    uint32_t pDispInfo;         /* Guest pointer to display info */
    uint32_t ulSharedDelta;     /* Delta for pointer fixup */
    WBOX_WNDMSG awmControl[FNID_NUM];  /* Message info per control class (31 entries) */
    WBOX_WNDMSG DefWindowMsgs;         /* Default window messages */
    WBOX_WNDMSG DefWindowSpecMsgs;     /* Default window special messages */
} WBOX_SHAREDINFO;

/*
 * USERCONNECT structure - used with NtUserProcessConnect
 */
typedef struct _WBOX_USERCONNECT {
    uint32_t ulVersion;         /* USER_VERSION (0x00050000) */
    uint32_t ulCurrentVersion;  /* Current version */
    uint32_t dwDispatchCount;   /* Dispatch count */
    WBOX_SHAREDINFO siClient;   /* Shared info */
} WBOX_USERCONNECT;

/* USER_VERSION constant */
#define USER_VERSION    0x00050000

/*
 * Initialize the shared info structures
 * Allocates and populates SERVERINFO with default values
 */
int user_shared_init(void);

/*
 * Shutdown shared info
 */
void user_shared_shutdown(void);

/*
 * Get the global server info pointer
 */
WBOX_SERVERINFO *user_get_serverinfo(void);

/*
 * Get system metric value
 */
int user_get_system_metric(int index);

/*
 * Set system metric value
 */
void user_set_system_metric(int index, int value);

/*
 * Get system color (COLORREF)
 */
uint32_t user_get_system_color(int index);

/*
 * Set system color
 */
void user_set_system_color(int index, uint32_t color);

/*
 * Fill USERCONNECT structure for guest
 * Returns the guest virtual address where SERVERINFO is mapped
 */
uint32_t user_fill_userconnect(WBOX_USERCONNECT *uc);

#endif /* WBOX_USER_SHARED_H */
