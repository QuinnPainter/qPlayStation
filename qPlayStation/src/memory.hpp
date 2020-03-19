#pragma once
#include "helpers.hpp"
#include "bios.hpp"

struct PeriphRequestInfo
{
	peripheral* periph;
	uint32_t adjustedAddress;
};

class memory
{
	public:
		memory(bios* b);
		~memory();
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
	private:
		bios* BIOS = nullptr;
		peripheralStub* pStub = nullptr;
		PeriphRequestInfo getPeriphAtAddress(uint32_t addr);
};