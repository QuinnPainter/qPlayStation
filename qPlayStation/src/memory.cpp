#include "memory.hpp"

memory::memory(bios* b)
{
	BIOS = b;
	RAM = new ram();
	pStub = new peripheralStub();
}

memory::~memory()
{
	delete(RAM);
	delete(pStub);
}

void memory::set32(uint32_t addr, uint32_t value)
{
	if (!helpers::is32BitAligned(addr))
	{
		logging::fatal("Misaligned 32-bit store address", logging::logSource::memory);
	}

	PeriphRequestInfo p = getPeriphAtAddress(addr);
	p.periph->set32(p.adjustedAddress, value);
}

uint32_t memory::get32(uint32_t addr)
{
	if (!helpers::is32BitAligned(addr))
	{
		logging::fatal("Misaligned 32-bit load address", logging::logSource::memory);
	}

	PeriphRequestInfo p = getPeriphAtAddress(addr);
	return p.periph->get32(p.adjustedAddress);
}

void memory::set16(uint32_t addr, uint16_t value)
{
	if (!helpers::is16BitAligned(addr))
	{
		logging::fatal("Misaligned 16-bit store address", logging::logSource::memory);
	}

	PeriphRequestInfo p = getPeriphAtAddress(addr);
	p.periph->set16(p.adjustedAddress, value);
}

uint16_t memory::get16(uint32_t addr)
{
	if (!helpers::is16BitAligned(addr))
	{
		logging::fatal("Misaligned 16-bit load address", logging::logSource::memory);
	}

	PeriphRequestInfo p = getPeriphAtAddress(addr);
	return p.periph->get16(p.adjustedAddress);
}

void memory::set8(uint32_t addr, uint8_t value)
{
	PeriphRequestInfo p = getPeriphAtAddress(addr);
	p.periph->set8(p.adjustedAddress, value);
}

uint8_t memory::get8(uint32_t addr)
{
	PeriphRequestInfo p = getPeriphAtAddress(addr);
	return p.periph->get8(p.adjustedAddress);
}

PeriphRequestInfo memory::getPeriphAtAddress(uint32_t addr)
{
	uint8_t segment = addr >> 29; //000 = KUSEG, 100 = KSEG0, 101 = KSEG1, 111 = KSEG2
	uint32_t adjAddr = addr & 0x1FFFFFFF;
	if (adjAddr < 0x200000)
	{
		// Main RAM
		return {RAM, adjAddr};
	}
	else if (adjAddr >= 0x1F000000 && adjAddr < 0x1F800000)
	{
		// Expansion 1
		return {pStub, 0};
	}
	else if (adjAddr >= 0x1F801000 && adjAddr < 0x1F803000)
	{
		// IO / Expansion Area
		//logging::info("IO Access: " + helpers::intToHex(addr), logging::logSource::memory);
		return {pStub, 0};
	}
	else if (addr == 0xFFFE0130)
	{
		// Cache Control
		logging::info("Cache Control Access", logging::logSource::memory);
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