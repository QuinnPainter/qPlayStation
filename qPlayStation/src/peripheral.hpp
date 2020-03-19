#pragma once
#include "helpers.hpp"

class peripheral
{
	public:
		virtual void set32(uint32_t addr, uint32_t value) = 0;
		virtual uint32_t get32(uint32_t addr) = 0;
		virtual void set16(uint32_t addr, uint16_t value) = 0;
		virtual uint16_t get16(uint32_t addr) = 0;
		virtual void set8(uint32_t addr, uint8_t value) = 0;
		virtual uint8_t get8(uint32_t addr) = 0;
};

//Empty peripheral class, used for stubbing out peripherals
class peripheralStub: public peripheral
{
	public:
		void set32(uint32_t addr, uint32_t value) {}
		uint32_t get32(uint32_t addr) { return 0; }
		void set16(uint32_t addr, uint16_t value) {}
		uint16_t get16(uint32_t addr) { return 0; }
		void set8(uint32_t addr, uint8_t value) {}
		uint8_t get8(uint32_t addr) { return 0; }
};