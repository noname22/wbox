/*
 * WBOX Display Manager
 * SDL3-based virtual desktop implementation
 */
#include "display.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int display_init(display_context_t *ctx, int width, int height, const char *title)
{
    memset(ctx, 0, sizeof(*ctx));

    /* Initialize SDL video subsystem */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "display: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    /* Create window */
    SDL_Window *window = SDL_CreateWindow(
        title ? title : "WBOX",
        width, height,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "display: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    ctx->window = window;

    /* Create renderer */
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "display: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    ctx->renderer = renderer;

    /* Create texture for frame buffer */
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height
    );
    if (!texture) {
        fprintf(stderr, "display: SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    ctx->texture = texture;

    /* Allocate frame buffer */
    ctx->width = width;
    ctx->height = height;
    ctx->pitch = width * sizeof(uint32_t);
    ctx->pixels = (uint32_t *)calloc(width * height, sizeof(uint32_t));
    if (!ctx->pixels) {
        fprintf(stderr, "display: Failed to allocate frame buffer\n");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    /* Fill with default background (Windows desktop color) */
    display_clear(ctx, 0xFF3A6EA5);  /* Classic Windows blue */

    ctx->initialized = true;
    ctx->dirty = true;

    printf("Display initialized: %dx%d\n", width, height);
    return 0;
}

void display_shutdown(display_context_t *ctx)
{
    if (!ctx->initialized) return;

    if (ctx->pixels) {
        free(ctx->pixels);
        ctx->pixels = NULL;
    }
    if (ctx->texture) {
        SDL_DestroyTexture((SDL_Texture *)ctx->texture);
        ctx->texture = NULL;
    }
    if (ctx->renderer) {
        SDL_DestroyRenderer((SDL_Renderer *)ctx->renderer);
        ctx->renderer = NULL;
    }
    if (ctx->window) {
        SDL_DestroyWindow((SDL_Window *)ctx->window);
        ctx->window = NULL;
    }
    SDL_Quit();
    ctx->initialized = false;
}

void display_present(display_context_t *ctx)
{
    if (!ctx->initialized || !ctx->dirty) return;

    SDL_Texture *texture = (SDL_Texture *)ctx->texture;
    SDL_Renderer *renderer = (SDL_Renderer *)ctx->renderer;

    /* Update texture with frame buffer contents */
    SDL_UpdateTexture(texture, NULL, ctx->pixels, ctx->pitch);

    /* Clear and copy texture to screen */
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    ctx->dirty = false;
}

bool display_poll_events(display_context_t *ctx)
{
    if (!ctx->initialized) return true;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                ctx->quit_requested = true;
                return true;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    ctx->quit_requested = true;
                    return true;
                }
                /* TODO: Route keyboard events to message queue */
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_MOTION:
                /* TODO: Route mouse events to message queue */
                break;

            case SDL_EVENT_WINDOW_EXPOSED:
                ctx->dirty = true;
                break;
        }
    }

    return ctx->quit_requested;
}

void display_fill_rect(display_context_t *ctx, int x, int y, int w, int h, uint32_t color)
{
    if (!ctx->initialized || !ctx->pixels) return;

    /* Clip to display bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ctx->width) w = ctx->width - x;
    if (y + h > ctx->height) h = ctx->height - y;
    if (w <= 0 || h <= 0) return;

    /* Fill rectangle */
    for (int row = y; row < y + h; row++) {
        uint32_t *dst = ctx->pixels + row * ctx->width + x;
        for (int col = 0; col < w; col++) {
            *dst++ = color;
        }
    }

    ctx->dirty = true;
}

void display_clear(display_context_t *ctx, uint32_t color)
{
    if (!ctx->initialized || !ctx->pixels) return;

    int count = ctx->width * ctx->height;
    for (int i = 0; i < count; i++) {
        ctx->pixels[i] = color;
    }

    ctx->dirty = true;
}

uint32_t display_get_pixel(display_context_t *ctx, int x, int y)
{
    if (!ctx->initialized || !ctx->pixels) return 0;
    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) return 0;

    return ctx->pixels[y * ctx->width + x];
}

void display_set_pixel(display_context_t *ctx, int x, int y, uint32_t color)
{
    if (!ctx->initialized || !ctx->pixels) return;
    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) return;

    ctx->pixels[y * ctx->width + x] = color;
    ctx->dirty = true;
}

void display_invalidate(display_context_t *ctx)
{
    if (ctx->initialized) {
        ctx->dirty = true;
    }
}

int display_get_width(display_context_t *ctx)
{
    return ctx->initialized ? ctx->width : 0;
}

int display_get_height(display_context_t *ctx)
{
    return ctx->initialized ? ctx->height : 0;
}
