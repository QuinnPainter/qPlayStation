#include "cpu.hpp"

cpu::cpu(memory* mem, EXEInfo exeI)
{
	Memory = mem;
	exeInfo = exeI;
	GTE = new gte();
	reset();
}

cpu::~cpu()
{
	delete(GTE);
}

void cpu::reset()
{
	pc = 0xBFC00000;
	next_pc = pc + 4;
	for (int i = 0; i < 32; i++)
	{
		in_regs[i] = 0;
		out_regs[i] = 0;
	}
	hi = 0;
	lo = 0;
	cop0_sr = 0;
	cop0_cause = 0;
	cop0_epc = 0;
	currentLoad = { 0, 0 };
	is_branch = false;
	delay_slot = false;

	GTE->reset();
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

void cpu::updateInterruptRequest(bool interruptRequest)
{
	// Set bit 10 of the cause register to interruptRequest
	cop0_cause &= ~(1 << 10);
	cop0_cause |= ((uint32_t)interruptRequest) << 10;
}

bool cpu::cacheIsolated()
{
	return (cop0_sr & 0x10000);
}

void cpu::step()
{
	if (pc == 0xBFC06FF0 && exeInfo.present)
	{
		logging::info("Jumping to EXE", logging::logSource::CPU);
		pc = exeInfo.initialPC;
		next_pc = pc + 4;
		setReg(28, exeInfo.initialR28);
		setReg(29, exeInfo.initialR29R30);
		setReg(30, exeInfo.initialR29R30);
	}

	if (!helpers::is32BitAligned(pc))
	{
		exception(psException::AddrErrorLoad);
	}

	uint32_t instr = Memory->get32(pc);

	current_pc = pc;

	pc = next_pc;
	next_pc += 4;

	setReg(currentLoad.regIndex, currentLoad.value);
	currentLoad = {0, 0};

	delay_slot = is_branch;
	is_branch = false;

	// Check bit 10 of cause - interrupt request
	// and bits 0 and 10 of sr - interrupt enable
	if ((cop0_cause & (1 << 10)) && (cop0_sr & (1 << 10)) && (cop0_sr & 1))
	{
		exception(psException::Interrupt);
	}
	else
	{
		executeInstr(instr);
	}

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
				case 0x02: op_srl(instr); break;
				case 0x03: op_sra(instr); break;
				case 0x04: op_sllv(instr); break;
				case 0x06: op_srlv(instr); break;
				case 0x07: op_srav(instr); break;
				case 0x08: op_jr(instr); break;
				case 0x09: op_jalr(instr); break;
				case 0x0C: op_syscall(instr); break;
				case 0x0D: op_break(instr); break;
				case 0x10: op_mfhi(instr); break;
				case 0x11: op_mthi(instr); break;
				case 0x12: op_mflo(instr); break;
				case 0x13: op_mtlo(instr); break;
				case 0x18: op_mult(instr); break;
				case 0x19: op_multu(instr); break;
				case 0x1A: op_div(instr); break;
				case 0x1B: op_divu(instr); break;
				case 0x20: op_add(instr); break;
				case 0x21: op_addu(instr); break;
				case 0x22: op_sub(instr); break;
				case 0x23: op_subu(instr); break;
				case 0x24: op_and(instr); break;
				case 0x25: op_or(instr); break;
				case 0x26: op_xor(instr); break;
				case 0x27: op_nor(instr); break;
				case 0x2A: op_slt(instr); break;
				case 0x2B: op_sltu(instr); break;
				default: op_illegal(instr); break;
			}
			break;
		}
		case 0x01: op_bcondz(instr); break;
		case 0x02: op_j(instr); break;
		case 0x03: op_jal(instr); break;
		case 0x04: op_beq(instr); break;
		case 0x05: op_bne(instr); break;
		case 0x06: op_blez(instr); break;
		case 0x07: op_bgtz(instr); break;
		case 0x08: op_addi(instr); break;
		case 0x09: op_addiu(instr); break;
		case 0x0A: op_slti(instr); break;
		case 0x0B: op_sltiu(instr); break;
		case 0x0C: op_andi(instr); break;
		case 0x0D: op_ori(instr); break;
		case 0x0E: op_xori(instr); break;
		case 0x0F: op_lui(instr); break;
		case 0x10: op_cop0(instr); break;
		case 0x11: op_cop1(instr); break;
		case 0x12: op_cop2(instr); break;
		case 0x13: op_cop3(instr); break;
		case 0x20: op_lb(instr); break;
		case 0x21: op_lh(instr); break;
		case 0x22: op_lwl(instr); break;
		case 0x23: op_lw(instr); break;
		case 0x24: op_lbu(instr); break;
		case 0x25: op_lhu(instr); break;
		case 0x26: op_lwr(instr); break;
		case 0x28: op_sb(instr); break;
		case 0x29: op_sh(instr); break;
		case 0x2A: op_swl(instr); break;
		case 0x2B: op_sw(instr); break;
		case 0x2E: op_swr(instr); break;
		case 0x30: op_lwc0(instr); break;
		case 0x31: op_lwc1(instr); break;
		case 0x32: op_lwc2(instr); break;
		case 0x33: op_lwc3(instr); break;
		case 0x38: op_swc0(instr); break;
		case 0x39: op_swc1(instr); break;
		case 0x3A: op_swc2(instr); break;
		case 0x3B: op_swc3(instr); break;
		default: op_illegal(instr); break;
	}
}

void cpu::branch(uint32_t offset)
{
	next_pc += (offset << 2);
	// compensate for pc add in cpu step
	next_pc -= 4;
	is_branch = true;
}

void cpu::exception(psException exceptionType)
{
	// Exception handler address depends on the BEV bit in the Status Register
	uint32_t handlerAddr = ((cop0_sr & (1 << 22)) != 0) ? 0xBFC00180 : 0x80000080;

	uint32_t mode = cop0_sr & 0x3F;
	cop0_sr &= ~0x3F;
	cop0_sr |= (mode << 2) & 0x3F;

	cop0_cause &= ~0x7C;
	cop0_cause |= ((uint32_t)exceptionType) << 2;

	if (delay_slot)
	{
		cop0_epc = current_pc - 4;
		cop0_cause |= 1 << 31;
	}
	else
	{
		cop0_epc = current_pc;
		cop0_cause &= ~(1 << 31);
	}

	// skip branch delay
	pc = handlerAddr;
	next_pc = pc + 4;
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
	uint32_t output = getReg(decode_rs(instr)) | decode_imm(instr);
	setReg(decode_rt(instr), output);
}

void cpu::op_andi(uint32_t instr) // And Immediate
{
	uint32_t output = getReg(decode_rs(instr)) & decode_imm(instr);
	setReg(decode_rt(instr), output);
}

void cpu::op_xori(uint32_t instr) // Xor Immediate
{
	uint32_t output = getReg(decode_rs(instr)) ^ decode_imm(instr);
	setReg(decode_rt(instr), output);
}

void cpu::op_sll(uint32_t instr) // Shift Left Logical
{
	setReg(decode_rd(instr), getReg(decode_rt(instr)) << decode_shamt(instr));
}

void cpu::op_srl(uint32_t instr) // Shift Right Logical
{
	setReg(decode_rd(instr), getReg(decode_rt(instr)) >> decode_shamt(instr));
}

void cpu::op_sra(uint32_t instr) // Shift Right Arithmetic
{
	setReg(decode_rd(instr), ((int32_t)getReg(decode_rt(instr))) >> decode_shamt(instr));
}

void cpu::op_sllv(uint32_t instr) // Shift Left Logical Variable
{
	setReg(decode_rd(instr), getReg(decode_rt(instr)) << (getReg(decode_rs(instr)) & 0x1F));
}

void cpu::op_srlv(uint32_t instr) // Shift Right Logical Variable
{
	setReg(decode_rd(instr), getReg(decode_rt(instr)) >> (getReg(decode_rs(instr)) & 0x1F));
}

void cpu::op_srav(uint32_t instr) // Shift Right Arithmetic Variable
{
	setReg(decode_rd(instr), ((int32_t)getReg(decode_rt(instr))) >> (getReg(decode_rs(instr)) & 0x1F));
}

void cpu::op_addi(uint32_t instr) // Add Immediate Signed
{
	int32_t a = getReg(decode_rs(instr));
	int32_t b = decode_imm_se(instr);
	if (helpers::checkAddOverflow(a, b))
	{
		exception(psException::ArithOverflow);
	}
	else
	{
		setReg(decode_rt(instr), a + b);
	}
}

void cpu::op_addiu(uint32_t instr) // Add Immediate Unsigned
{
	// The name of this opcode is a lie. The immediate value is treated as signed.
	// Only difference between this and ADDI is that ADDIU doesn't generate an exception on overflow.
	setReg(decode_rt(instr), getReg(decode_rs(instr)) + decode_imm_se(instr));
}

void cpu::op_j(uint32_t instr) // Jump
{
	next_pc = (pc & 0xF0000000) | (decode_target(instr) << 2);
	is_branch = true;
}

void cpu::op_jal(uint32_t instr) // Jump and Link
{
	setReg(31, next_pc);
	op_j(instr);
}

void cpu::op_jr(uint32_t instr) // Jump Register
{
	next_pc = getReg(decode_rs(instr));
	is_branch = true;
}

void cpu::op_jalr(uint32_t instr) // Jump and Link Register
{
	setReg(decode_rd(instr), next_pc);
	next_pc = getReg(decode_rs(instr));
	is_branch = true;
}

void cpu::op_or(uint32_t instr) // Bitwise Or
{
	setReg(decode_rd(instr), getReg(decode_rs(instr)) | getReg(decode_rt(instr)));
}

void cpu::op_and(uint32_t instr) // Bitwise And
{
	setReg(decode_rd(instr), getReg(decode_rs(instr)) & getReg(decode_rt(instr)));
}

void cpu::op_xor(uint32_t instr) // Bitwise Xor
{
	setReg(decode_rd(instr), getReg(decode_rs(instr)) ^ getReg(decode_rt(instr)));
}

void cpu::op_nor(uint32_t instr) // Bitwise Nor
{
	setReg(decode_rd(instr), ~(getReg(decode_rs(instr)) | getReg(decode_rt(instr))));
}

void cpu::op_cop0(uint32_t instr) // Coprocessor 0 Operation
{
	switch (decode_rs(instr))
	{
		case 0b00000: // MFC - Move from Coprocessor
		{
			uint32_t outputValue = 0;
			switch (decode_rd(instr))
			{
				case 6: outputValue = 0; break; // JUMPDEST - contains pretty much useless random jump address
				case 7: outputValue = 0; break; // Breakpoint control
				case 8: outputValue = 0; break; // BadVaddr - should contain address that caused an address error
				case 12: outputValue = cop0_sr; break;
				case 13: outputValue = cop0_cause; break;
				case 14: outputValue = cop0_epc; break;
				case 15: outputValue = 0x00000002; break; // Processor ID
				default: logging::fatal("Read from unhandled COP0 register: " + std::to_string(decode_rd(instr)), logging::logSource::CPU); break;
			}
			currentLoad = {decode_rt(instr), outputValue};
			break;
		}
		case 0b00100: // MTC - Move to Coprocessor
		{
			uint32_t value = getReg(decode_rt(instr));
			switch (decode_rd(instr))
			{
				case 12: cop0_sr = value; break;
				default: logging::info("Write to unhandled COP0 register: " + std::to_string(decode_rd(instr)), logging::logSource::CPU); break;
			}
			break;
		}
		case 0b10000: // Special COP0 commands
		{
			if (decode_funct(instr) == 0b010000) // RFE - Return from Exception
			{
				uint32_t mode = cop0_sr & 0x3F;
				cop0_sr &= ~0xF;
				cop0_sr |= mode >> 2;
			}
			else
			{
				logging::fatal("Unimplemented COP0 instruction: " + helpers::intToHex(instr), logging::logSource::CPU); break;
			}
			break;
		}
		default: logging::fatal("Unimplemented COP0 instruction: " + helpers::intToHex(instr), logging::logSource::CPU); break;
	}
}

void cpu::op_cop1(uint32_t instr) // Coprocessor 1 Operation
{
	exception(psException::CoprocessorUnusable);
}

void cpu::op_cop2(uint32_t instr) // Coprocessor 2 Operation (GTE)
{
	uint8_t subop = decode_rs(instr);
	uint8_t rt = decode_rt(instr);
	uint8_t rd = decode_rd(instr);
	uint32_t val = getReg(rt);

	if (!(cop0_sr & (1 << 30))) // GTE is disabled
	{
		exception(psException::CoprocessorUnusable);
	}
	else
	{
		switch (subop)
		{
			case 0x00: // MFC - Move from Coprocessor
			{
				currentLoad = {rt, GTE->readDR(rd)};
				break;
			}
			case 0x04: // MTC - Move to Coprocessor
			{
				GTE->writeDR(rd, val);
				break;
			}
			case 0x02: // CFC - Move Control from Coprocessor
			{
				currentLoad = {rt, GTE->readCR(rd)};
				break;
			}
			case 0x06: // CTC - Move Control to Coprocessor
			{
				GTE->writeCR(rd, val);
				break;
			}
			case 0x08: case 0x0C: // Some sort of branch
			{
				logging::fatal("Unhandled GTE branch instruction: " + helpers::intToHex(instr), logging::logSource::CPU);
				break;
			}
			case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
			case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
			{
				GTE->instruction(instr);
				break;
			}
			default: logging::fatal("Invalid GTE command: " + helpers::intToHex(instr), logging::logSource::CPU); break;
		}
	}
}

void cpu::op_cop3(uint32_t instr) // Coprocessor 3 Operation
{
	exception(psException::CoprocessorUnusable);
}

void cpu::op_lwc0(uint32_t instr) // Load Word Coprocessor 0
{
	exception(psException::CoprocessorUnusable);
}

void cpu::op_lwc1(uint32_t instr) // Load Word Coprocessor 1
{
	exception(psException::CoprocessorUnusable);
}

void cpu::op_lwc2(uint32_t instr) // Load Word Coprocessor 2 (GTE)
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	if (addr & 0x3)
	{
		exception(psException::AddrErrorLoad);
	}
	else
	{
		GTE->writeDR(decode_rt(instr), Memory->get32(addr));
	}
}

void cpu::op_lwc3(uint32_t instr) // Load Word Coprocessor 3
{
	exception(psException::CoprocessorUnusable);
}

void cpu::op_swc0(uint32_t instr) // Store Word Coprocessor 0
{
	exception(psException::CoprocessorUnusable);
}

void cpu::op_swc1(uint32_t instr) // Store Word Coprocessor 1
{
	exception(psException::CoprocessorUnusable);
}

void cpu::op_swc2(uint32_t instr) // Store Word Coprocessor 2 (GTE)
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	if (addr & 0x3)
	{
		exception(psException::AddrErrorStore);
	}
	else
	{
		Memory->set32(addr, GTE->readDR(decode_rt(instr)));
	}
}

void cpu::op_swc3(uint32_t instr) // Store Word Coprocessor 3
{
	exception(psException::CoprocessorUnusable);
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

void cpu::op_bgtz(uint32_t instr) // Branch if Greater than Zero
{
	if (((int32_t)getReg(decode_rs(instr))) > 0)
	{
		branch(decode_imm_se(instr));
	}
}

void cpu::op_blez(uint32_t instr) // Branch if Less than or Equal to Zero
{
	if (((int32_t)getReg(decode_rs(instr))) <= 0)
	{
		branch(decode_imm_se(instr));
	}
}

void cpu::op_bcondz(uint32_t instr) // BLTZ / BLTZAL / BGEZ / BGEZAL
{
	// Branch Less than Zero, Branch Less than Zero and Link,
	// Branch Greater than or Equal to Zero, Branch Greater than or Equal to Zero and Link
	bool link = (decode_rt(instr) & 0b11110) == 0b10000; // True - AL, False - No AL
	bool bgez = decode_rt(instr) & 0b00001; // True - BGEZ, False - BLTZ

	bool test = ((int32_t)getReg(decode_rs(instr))) < 0;
	if (bgez) { test = !test; }

	if (link)
	{
		setReg(31, next_pc);
	}
	if (test)
	{
		branch(decode_imm_se(instr));
	}
}

void cpu::op_slt(uint32_t instr) // Set on Less Than
{
	setReg(decode_rd(instr), ((int32_t)getReg(decode_rs(instr))) < ((int32_t)getReg(decode_rt(instr))));
}

void cpu::op_sltu(uint32_t instr) // Set on Less Than Unsigned
{
	setReg(decode_rd(instr), (uint32_t)(getReg(decode_rs(instr)) < getReg(decode_rt(instr))));
}

void cpu::op_add(uint32_t instr) // Add
{
	int32_t a = getReg(decode_rs(instr));
	int32_t b = getReg(decode_rt(instr));
	if (helpers::checkAddOverflow(a, b))
	{
		exception(psException::ArithOverflow);
	}
	else
	{
		setReg(decode_rd(instr), a + b);
	}
}

void cpu::op_addu(uint32_t instr) // Add Unsigned
{
	setReg(decode_rd(instr), getReg(decode_rs(instr)) + getReg(decode_rt(instr)));
}

void cpu::op_sub(uint32_t instr) // Subtract
{
	int32_t a = getReg(decode_rs(instr));
	int32_t b = getReg(decode_rt(instr));
	if (helpers::checkSubtractOverflow(a, b))
	{
		exception(psException::ArithOverflow);
	}
	else
	{
		setReg(decode_rd(instr), a - b);
	}
}

void cpu::op_subu(uint32_t instr) // Subtract Unsigned
{
	setReg(decode_rd(instr), getReg(decode_rs(instr)) - getReg(decode_rt(instr)));
}

void cpu::op_sw(uint32_t instr) // Store Word
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	if (helpers::is32BitAligned(addr))
	{
		if (cacheIsolated()) { return; }
		Memory->set32(addr, getReg(decode_rt(instr)));
	}
	else
	{
		exception(psException::AddrErrorStore);
	}
}

void cpu::op_sh(uint32_t instr) // Store Halfword
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	if (helpers::is16BitAligned(addr))
	{
		if (cacheIsolated()) { return; }
		Memory->set16(addr, getReg(decode_rt(instr)));
	}
	else
	{
		exception(psException::AddrErrorStore);
	}
}

void cpu::op_sb(uint32_t instr) // Store Byte
{
	if (cacheIsolated()) { return; }

	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	Memory->set8(addr, getReg(decode_rt(instr)));
}

void cpu::op_swl(uint32_t instr) // Store Word Left
{
	if (cacheIsolated()) { return; }

	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);

	uint32_t toSet = getReg(decode_rt(instr));
	uint32_t alignedAddr = addr & ~0x3;
	uint32_t alignedValue = Memory->get32(alignedAddr);

	uint32_t output = 0;
	switch (addr & 0x3)
	{
		case 0: output = (alignedValue & 0xFFFFFF00) | (toSet >> 24); break;
		case 1: output = (alignedValue & 0xFFFF0000) | (toSet >> 16); break;
		case 2: output = (alignedValue & 0xFF000000) | (toSet >> 8); break;
		case 3: output = (alignedValue & 0x00000000) | (toSet >> 0); break;
	}
	Memory->set32(alignedAddr, output);
}

void cpu::op_swr(uint32_t instr) // Store Word Right
{
	if (cacheIsolated()) { return; }

	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);

	uint32_t toSet = getReg(decode_rt(instr));
	uint32_t alignedAddr = addr & ~0x3;
	uint32_t alignedValue = Memory->get32(alignedAddr);

	uint32_t output = 0;
	switch (addr & 0x3)
	{
		case 0: output = (alignedValue & 0x00000000) | (toSet << 0); break;
		case 1: output = (alignedValue & 0x000000FF) | (toSet << 8); break;
		case 2: output = (alignedValue & 0x0000FFFF) | (toSet << 16); break;
		case 3: output = (alignedValue & 0x00FFFFFF) | (toSet << 24); break;
	}
	Memory->set32(alignedAddr, output);
}

void cpu::op_lw(uint32_t instr) // Load Word
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	if (helpers::is32BitAligned(addr))
	{
		currentLoad = { decode_rt(instr), Memory->get32(addr) };
	}
	else
	{
		exception(psException::AddrErrorLoad);
	}
}

void cpu::op_lh(uint32_t instr) // Load Halfword
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	if (helpers::is16BitAligned(addr))
	{
		int16_t value = Memory->get16(addr);
		currentLoad = {decode_rt(instr), (uint32_t)value};
	}
	else
	{
		exception(psException::AddrErrorLoad);
	}
}

void cpu::op_lhu(uint32_t instr) // Load Halfword Unsigned
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	if (helpers::is16BitAligned(addr))
	{
		currentLoad = {decode_rt(instr), Memory->get16(addr)};
	}
	else
	{
		exception(psException::AddrErrorLoad);
	}
}

void cpu::op_lb(uint32_t instr) // Load Byte
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	int8_t val = Memory->get8(addr);
	currentLoad = {decode_rt(instr), (uint32_t)val};
}

void cpu::op_lbu(uint32_t instr) // Load Byte Unsigned
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);
	currentLoad = {decode_rt(instr), Memory->get8(addr)};
}

void cpu::op_lwl(uint32_t instr) // Load Word Left
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);

	uint32_t loadValue = out_regs[decode_rt(instr)];
	uint32_t alignedValue = Memory->get32(addr & ~0x3);

	uint32_t output = 0;
	switch (addr & 0x3)
	{
		case 0: output = (loadValue & 0x00FFFFFF) | (alignedValue << 24); break;
		case 1: output = (loadValue & 0x0000FFFF) | (alignedValue << 16); break;
		case 2: output = (loadValue & 0x000000FF) | (alignedValue << 8); break;
		case 3: output = (loadValue & 0x00000000) | (alignedValue << 0); break;
	}
	currentLoad = {decode_rt(instr), output};
}

void cpu::op_lwr(uint32_t instr) // Load Word Right
{
	uint32_t addr = getReg(decode_rs(instr)) + decode_imm_se(instr);

	uint32_t loadValue = out_regs[decode_rt(instr)];
	uint32_t alignedValue = Memory->get32(addr & ~0x3);

	uint32_t output = 0;
	switch (addr & 0x3)
	{
		case 0: output = (loadValue & 0x00000000) | (alignedValue >> 0); break;
		case 1: output = (loadValue & 0xFF000000) | (alignedValue >> 8); break;
		case 2: output = (loadValue & 0xFFFF0000) | (alignedValue >> 16); break;
		case 3: output = (loadValue & 0xFFFFFF00) | (alignedValue >> 24); break;
	}
	currentLoad = {decode_rt(instr), output};
}

void cpu::op_slti(uint32_t instr) // Set if Less Than Immediate
{
	setReg(decode_rt(instr), ((int32_t)getReg(decode_rs(instr))) < ((int32_t)decode_imm_se(instr)));
}

void cpu::op_sltiu(uint32_t instr) // Set if Less Than Immediate Unsigned
{
	setReg(decode_rt(instr), getReg(decode_rs(instr)) < decode_imm_se(instr));
}

void cpu::op_mult(uint32_t instr) // Multiply
{
	int64_t a = (int32_t)getReg(decode_rs(instr));
	int64_t b = (int32_t)getReg(decode_rt(instr));
	uint64_t result = a * b;
	hi = (uint32_t)(result >> 32);
	lo = (uint32_t)(result & 0xFFFFFFFF);
}

void cpu::op_multu(uint32_t instr) // Multiply Unsigned
{
	uint64_t result = ((uint64_t)getReg(decode_rs(instr))) * ((uint64_t)getReg(decode_rt(instr)));
	hi = (uint32_t)(result >> 32);
	lo = (uint32_t)(result & 0xFFFFFFFF);
}

void cpu::op_div(uint32_t instr) // Divide
{
	int32_t numerator = getReg(decode_rs(instr));
	int32_t denominator = getReg(decode_rt(instr));

	if (denominator == 0) // Special case 1 - Division by 0
	{
		hi = numerator;
		lo = (numerator >= 0) ? 0xFFFFFFFF : 1;
	}
	else if (((uint32_t)numerator) == 0x80000000 && denominator == -1)
	{
		// Special case 2 - Result doesn't fit in 32 bit int
		hi = 0;
		lo = 0x80000000;
	}
	else
	{
		hi = numerator % denominator;
		lo = numerator / denominator;
	}
}

void cpu::op_divu(uint32_t instr) // Divide Unsigned
{
	uint32_t numerator = getReg(decode_rs(instr));
	uint32_t denominator = getReg(decode_rt(instr));

	if (denominator == 0) // Special case - Division by 0
	{
		hi = numerator;
		lo = 0xFFFFFFFF;
	}
	else
	{
		hi = numerator % denominator;
		lo = numerator / denominator;
	}
}

void cpu::op_mflo(uint32_t instr) // Move from Lo
{
	setReg(decode_rd(instr), lo);
}

void cpu::op_mfhi(uint32_t instr) // Move from Hi
{
	setReg(decode_rd(instr), hi);
}

void cpu::op_mtlo(uint32_t instr) // Move to Lo
{
	lo = getReg(decode_rs(instr));
}

void cpu::op_mthi(uint32_t instr) // Move to Hi
{
	hi = getReg(decode_rs(instr));
}

void cpu::op_syscall(uint32_t instr) // System Call
{
	exception(psException::Syscall);
}

void cpu::op_break(uint32_t instr) // Break
{
	exception(psException::Breakpoint);
}

void cpu::op_illegal(uint32_t instr) // Illegal instruction
{
	logging::warning("Illegal instruction: " + helpers::intToHex(instr), logging::logSource::CPU);
	exception(psException::ReservedInstr);
}