#include "memory.hpp"

memory::memory(bios* b)
{
	BIOS = b;
	pStub = new peripheralStub();
}

memory::~memory()
{
	delete(pStub);
}

void memory::set32(uint32_t addr, uint32_t value)
{
	if ((addr & 0x3) != 0)
	{
		logging::fatal("Misaligned 32-bit store address", logging::logSource::memory);
	}

	PeriphRequestInfo p = getPeriphAtAddress(addr);
	p.periph->set32(p.adjustedAddress, value);
}

uint32_t memory::get32(uint32_t addr)
{
	if ((addr & 0x3) != 0)
	{
		logging::fatal("Misaligned 32-bit load address", logging::logSource::memory);
	}

	PeriphRequestInfo p = getPeriphAtAddress(addr);
	return p.periph->get32(p.adjustedAddress);
}

PeriphRequestInfo memory::getPeriphAtAddress(uint32_t addr)
{
	uint8_t segment = addr >> 29; //000 = KUSEG, 100 = KSEG0, 101 = KSEG1, 111 = KSEG2
	uint32_t adjAddr = addr & 0x1FFFFFFF;
	if (adjAddr < 0x200000)
	{
		//Main RAM
		logging::fatal("main RAM access", logging::logSource::memory);
	}
	else if (adjAddr >= 0x1F801000 && adjAddr < 0x1F803000)
	{
		//IO / Expansion Area
		logging::info("IO Access: " + helpers::intToHex(addr), logging::logSource::memory);
		return {pStub, 0};
	}
	else if (adjAddr >= 0x1FC00000 && adjAddr < 0x1FC00000 + 524288)
	{
		return {BIOS, adjAddr - 0x1FC00000};
	}
	else
	{
		logging::fatal("unimplemented memory location: " + helpers::intToHex(addr), logging::logSource::memory);
	}
}