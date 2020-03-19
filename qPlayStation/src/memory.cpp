#include "memory.hpp"

memory::memory(bios* b)
{
	BIOS = b;
}

void memory::set32(uint32_t addr, uint32_t value)
{
	PeriphRequestInfo p = getPeriphAtAddress(addr);
	p.periph->set32(p.adjustedAddress, value);
}

uint32_t memory::get32(uint32_t addr)
{
	PeriphRequestInfo p = getPeriphAtAddress(addr);
	return p.periph->get32(p.adjustedAddress);
}

PeriphRequestInfo memory::getPeriphAtAddress(uint32_t addr)
{
	uint8_t segment = addr >> 29; //000 = KUSEG, 100 = KSEG0, 101 = KSEG1, 111 = KSEG2
	addr &= 0x1FFFFFFF;
	if (addr < 0x200000)
	{
		//Main RAM
		logging::fatal("main RAM access", logging::logSource::memory);
	}
	else if (addr >= 0x1FC00000 && addr < 0x1FC00000 + 524288)
	{
		return {BIOS, addr - 0x1FC00000};
	}
	else
	{
		logging::fatal("unimplemented memory location", logging::logSource::memory);
	}
}