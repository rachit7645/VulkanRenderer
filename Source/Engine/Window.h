#ifndef WINDOW_H
#define WINDOW_H

#include <SDL2/SDL.h>

#include "../Externals/GLM.h"

namespace Engine
{
    class Window
    {
    public:
        // Create window using SDL
        Window();
        // Destroy window
        ~Window();

        // Function to process SDL Events
        [[nodiscard]] bool PollEvents();
        // Sleeps till window is restored
        void WaitForRestoration();

        // SDL window handle
        SDL_Window* handle = nullptr;
        // Window size
        glm::ivec2 size = {1280, 768};

        // Was window resized?
        bool wasResized = false;
    private:
        // Scale window resolution according to aspect ratio
        void ScaleWindowSize();

        // SDL event
        SDL_Event m_event = {};
    };
}

#endif
