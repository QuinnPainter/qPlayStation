#pragma once
#include "helpers.hpp"

class gte
{
	public:
		void reset();
		void writeCR(uint32_t which, uint32_t value);
		void writeDR(uint32_t which, uint32_t value);
		uint32_t readCR(uint32_t which);
		uint32_t readDR(uint32_t which);
		int32_t instruction(uint32_t instr);
};