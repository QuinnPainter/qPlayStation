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
		uint32_t hi;
		uint32_t lo;
		memory* Memory = nullptr;
		void executeInstr(uint32_t instr);
		void setReg(int index, uint32_t value);
		uint32_t getReg(int index);

		// Instruction Decode Helpers
		uint8_t decode_op(uint32_t instr);
		uint8_t decode_rs(uint32_t instr);
		uint8_t decode_rt(uint32_t instr);
		uint8_t decode_rd(uint32_t instr);
		uint16_t decode_imm(uint32_t instr);
		uint8_t decode_shamt(uint32_t instr);
		uint8_t decode_funct(uint32_t instr);
		uint32_t decode_target(uint32_t instr);

		// Instructions
		void op_lui(uint32_t instr);
		void op_ori(uint32_t instr);
		void op_sw(uint32_t instr);
		void op_sll(uint32_t instr);
		void op_addiu(uint32_t instr);
};