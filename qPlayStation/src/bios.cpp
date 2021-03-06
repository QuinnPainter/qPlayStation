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
    patchBIOSforTTY();
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

void bios::set16(uint32_t addr, uint16_t value)
{
    logging::fatal("attempted to set BIOS");
}

uint16_t bios::get16(uint32_t addr) 
{
    return biosData[addr] | (biosData[addr + 1] << 8);
}

void bios::set8(uint32_t addr, uint8_t value)
{
    logging::fatal("attempted to set BIOS");
}

uint8_t bios::get8(uint32_t addr)
{
    return biosData[addr];
}

void bios::patchBIOS(uint32_t addr, uint32_t value)
{
    addr -= 0x1FC00000;
    biosData[addr] = value & 0xFF;
    biosData[addr + 1] = (value >> 8) & 0xFF;
    biosData[addr + 2] = (value >> 16) & 0xFF;
    biosData[addr + 3] = (value >> 24) & 0xFF;
}

void bios::patchBIOSforTTY()
{
    logging::info("Patching BIOS to enable TTY", logging::logSource::BIOS);
    patchBIOS(0x1FC06F0C, 0x24010001); // Taken from duckstation
    patchBIOS(0x1FC06F14, 0xAF81A9C0);
}