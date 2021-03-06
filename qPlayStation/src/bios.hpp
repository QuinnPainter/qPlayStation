#pragma once
#include "helpers.hpp"
#include "peripheral.hpp"

class bios : public peripheral
{
	public:
		bios(char* biosPath);
		~bios();
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
		void patchBIOS(uint32_t addr, uint32_t value);
		void patchBIOSforTTY();
	private:
		uint8_t* biosData = nullptr;
};