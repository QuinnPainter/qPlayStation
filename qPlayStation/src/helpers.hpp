#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <SDL.h>
#include <GL\glew.h>
#include <SDL_opengl.h>

#include "logging.hpp"

class helpers
{
	private:
		helpers() {}
	public:
		template<typename T>
		static std::string intToHex(T i)
		{
			std::ostringstream stream;
			stream.width(sizeof(T) * 2);
			stream.fill('0');
			stream << std::uppercase << std::hex << (int)i;
			return stream.str();
		}

		template<typename T>
		static void swap(T* var1, T* var2)
		{
			T temp = *var1;
			*var1 = *var2;
			*var2 = temp;
		}

		//Sign extends a number. X is the number and bits is the number of bits the number is now.
		//Example : Sign extend a 24 bit number to 32 bit: x is a uint32_t, and bits is 24
		//https://stackoverflow.com/questions/42534749/signed-extension-from-24-bit-to-32-bit-in-c
		template<class T>
		static T signExtend(T x, const int bits) {
			T m = 1 << (bits - 1);
			return (x ^ m) - m;
		}

		//https://stackoverflow.com/questions/199333/how-do-i-detect-unsigned-integer-multiply-overflow
		//Checks if adding these 2 32 bit signed ints will cause overflow (or underflow)
		static bool checkAddOverflow(int32_t a, int32_t b)
		{
			bool overflow = false;
			if ((b > 0) && (a > INT32_MAX - b)) { overflow = true; } // Overflow
			if ((b < 0) && (a < INT32_MIN - b)) { overflow = true; } // Underflow
			return overflow;
		}

		//Checks if subtracting B from A will cause overflow (or underflow)
		static bool checkSubtractOverflow(int32_t a, int32_t b)
		{
			bool overflow = false;
			if ((b < 0) && (a > INT32_MAX + b)) { overflow = true; } // Overflow
			if ((b > 0) && (a < INT32_MIN + b)) { overflow = true; } // Underflow
			return overflow;
		}

		//Checks if a 32 bit address is 32 bit aligned
		static bool is32BitAligned(uint32_t a)
		{
			return (a & 0x3) == 0;
		}

		//Checks if a 32 bit address is 16 bit aligned
		static bool is16BitAligned(uint32_t a)
		{
			return (a & 0x1) == 0;
		}
};