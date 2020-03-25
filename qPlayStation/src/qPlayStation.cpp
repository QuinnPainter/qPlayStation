#include "qPlayStation.hpp"

SDL_Window* window;

void initSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
    window = SDL_CreateWindow("qPlayStation", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 512, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if (window == NULL)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
}

// Arg 1 = BIOS path, Arg 2 = Game Path
int main(int argc, char* args[])
{
    if (argc < 2)
    {
        logging::fatal("need BIOS path", logging::logSource::qPS);
    }
    /*if (argc < 3) //only need BIOS for now
    {
        logging::fatal("need game path", logging::logSource::qPS);
    }*/

    initSDL();

    bios* BIOS = new bios(args[1]);
    gpu* GPU = new gpu(window);
    memory* Memory = new memory(BIOS, GPU);
    cpu* CPU = new cpu(Memory);

    int exitCode = 0;

    try
    {
        SDL_Event event;
        bool running = true;
        while (running)
        {
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_QUIT: running = false; break;
                }
            }

            // This loop is 60FPS, and the PS CPU runs at approx 30 MIPS
            // So it should run 0.5 Million Instructions per frame (500,000)
            for (uint32_t i = 0; i < 500000; i++)
            {
                CPU->step();
            }

            GPU->display();
            //SDL_Delay(13);
        }
    }
    catch (int e)
    {
        //just continue to the cleanup
        exitCode = 1;
    }

    delete(BIOS);
    delete(GPU);
    delete(Memory);
    delete(CPU);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}