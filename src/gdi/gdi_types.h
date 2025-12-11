/*
 * WBOX GDI Type Definitions
 * Windows-compatible GDI types for the emulator
 */
#ifndef WBOX_GDI_TYPES_H
#define WBOX_GDI_TYPES_H

#include <stdint.h>

/* Basic Windows types */
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef int32_t  LONG;
typedef uint32_t UINT;
typedef int32_t  INT;
typedef int      BOOL;

/* Handle types (all 32-bit values in guest) */
typedef uint32_t HANDLE;
typedef uint32_t HWND;
typedef uint32_t HDC;
typedef uint32_t HBITMAP;
typedef uint32_t HBRUSH;
typedef uint32_t HPEN;
typedef uint32_t HFONT;
typedef uint32_t HRGN;
typedef uint32_t HGDIOBJ;
typedef uint32_t HMENU;
typedef uint32_t HICON;
typedef uint32_t HCURSOR;
typedef uint32_t HINSTANCE;

/* Color */
typedef uint32_t COLORREF;
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(c) ((BYTE)(c))
#define GetGValue(c) ((BYTE)(((WORD)(c))>>8))
#define GetBValue(c) ((BYTE)((c)>>16))

/* Point */
typedef struct {
    LONG x;
    LONG y;
} POINT;

/* Size */
typedef struct {
    LONG cx;
    LONG cy;
} SIZE;

/* Rectangle */
typedef struct {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT;

/* Null handles */
#define NULL_HANDLE  0

/* Boolean */
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* GDI object types */
#define OBJ_PEN         1
#define OBJ_BRUSH       2
#define OBJ_DC          3
#define OBJ_METADC      4
#define OBJ_PAL         5
#define OBJ_FONT        6
#define OBJ_BITMAP      7
#define OBJ_REGION      8
#define OBJ_METAFILE    9
#define OBJ_MEMDC       10
#define OBJ_EXTPEN      11
#define OBJ_ENHMETADC   12
#define OBJ_ENHMETAFILE 13

/* Stock objects */
#define WHITE_BRUSH         0
#define LTGRAY_BRUSH        1
#define GRAY_BRUSH          2
#define DKGRAY_BRUSH        3
#define BLACK_BRUSH         4
#define NULL_BRUSH          5
#define WHITE_PEN           6
#define BLACK_PEN           7
#define NULL_PEN            8
#define OEM_FIXED_FONT      10
#define ANSI_FIXED_FONT     11
#define ANSI_VAR_FONT       12
#define SYSTEM_FONT         13
#define DEVICE_DEFAULT_FONT 14
#define DEFAULT_PALETTE     15
#define SYSTEM_FIXED_FONT   16
#define DEFAULT_GUI_FONT    17
#define DC_BRUSH            18
#define DC_PEN              19
#define STOCK_LAST          19

/* Device caps */
#define HORZRES      8
#define VERTRES      10
#define BITSPIXEL    12
#define PLANES       14
#define NUMCOLORS    24
#define LOGPIXELSX   88
#define LOGPIXELSY   90

/* Brush styles */
#define BS_SOLID      0
#define BS_NULL       1
#define BS_HOLLOW     BS_NULL
#define BS_HATCHED    2
#define BS_PATTERN    3

/* Pen styles */
#define PS_SOLID      0
#define PS_DASH       1
#define PS_DOT        2
#define PS_DASHDOT    3
#define PS_DASHDOTDOT 4
#define PS_NULL       5

/* Raster operations */
#define SRCCOPY     0x00CC0020
#define SRCPAINT    0x00EE0086
#define SRCAND      0x008800C6
#define SRCINVERT   0x00660046
#define SRCERASE    0x00440328
#define NOTSRCCOPY  0x00330008
#define NOTSRCERASE 0x001100A6
#define MERGECOPY   0x00C000CA
#define MERGEPAINT  0x00BB0226
#define PATCOPY     0x00F00021
#define PATPAINT    0x00FB0A09
#define PATINVERT   0x005A0049
#define DSTINVERT   0x00550009
#define BLACKNESS   0x00000042
#define WHITENESS   0x00FF0062

/* Background modes */
#define TRANSPARENT 1
#define OPAQUE      2

#endif /* WBOX_GDI_TYPES_H */
