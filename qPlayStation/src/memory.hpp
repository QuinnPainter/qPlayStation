#pragma once
#include "helpers.hpp"
#include "bios.hpp"
#include "ram.hpp"

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
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		bios* BIOS = nullptr;
		ram* RAM = nullptr;
		peripheralStub* pStub = nullptr;
		PeriphRequestInfo getPeriphAtAddress(uint32_t addr);
};