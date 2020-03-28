#include "interrupt.hpp"
#include "cpu.hpp" // solve circular dependency

interruptController::interruptController()
{
	InterruptStatus = 0;
	InterruptMask = 0;
}

void interruptController::giveCpuRef(cpu* c)
{
	CPU = c; // this is a bit messy, but it's the best I can think of
}

void interruptController::requestInterrupt(interruptType type)
{
	InterruptStatus |= 1 << (uint32_t)type;
	checkForInterrupts();
}

void interruptController::checkForInterrupts()
{
	CPU->updateInterruptRequest((InterruptStatus & InterruptMask) != 0);
}

void interruptController::set32(uint32_t addr, uint32_t value)
{
	switch (addr)
	{
		case 0: // I_STAT - Interrupt status register
		{
			InterruptStatus &= value; // Acknowledge interrupts
			break;
		}
		case 4: // I_MASK - Interrupt mask register
		{
			InterruptMask = value;
			break;
		}
	}
	checkForInterrupts();
}

uint32_t interruptController::get32(uint32_t addr)
{
	switch (addr)
	{
		case 0: return InterruptStatus;
		case 4: return InterruptMask;
	}
}

void interruptController::set16(uint32_t addr, uint16_t value)
{
	set32(addr, value);
}

uint16_t interruptController::get16(uint32_t addr)
{
	return (uint16_t)get32(addr);
}

void interruptController::set8(uint32_t addr, uint8_t value) { logging::fatal("unhandled 8 bit write to Interrupt Ctrl: " + helpers::intToHex(addr), logging::logSource::memory); }
uint8_t interruptController::get8(uint32_t addr) { logging::fatal("unhandled 8 bit read from Interrupt Ctrl: " + helpers::intToHex(addr), logging::logSource::memory); return 0; }