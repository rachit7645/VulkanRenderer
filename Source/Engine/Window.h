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
        bool PollEvents();

        // SDL window handle
        SDL_Window* handle = nullptr;
    private:
        // Window size
        glm::ivec2 m_size = {1280, 768};
        // SDL event
        SDL_Event m_event = {};
    };
}

#endif
