#pragma once
#include "helpers.hpp"
#include "peripheral.hpp"
#include "ram.hpp"
#include "gpu.hpp"

class dmaChannel
{
	public:
		void setControl(uint32_t value);
		uint32_t getControl();
		void setBase(uint32_t value);
		uint32_t getBase();
		void setBlockCtrl(uint32_t value);
		uint32_t getBlockCtrl();
		bool isActive();

		bool enabled;
		bool trigger; // Triggers the DMA in "Manual" sync mode
		bool direction; // False - Device to RAM | True - RAM to Device
		bool addrMode; // False - Increment address | True - Decrement address
		uint8_t syncMode; // 0 - Manual | 1 - Request | 2 - Linked List
		bool chop;
		uint8_t chopDMAsize;
		uint8_t chopCPUsize;

		uint32_t baseAddr;

		uint16_t blockSize;
		uint16_t blockCount;
};

class dma : public peripheral
{
	public:
		dma(ram* r, gpu* g);
		~dma();
		void reset();
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		ram* RAM;
		gpu* GPU;

		dmaChannel* channels[7];
		uint32_t control;

		uint8_t irq_enable;
		bool irq_masterEnable;
		uint8_t irq_flags;
		bool irq_force;

		void doDMA(uint8_t port);
};

enum class dmaPort : uint8_t
{
	MDECin = 0,
	MDECout = 1,
	GPU = 2,
	CDROM = 3,
	SPU = 4,
	PIO = 5,
	OTC = 6
};