#pragma once
#include "helpers.hpp"
#include "memory.hpp"

struct pendingLoad
{
	uint8_t regIndex;
	uint32_t value;
};

class cpu
{
	public:
		cpu(memory* mem);
		void reset();
		void step();
	private:
		uint32_t pc;
		uint32_t in_regs[32];
		uint32_t out_regs[32];
		uint32_t hi;
		uint32_t lo;
		uint32_t next_instr = 0;
		pendingLoad currentLoad = {0, 0};
		memory* Memory = nullptr;
		void executeInstr(uint32_t instr);
		void setReg(int index, uint32_t value);
		uint32_t getReg(int index);

		void branch(uint32_t offset);

		// Instruction Decode Helpers
		uint8_t decode_op(uint32_t instr);
		uint8_t decode_rs(uint32_t instr);
		uint8_t decode_rt(uint32_t instr);
		uint8_t decode_rd(uint32_t instr);
		uint16_t decode_imm(uint32_t instr);
		uint32_t decode_imm_se(uint32_t instr);
		uint8_t decode_shamt(uint32_t instr);
		uint8_t decode_funct(uint32_t instr);
		uint32_t decode_target(uint32_t instr);

		// Instructions
		void op_lui(uint32_t instr);
		void op_ori(uint32_t instr);
		void op_andi(uint32_t instr);
		void op_sw(uint32_t instr);
		void op_sll(uint32_t instr);
		void op_addi(uint32_t instr);
		void op_addiu(uint32_t instr);
		void op_j(uint32_t instr);
		void op_jal(uint32_t instr);
		void op_or(uint32_t instr);
		void op_and(uint32_t instr);
		void op_cop0(uint32_t instr);
		void op_bne(uint32_t instr);
		void op_beq(uint32_t instr);
		void op_lw(uint32_t instr);
		void op_sltu(uint32_t instr);
		void op_addu(uint32_t instr);
		void op_add(uint32_t instr);
		void op_sh(uint32_t instr);
		void op_sb(uint32_t instr);
		void op_jr(uint32_t instr);
		void op_lb(uint32_t instr);
};