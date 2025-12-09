#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "wbox",
        800,
        600,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
            }
        }

        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
