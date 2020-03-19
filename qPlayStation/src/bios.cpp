#include "bios.hpp"

bios::bios(char* biosPath)
{
    std::ifstream biosFile(biosPath, std::ios::in | std::ios::binary | std::ios::ate);
    if (biosFile.is_open())
    {
        int size = biosFile.tellg();
        if (size != 524288)
        {
            logging::fatal("incorrect BIOS file size", logging::logSource::BIOS);
        }
        biosData = new uint8_t[size];
        biosFile.seekg(0, std::ios::beg);
        biosFile.read((char*)biosData, size);
        biosFile.close();
        logging::info("Loaded BIOS successfully", logging::logSource::BIOS);
    }
    else
    {
        logging::fatal("unable to load BIOS file", logging::logSource::BIOS);
    }
}

bios::~bios()
{
    delete[] biosData;
}

void bios::set32(uint32_t addr, uint32_t value)
{
    logging::fatal("attempted to set BIOS");
}

uint32_t bios::get32(uint32_t addr)
{
    return biosData[addr] | (biosData[addr + 1] << 8) | (biosData[addr + 2] << 16) | (biosData[addr + 3] << 24);
}