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
    EXEInfo exeInfo = { true, 0, 0, 0 };
    if (argc < 2)
    {
        logging::fatal("need BIOS path", logging::logSource::qPS);
    }
    if (argc < 3)
    {
        exeInfo.present = false;
    }

    initSDL();

    bios* BIOS = new bios(args[1]);
    gpu* GPU = new gpu(window);
    memory* Memory = new memory(BIOS, GPU);

    if (exeInfo.present)
    {
        //Load EXE file
        std::ifstream exeFile(args[2], std::ios::in | std::ios::binary | std::ios::ate);
        if (exeFile.is_open())
        {
            int size = (int)exeFile.tellg();
            uint8_t* exeData = new uint8_t[size];
            exeFile.seekg(0, std::ios::beg);
            exeFile.read((char*)exeData, size);
            exeFile.close();
            logging::info("Loaded EXE successfully", logging::logSource::qPS);

            const char correctHeader[] = "PS-X EXE";
            for (int i = 0; i < sizeof(correctHeader); i++)
            {
                if (exeData[i] != correctHeader[i])
                {
                    logging::fatal("EXE has incorrect header", logging::logSource::qPS);
                }
            }

            exeInfo.initialPC = exeData[0x10] | (((uint32_t)exeData[0x11]) << 8) | (((uint32_t)exeData[0x12]) << 16) | (((uint32_t)exeData[0x13]) << 24);
            exeInfo.initialR28 = exeData[0x14] | (((uint32_t)exeData[0x15]) << 8) | (((uint32_t)exeData[0x16]) << 16) | (((uint32_t)exeData[0x17]) << 24);
            uint32_t initialR29R30 = exeData[0x30] | (((uint32_t)exeData[0x31]) << 8) | (((uint32_t)exeData[0x32]) << 16) | (((uint32_t)exeData[0x33]) << 24);
            uint32_t offsetR29R30 = exeData[0x34] | (((uint32_t)exeData[0x35]) << 8) | (((uint32_t)exeData[0x36]) << 16) | (((uint32_t)exeData[0x37]) << 24);
            exeInfo.initialR29R30 = initialR29R30 + offsetR29R30;

            uint32_t destAddress = exeData[0x18] | (((uint32_t)exeData[0x19]) << 8) | (((uint32_t)exeData[0x1A]) << 16) | (((uint32_t)exeData[0x1B]) << 24);
            uint32_t fileSize = exeData[0x1C] | (((uint32_t)exeData[0x1D]) << 8) | (((uint32_t)exeData[0x1E]) << 16) | (((uint32_t)exeData[0x1F]) << 24);

            for (uint32_t i = 0; i < fileSize; i++)
            {
                // this should probably be a memset, but whatever
                Memory->set8(destAddress + i, exeData[0x800 + i]);
            }
            delete[] exeData;
        }
        else
        {
            logging::fatal("unable to load EXE file", logging::logSource::qPS);
        }
    }

    cpu* CPU = new cpu(Memory, exeInfo);

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