#pragma once
#include "helpers.hpp"
#include "memory.hpp"

struct pendingLoad
{
	uint8_t regIndex;
	uint32_t value;
};

enum class psException : uint32_t
{
	Interrupt = 0x0,
	AddrErrorLoad = 0x4,
	AddrErrorStore = 0x5,
	BusErrorInstrFetch = 0x6,
	BusErrorData = 0x7,
	Syscall = 0x8,
	Breakpoint = 0x9,
	ReservedInstr = 0xA,
	CoprocessorUnusable = 0xB,
	ArithOverflow = 0xC
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
		uint32_t next_pc;
		pendingLoad currentLoad = {0, 0};
		uint32_t current_pc;
		bool is_branch;
		bool delay_slot;
		uint32_t cop0_sr;
		uint32_t cop0_cause;
		uint32_t cop0_epc;
		bool cacheIsolated();
		memory* Memory = nullptr;
		void executeInstr(uint32_t instr);
		void setReg(int index, uint32_t value);
		uint32_t getReg(int index);

		void branch(uint32_t offset);
		void exception(psException exceptionType);

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
		void op_xori(uint32_t instr);
		void op_sll(uint32_t instr);
		void op_srl(uint32_t instr);
		void op_sra(uint32_t instr);
		void op_sllv(uint32_t instr);
		void op_srlv(uint32_t instr);
		void op_srav(uint32_t instr);
		void op_addi(uint32_t instr);
		void op_addiu(uint32_t instr);
		void op_j(uint32_t instr);
		void op_jal(uint32_t instr);
		void op_jr(uint32_t instr);
		void op_jalr(uint32_t instr);
		void op_or(uint32_t instr);
		void op_and(uint32_t instr);
		void op_xor(uint32_t instr);
		void op_nor(uint32_t instr);
		void op_cop0(uint32_t instr);
		void op_cop1(uint32_t instr);
		void op_cop2(uint32_t instr);
		void op_cop3(uint32_t instr);
		void op_lwc0(uint32_t instr);
		void op_lwc1(uint32_t instr);
		void op_lwc2(uint32_t instr);
		void op_lwc3(uint32_t instr);
		void op_swc0(uint32_t instr);
		void op_swc1(uint32_t instr);
		void op_swc2(uint32_t instr);
		void op_swc3(uint32_t instr);
		void op_bne(uint32_t instr);
		void op_beq(uint32_t instr);
		void op_bgtz(uint32_t instr);
		void op_blez(uint32_t instr);
		void op_bcondz(uint32_t instr);
		void op_slt(uint32_t instr);
		void op_sltu(uint32_t instr);
		void op_add(uint32_t instr);
		void op_addu(uint32_t instr);
		void op_sub(uint32_t instr);
		void op_subu(uint32_t instr);
		void op_sw(uint32_t instr);
		void op_sh(uint32_t instr);
		void op_sb(uint32_t instr);
		void op_swl(uint32_t instr);
		void op_swr(uint32_t instr);
		void op_lw(uint32_t instr);
		void op_lh(uint32_t instr);
		void op_lhu(uint32_t instr);
		void op_lb(uint32_t instr);
		void op_lbu(uint32_t instr);
		void op_lwl(uint32_t instr);
		void op_lwr(uint32_t instr);
		void op_slti(uint32_t instr);
		void op_sltiu(uint32_t instr);
		void op_mult(uint32_t instr);
		void op_multu(uint32_t instr);
		void op_div(uint32_t instr);
		void op_divu(uint32_t instr);
		void op_mflo(uint32_t instr);
		void op_mfhi(uint32_t instr);
		void op_mtlo(uint32_t instr);
		void op_mthi(uint32_t instr);
		void op_syscall(uint32_t instr);
		void op_break(uint32_t instr);
		void op_illegal(uint32_t instr);
};