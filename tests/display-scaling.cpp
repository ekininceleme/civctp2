#include "../ctp2_code/ctp/display_scaling.h"
#include <SDL.h>
#include <cassert>
#include <climits>
#include <cstdio>

int main()
{
    auto normal = CalculateDisplayScaling(3024, 1964, 100);
    assert(normal.width == 3024 && normal.height == 1964 && normal.percent == 100);
    auto doubled = CalculateDisplayScaling(3024, 1964, 200);
    assert(doubled.width == 1512 && doubled.height == 982 && doubled.percent == 200);
    auto small = CalculateDisplayScaling(1024, 768, 200);
    assert(small.width >= 800 && small.height >= 600 && small.percent == 125);
    auto wide = CalculateDisplayScaling(1920, 1080, 200);
    assert(wide.width >= 800 && wide.height >= 600 && wide.percent == 175);
    assert(CalculateDisplayScaling(800, 600, 300).percent == 100);
    assert(CalculateDisplayScaling(3024, 1964, INT_MIN).percent == 100);
    assert(CalculateDisplayScaling(3024, 1964, INT_MAX).percent == 300);

    // Test the actual SDL presentation/input transform used by the game.
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    auto window = SDL_CreateWindow("scaling regression", 0, 0, 1600, 1200, SDL_WINDOW_HIDDEN);
    assert(window);
    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    assert(SDL_RenderSetLogicalSize(renderer, 800, 600) == 0);
    float x, y;
    SDL_RenderWindowToLogical(renderer, 1200, 300, &x, &y);
    assert(x == 600 && y == 150);
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    assert(width == 1600 && height == 1200); // scale did not shrink the output

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Rect logicalRect = {100, 100, 100, 100};
    SDL_RenderFillRect(renderer, &logicalRect);
    SDL_Rect physicalPixel = {250, 250, 1, 1};
    Uint32 pixel = 0;
    assert(SDL_RenderReadPixels(renderer, &physicalPixel, SDL_PIXELFORMAT_ARGB8888, &pixel, 4) == 0);
    assert((pixel & 0xffffff) == 0xff0000); // enlarged rectangle covers this pixel

    SDL_Event click = {};
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.windowID = SDL_GetWindowID(window);
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = 1200;
    click.button.y = 300;
    int capturedX = -1, capturedY = -1;
    struct Capture { int *x; int *y; } capture = {&capturedX, &capturedY};
    auto watch = [](void *data, SDL_Event *event) -> int {
        if (event->type == SDL_MOUSEBUTTONDOWN) {
            Capture *capture = static_cast<Capture *>(data);
            *capture->x = event->button.x;
            *capture->y = event->button.y;
        }
        return 0;
    };
    SDL_AddEventWatch(watch, &capture);
    assert(SDL_PushEvent(&click) == 1);
    SDL_DelEventWatch(watch, &capture);
    assert(capturedX == 600 && capturedY == 150);

    // A nonmatching aspect ratio must letterbox, not stretch or offset clicks.
    assert(SDL_RenderSetLogicalSize(renderer, 800, 400) == 0);
    SDL_RenderWindowToLogical(renderer, 800, 600, &x, &y);
    assert(x == 400 && y == 200);
    SDL_RenderWindowToLogical(renderer, 0, 200, &x, &y);
    assert(x == 0 && y == 0);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("Display scaling: sizing, minimum canvas, SDL rendering and input mapping passed");
}
