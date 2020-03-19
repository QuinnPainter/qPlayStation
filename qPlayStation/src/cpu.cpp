#include "cpu.hpp"

cpu::cpu(memory* mem)
{
	Memory = mem;
	reset();
}

void cpu::reset()
{
	pc = 0xBFC00000;
}

void cpu::step()
{
	uint32_t instr = Memory->get32(pc);
	executeInstr(instr);
	pc += 4;
}

void cpu::executeInstr(uint32_t instr)
{
	logging::fatal("Instr " + helpers::intToHex(instr), logging::logSource::CPU);
}