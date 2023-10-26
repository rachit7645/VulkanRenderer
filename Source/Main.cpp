#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "Util/Util.h"
#include "Engine/AppInstance.h"

int main(UNUSED int argc, UNUSED char** argv)
{
    // Create and run app
    Engine::AppInstance().Run();
    // Exit successfully
    return 0;
}