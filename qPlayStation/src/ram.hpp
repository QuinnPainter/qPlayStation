#pragma once
#include "helpers.hpp"
#include "peripheral.hpp"

class ram : public peripheral
{
	public:
		ram();
		~ram();
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		uint8_t* ramData = nullptr;
};

class scratchpad : public peripheral
{
	public:
		scratchpad();
		~scratchpad();
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		uint8_t* scratchpadData = nullptr;
};