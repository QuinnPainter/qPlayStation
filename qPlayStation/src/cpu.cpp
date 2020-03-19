#include "cpu.hpp"

cpu::cpu(memory* mem)
{
	Memory = mem;
	reset();
}

void cpu::reset()
{
	pc = 0xBFC00000;
	for (int i = 0; i < 32; i++)
	{
		in_regs[i] = 0;
		out_regs[i] = 0;
	}
	hi = 0;
	lo = 0;
	currentLoad = {0, 0};
	next_instr = 0;
}

void cpu::setReg(int index, uint32_t value)
{
	out_regs[index] = value;
	out_regs[0] = 0;
}

uint32_t cpu::getReg(int index)
{
	return in_regs[index];
}

void cpu::step()
{
	setReg(currentLoad.regIndex, currentLoad.value);
	currentLoad = {0, 0};

	uint32_t instr = next_instr;
	next_instr = Memory->get32(pc);
	pc += 4;
	executeInstr(instr);

	memcpy(in_regs, out_regs, sizeof(in_regs));
}

void cpu::executeInstr(uint32_t instr)
{
	switch (decode_op(instr))
	{
		case 0x00: // "Special" instructions
		{
			switch (decode_funct(instr))
			{
				case 0x00: op_sll(instr); break;
				case 0x02: logging::fatal("Unimplemented instruction: SRL" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x03: logging::fatal("Unimplemented instruction: SRA" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x04: logging::fatal("Unimplemented instruction: SLLV" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x06: logging::fatal("Unimplemented instruction: SRLV" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x07: logging::fatal("Unimplemented instruction: SRAV" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x08: op_jr(instr); break;
				case 0x09: logging::fatal("Unimplemented instruction: JALR" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x0C: logging::fatal("Unimplemented instruction: SYSCALL" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x0D: logging::fatal("Unimplemented instruction: BREAK" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x10: logging::fatal("Unimplemented instruction: MFHI" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x11: logging::fatal("Unimplemented instruction: MTHI" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x12: logging::fatal("Unimplemented instruction: MFLO" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x13: logging::fatal("Unimplemented instruction: MTLO" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x18: logging::fatal("Unimplemented instruction: MULT" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x19: logging::fatal("Unimplemented instruction: MULTU" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x1A: logging::fatal("Unimplemented instruction: DIV" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x1B: logging::fatal("Unimplemented instruction: DIVU" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x20: op_add(instr); break;
				case 0x21: op_addu(instr); break;
				case 0x22: logging::fatal("Unimplemented instruction: SUB" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x23: logging::fatal("Unimplemented instruction: SUBU" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x24: op_and(instr); break;
				case 0x25: op_or(instr); break;
				case 0x26: logging::fatal("Unimplemented instruction: XOR" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x27: logging::fatal("Unimplemented instruction: NOR" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x2A: logging::fatal("Unimplemented instruction: SLT" + helpers::intToHex(instr), logging::logSource::CPU); break;
				case 0x2B: op_sltu(instr); break;
				default: logging::fatal("Invalid instruction: " + helpers::intToHex(instr), logging::logSource::CPU); break;
			}
			break;
		}
		case 0x01: logging::fatal("Unimplemented instruction: BcondZ" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x02: op_j(instr); break;
		case 0x03: op_jal(instr); break;
		case 0x04: op_beq(instr); break;
		case 0x05: op_bne(instr); break;
		case 0x06: logging::fatal("Unimplemented instruction: BLEZ" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x07: logging::fatal("Unimplemented instruction: BGTZ" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x08: op_addi(instr); break;
		case 0x09: op_addiu(instr); break;
		case 0x0A: logging::fatal("Unimplemented instruction: SLTI" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x0B: logging::fatal("Unimplemented instruction: SLTIU" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x0C: op_andi(instr); break;
		case 0x0D: op_ori(instr); break;
		case 0x0E: logging::fatal("Unimplemented instruction: XORI" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x0F: op_lui(instr); break;
		case 0x10: op_cop0(instr); break;
		case 0x11: logging::fatal("Unimplemented instruction: COP1" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x12: logging::fatal("Unimplemented instruction: COP2" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x13: logging::fatal("Unimplemented instruction: COP3" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x20: op_lb(instr); break;
		case 0x21: logging::fatal("Unimplemented instruction: LH" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x22: logging::fatal("Unimplemented instruction: LWL" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x23: op_lw(instr); break;
		case 0x24: logging::fatal("Unimplemented instruction: LBU" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x25: logging::fatal("Unimplemented instruction: LHU" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x26: logging::fatal("Unimplemented instruction: LWR" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x28: op_sb(instr); break;
		case 0x29: op_sh(instr); break;
		case 0x2A: logging::fatal("Unimplemented instruction: SWL" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x2B: op_sw(instr); break;
		case 0x2E: logging::fatal("Unimplemented instruction: SWR" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x30: logging::fatal("Unimplemented instruction: LWC0" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x31: logging::fatal("Unimplemented instruction: LWC1" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x32: logging::fatal("Unimplemented instruction: LWC2" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x33: logging::fatal("Unimplemented instruction: LWC3" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x38: logging::fatal("Unimplemented instruction: SWC0" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x39: logging::fatal("Unimplemented instruction: SWC1" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x3A: logging::fatal("Unimplemented instruction: SWC2" + helpers::intToHex(instr), logging::logSource::CPU); break;
		case 0x3B: logging::fatal("Unimplemented instruction: SWC3" + helpers::intToHex(instr), logging::logSource::CPU); break;
		default: logging::fatal("Invalid instruction: " + helpers::intToHex(instr), logging::logSource::CPU); break;
	}
}

void cpu::branch(uint32_t offset)
{
	pc += (offset << 2);
	// compensate for pc add in cpu step
	pc -= 4;
}

// -------------------------- Instruction Decode Helpers --------------------------

uint8_t cpu::decode_op(uint32_t instr) // Main opcode identifier
{
	return instr >> 26;
}

uint8_t cpu::decode_rs(uint32_t instr) // Source Register
{
	return (instr >> 21) & 0x1F;
}

uint8_t cpu::decode_rt(uint32_t instr) // Target Register
{
	return (instr >> 16) & 0x1F;
}

uint8_t cpu::decode_rd(uint32_t instr) // Destination Register
{
	return (instr >> 11) & 0x1F;
}

uint16_t cpu::decode_imm(uint32_t instr) // Immediate Value
{
	return instr & 0xFFFF;
}

uint32_t cpu::decode_imm_se(uint32_t instr) // Immediate Value (Sign Extended)
{
	return (int16_t)decode_imm(instr);
}

uint8_t cpu::decode_shamt(uint32_t instr) // Shamt (used for shifts)
{
	return (instr >> 6) & 0x1F;
}

uint8_t cpu::decode_funct(uint32_t instr) // Funct (used for secondary opcode decoding)
{
	return instr & 0x3F;
}

uint32_t cpu::decode_target(uint32_t instr) // 26 bit jump target address
{
	return instr & 0x3FFFFFF;
}

// -------------------------- Instructions --------------------------

void cpu::op_lui(uint32_t instr) // Load Upper Immediate
{
	// Set upper 16 bits of a reg to the 16 bit immediate value (lower bits are 0)
	setReg(decode_rt(instr), ((uint32_t)decode_imm(instr)) << 16);
}

void cpu::op_ori(uint32_t instr) // Or Immediate
{
	// Set lower 16 bits of a reg to another reg bitwise OR an immediate value
	uint32_t output = getReg(decode_rs(instr)) | decode_imm(instr);
	setReg(decode_rt(instr), output);
}

void cpu::op_andi(uint32_t instr) // And Immediate
{
	// Set lower 16 bits of a reg to another reg bitwise AND an immediate value
	uint32_t output = getReg(decode_rs(instr)) & decode_imm(instr);
	setReg(decode_rt(instr), output);
}

void cpu::op_sw(uint32_t instr) // Store Word
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	Memory->set32(addr, getReg(decode_rt(instr)));
}

void cpu::op_sll(uint32_t instr) // Shift Left Logical
{
	setReg(decode_rd(instr), getReg(decode_rt(instr)) << decode_shamt(instr));
}

void cpu::op_addi(uint32_t instr) // Add Immediate Signed
{
	int32_t a = getReg(decode_rs(instr));
	int32_t b = decode_imm_se(instr);
	if (helpers::checkAddOverflow(a, b))
	{
		logging::fatal("Addi Overflow", logging::logSource::CPU);
	}
	setReg(decode_rt(instr), a + b);
}

void cpu::op_addiu(uint32_t instr) // Add Immediate Unsigned
{
	// The name of this opcode is a lie. The immediate value is treated as signed.
	// Only difference between this and ADDI is that ADDIU doesn't generate an exception on overflow.
	setReg(decode_rt(instr), getReg(decode_rs(instr)) + decode_imm_se(instr));
}

void cpu::op_j(uint32_t instr) // Jump
{
	pc = (pc & 0xF0000000) | (decode_target(instr) << 2);
}

void cpu::op_jal(uint32_t instr) // Jump and Link
{
	setReg(31, pc);
	op_j(instr);
}

void cpu::op_or(uint32_t instr) // Bitwise Or
{
	setReg(decode_rd(instr), getReg(decode_rs(instr)) | getReg(decode_rt(instr)));
}

void cpu::op_and(uint32_t instr) // Bitwise And
{
	setReg(decode_rd(instr), getReg(decode_rs(instr)) & getReg(decode_rt(instr)));
}

void cpu::op_cop0(uint32_t instr) // Coprocessor 0 Operation
{
	logging::important("Unimplemented Coprocessor 0 opcode: " + helpers::intToHex(instr), logging::logSource::CPU);
}

void cpu::op_bne(uint32_t instr) // Branch if Not Equal
{
	if (getReg(decode_rs(instr)) != getReg(decode_rt(instr)))
	{
		branch(decode_imm_se(instr));
	}
}

void cpu::op_beq(uint32_t instr) // Branch if Equal
{
	if (getReg(decode_rs(instr)) == getReg(decode_rt(instr)))
	{
		branch(decode_imm_se(instr));
	}
}

void cpu::op_lw(uint32_t instr) // Load Word
{
	uint32_t value = Memory->get32(getReg(decode_rs(instr)) + decode_imm_se(instr));
	currentLoad = {decode_rt(instr), value};
}

void cpu::op_sltu(uint32_t instr) // Set on Less Than Unsigned
{
	setReg(decode_rd(instr), (uint32_t)(getReg(decode_rs(instr)) < getReg(decode_rt(instr))));
}

void cpu::op_addu(uint32_t instr) // Add Unsigned
{
	setReg(decode_rd(instr), getReg(decode_rs(instr)) + getReg(decode_rt(instr)));
}

void cpu::op_add(uint32_t instr) // Add
{
	int32_t a = getReg(decode_rs(instr));
	int32_t b = getReg(decode_rt(instr));
	if (helpers::checkAddOverflow(a, b))
	{
		logging::fatal("Add Overflow", logging::logSource::CPU);
	}
	setReg(decode_rd(instr), a + b);
}

void cpu::op_sh(uint32_t instr) // Store Halfword
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	Memory->set16(addr, getReg(decode_rt(instr)));
}

void cpu::op_sb(uint32_t instr) // Store Byte
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	Memory->set8(addr, getReg(decode_rt(instr)));
}

void cpu::op_jr(uint32_t instr) // Jump Register
{
	pc = getReg(decode_rs(instr));
}

void cpu::op_lb(uint32_t instr) // Load Byte
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	int8_t val = Memory->get8(addr);
	currentLoad = {decode_rt(instr), (uint32_t)val};
}