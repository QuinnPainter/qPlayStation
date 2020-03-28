#pragma once
#include "helpers.hpp"
#include "peripheral.hpp"
class cpu; // forward declare instead of include to solve circular dependency

enum class interruptType : uint32_t
{
	VBLANK = 0,
	GPU = 1,
	CDROM = 2,
	DMA = 3,
	TMR0 = 4,
	TMR1 = 5,
	TMR2 = 6,
	CONTROLLERMEMCARD = 7,
	SIO = 8,
	SPU = 9,
	LIGHTPEN = 10
};

class interruptController : public peripheral
{
	public:
		interruptController();
		void giveCpuRef(cpu* c);
		void requestInterrupt(interruptType type);
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		cpu* CPU;
		uint32_t InterruptStatus;
		uint32_t InterruptMask;
		void checkForInterrupts();
};