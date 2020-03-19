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
	private:
		uint8_t* biosData = nullptr;
};