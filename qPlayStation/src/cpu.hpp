#pragma once
#include "helpers.hpp"
#include "memory.hpp"

class cpu
{
	public:
		cpu(memory* mem);
		void reset();
		void step();
	private:
		uint32_t pc;
		uint32_t regs[32];
		memory* Memory = nullptr;
		void executeInstr(uint32_t instr);
};