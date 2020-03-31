#pragma once
#include "helpers.hpp"
#include "bios.hpp"
#include "ram.hpp"
#include "dma.hpp"
#include "gpu.hpp"
#include "interrupt.hpp"
#include "cdrom.hpp"

struct PeriphRequestInfo
{
	peripheral* periph;
	uint32_t adjustedAddress;
};

class tty : public peripheral
{
	private:
		std::string buffer = "";
	public:
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
};

class memory
{
	public:
		memory(bios* b, gpu* g, interruptController* i, cdrom* c);
		~memory();
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		bios* BIOS;
		ram* RAM;
		scratchpad* Scratchpad;
		dma* DMA;
		gpu* GPU;
		tty* TTY;
		cdrom* CDROM;
		interruptController* InterruptController;
		peripheralStub* pStub;
		PeriphRequestInfo getPeriphAtAddress(uint32_t addr);
};