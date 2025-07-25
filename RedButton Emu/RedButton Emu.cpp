#define NOMINMAX
#include <windows.h>
#include <SDL.h>
#include <SDL_image.h>

#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[])
{
    // Init SDL video + audio
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << "\n";
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Load and open WAV
    SDL_AudioSpec wavSpec;
    Uint8* wavBuf = NULL;
    Uint32 wavLen = 0;
    if (!SDL_LoadWAV("Contents/SFX/Click.wav", &wavSpec, &wavBuf, &wavLen)) {
        std::cerr << "SDL_LoadWAV failed: " << SDL_GetError() << "\n";
        IMG_Quit(); SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(NULL, 0, &wavSpec, NULL, 0);
    if (audioDev == 0) {
        std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << "\n";
        SDL_FreeWAV(wavBuf); IMG_Quit(); SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_PauseAudioDevice(audioDev, 1);

    // Load button images *and keep one surface for pixel‑sampling*
    const std::string path = "Contents/RedButton/";
    SDL_Surface* surfUp = IMG_Load((path + "Button Not Pressed.png").c_str());
    SDL_Surface* surfDown = IMG_Load((path + "Button Pressed.png").c_str());
    if (!surfUp || !surfDown) {
        std::cerr << "IMG_Load failed: " << IMG_GetError() << "\n";
        SDL_CloseAudioDevice(audioDev);
        SDL_FreeWAV(wavBuf);
        IMG_Quit(); SDL_Quit();
        return EXIT_FAILURE;
    }
    // convert to known RGBA32 format for easy SDL_GetRGBA()
    SDL_Surface* mask = SDL_ConvertSurfaceFormat(surfUp,
        SDL_PIXELFORMAT_RGBA32, 0);
    if (!mask) {
        std::cerr << "ConvertSurfaceFormat failed: " << SDL_GetError() << "\n";
        SDL_FreeSurface(surfUp);
        SDL_FreeSurface(surfDown);
        SDL_CloseAudioDevice(audioDev);
        SDL_FreeWAV(wavBuf);
        IMG_Quit(); SDL_Quit();
        return EXIT_FAILURE;
    }

    int w = mask->w, h = mask->h;

    // Create window + renderer
    SDL_Window* win = SDL_CreateWindow("RedButton Emu",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_SHOWN);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!win || !ren) {
        std::cerr << "Window/Renderer error: " << SDL_GetError() << "\n";
        SDL_FreeSurface(surfUp);
        SDL_FreeSurface(surfDown);
        SDL_FreeSurface(mask);
        SDL_CloseAudioDevice(audioDev);
        SDL_FreeWAV(wavBuf);
        IMG_Quit(); SDL_Quit();
        return EXIT_FAILURE;
    }

    // Textures for up/down states
    SDL_Texture* texUp = SDL_CreateTextureFromSurface(ren, surfUp);
    SDL_Texture* texDown = SDL_CreateTextureFromSurface(ren, surfDown);
    SDL_FreeSurface(surfUp);
    SDL_FreeSurface(surfDown);
    if (!texUp || !texDown) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << "\n";
        if (texUp)   SDL_DestroyTexture(texUp);
        if (texDown) SDL_DestroyTexture(texDown);
        SDL_FreeSurface(mask);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_CloseAudioDevice(audioDev);
        SDL_FreeWAV(wavBuf);
        IMG_Quit(); SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Rect dst = { 0, 0, w, h };
    SDL_Texture* current = texUp;
    bool running = true;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN &&
                e.button.button == SDL_BUTTON_LEFT)
            {
                int mx = e.button.x;
                int my = e.button.y;
                if (mx >= 0 && mx < w && my >= 0 && my < h) {
                    // sample the pixel at (mx,my)
                    Uint32* pixels = (Uint32*)mask->pixels;
                    Uint32 pixel = pixels[my * w + mx];
                    Uint8 r, g, b, a;
                    SDL_GetRGBA(pixel, mask->format, &r, &g, &b, &a);

                    // only accept 'mostly red' pixels
                    if (r > 200 && g < 80 && b < 80) {
                        // show pressed state
                        current = texDown;
                        SDL_RenderClear(ren);
                        SDL_RenderCopy(ren, current, NULL, &dst);
                        SDL_RenderPresent(ren);

                        // play click
                        SDL_PauseAudioDevice(audioDev, 1);
                        SDL_ClearQueuedAudio(audioDev);
                        SDL_QueueAudio(audioDev, wavBuf, wavLen);
                        SDL_PauseAudioDevice(audioDev, 0);

                        // send Win32 message
                        HWND hwnd = FindWindowA("RED_BUTTON_Window", NULL);
                        if (hwnd) SendMessageA(hwnd, 0x8004, 1, 0);

                        SDL_Delay(200);
                        current = texUp;
                    }
                }
            }
        }

        // draw current
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, current, NULL, &dst);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    // cleanup
    SDL_FreeSurface(mask);
    SDL_DestroyTexture(texDown);
    SDL_DestroyTexture(texUp);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_CloseAudioDevice(audioDev);
    SDL_FreeWAV(wavBuf);
    IMG_Quit();
    SDL_Quit();
    return EXIT_SUCCESS;
}
