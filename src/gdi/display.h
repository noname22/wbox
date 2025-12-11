/*
 * WBOX Display Manager
 * SDL3-based virtual desktop for GUI applications
 */
#ifndef WBOX_DISPLAY_H
#define WBOX_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "gdi_types.h"

/* Default display dimensions */
#define DISPLAY_DEFAULT_WIDTH   800
#define DISPLAY_DEFAULT_HEIGHT  600

/* Display context - manages SDL3 window and frame buffer */
typedef struct {
    /* SDL handles (opaque to avoid SDL header dependency) */
    void *window;       /* SDL_Window* */
    void *renderer;     /* SDL_Renderer* */
    void *texture;      /* SDL_Texture* */

    /* Frame buffer */
    uint32_t *pixels;   /* Host-side pixel buffer (ARGB8888) */
    int width;
    int height;
    int pitch;          /* Bytes per row */

    /* State */
    bool initialized;
    bool dirty;         /* Needs redraw */
    bool quit_requested;
} display_context_t;

/*
 * Initialize the display subsystem
 * Creates SDL3 window and frame buffer
 * Returns 0 on success, -1 on failure
 */
int display_init(display_context_t *ctx, int width, int height, const char *title);

/*
 * Shutdown the display subsystem
 * Frees SDL resources and frame buffer
 */
void display_shutdown(display_context_t *ctx);

/*
 * Present the frame buffer to screen
 * Copies pixels to SDL texture and renders
 */
void display_present(display_context_t *ctx);

/*
 * Process SDL events
 * Returns true if quit was requested
 */
bool display_poll_events(display_context_t *ctx);

/*
 * Fill a rectangle with a solid color
 */
void display_fill_rect(display_context_t *ctx, int x, int y, int w, int h, uint32_t color);

/*
 * Clear the display with a color
 */
void display_clear(display_context_t *ctx, uint32_t color);

/*
 * Get a pixel value at (x, y)
 */
uint32_t display_get_pixel(display_context_t *ctx, int x, int y);

/*
 * Set a pixel value at (x, y)
 */
void display_set_pixel(display_context_t *ctx, int x, int y, uint32_t color);

/*
 * Mark the display as needing redraw
 */
void display_invalidate(display_context_t *ctx);

/*
 * Get display dimensions
 */
int display_get_width(display_context_t *ctx);
int display_get_height(display_context_t *ctx);

#endif /* WBOX_DISPLAY_H */
