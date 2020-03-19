#pragma once
#include "helpers.hpp"

class peripheral
{
	public:
		virtual void set32(uint32_t addr, uint32_t value) = 0;
		virtual uint32_t get32(uint32_t addr) = 0;
};