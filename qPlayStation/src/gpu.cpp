#include "gpu.hpp"

void gpu::set32(uint32_t addr, uint32_t value)
{
	switch (addr)
	{
		case 0:
		{
			logging::info("GP0 write: " + helpers::intToHex(value), logging::logSource::GPU);
			break;
		}
		case 4:
		{
			logging::info("GP1 write: " + helpers::intToHex(value), logging::logSource::GPU);
			break;
		}
	}
}

uint32_t gpu::get32(uint32_t addr)
{
	switch (addr)
	{
		case 0:
		{
			logging::info("GPUREAD", logging::logSource::GPU);
			return 0;
		}
		case 4:
		{
			logging::info("GPUSTAT", logging::logSource::GPU);
			return 0x1C000000;
		}
	}
}

void gpu::set16(uint32_t addr, uint16_t value) { logging::fatal("unimplemented 16 bit GPU write" + helpers::intToHex(addr), logging::logSource::GPU); }
uint16_t gpu::get16(uint32_t addr) { logging::fatal("unimplemented 16 bit GPU read" + helpers::intToHex(addr), logging::logSource::GPU); return 0; }
void gpu::set8(uint32_t addr, uint8_t value) { logging::fatal("unimplemented 8 bit GPU write " + helpers::intToHex(addr), logging::logSource::GPU); }
uint8_t gpu::get8(uint32_t addr) { logging::fatal("unimplemented 8 bit GPU read" + helpers::intToHex(addr), logging::logSource::GPU); return 0; }