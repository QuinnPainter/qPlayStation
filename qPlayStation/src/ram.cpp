#include "ram.hpp"

ram::ram()
{
    ramData = new uint8_t[2 * 1024 * 1024];
    std::memset(ramData, 0, 2 * 1024 * 1024);
}

ram::~ram()
{
    delete[] ramData;
}

void ram::set32(uint32_t addr, uint32_t value)
{
    ramData[addr] = value & 0xFF;
    ramData[addr + 1] = (value >> 8) & 0xFF;
    ramData[addr + 2] = (value >> 16) & 0xFF;
    ramData[addr + 3] = (value >> 24) & 0xFF;
}

uint32_t ram::get32(uint32_t addr)
{
    return ramData[addr] | (ramData[addr + 1] << 8) | (ramData[addr + 2] << 16) | (ramData[addr + 3] << 24);
}

void ram::set16(uint32_t addr, uint16_t value)
{
    ramData[addr] = value & 0xFF;
    ramData[addr + 1] = (value >> 8) & 0xFF;
}

uint16_t ram::get16(uint32_t addr)
{
    return ramData[addr] | (ramData[addr + 1] << 8);
}

void ram::set8(uint32_t addr, uint8_t value)
{
    ramData[addr] = value;
}

uint8_t ram::get8(uint32_t addr)
{
    return ramData[addr];
}