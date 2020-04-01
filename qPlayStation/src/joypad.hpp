#pragma once
#include "helpers.hpp"
#include "peripheral.hpp"
#include "interrupt.hpp"

enum class joypadButton : uint8_t
{
	Select = 0,
	LeftStick, // press in left stick
	RightStick,
	Start,
	DpadUp,
	DpadRight,
	DpadDown,
	DpadLeft,
	L2,
	R2,
	L1,
	R1,
	Triangle,
	Circle,
	Cross,
	Square
};

class JoypadRXFifo
{
	public:
		JoypadRXFifo();
		void reset();
		bool isEmpty();
		void push(uint8_t value);
		uint8_t pop();
		uint8_t numElements();
	private:
		uint8_t buffer[8];
		uint8_t writeIndex;
		uint8_t readIndex;
};

class joypad : public peripheral
{
	public:
		joypad(interruptController* i);
		void keyChanged(SDL_Keycode key, bool value);
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		interruptController* InterruptController;
		uint16_t buttonState;
		bool interruptRequest;
		uint8_t communicationSequenceIndex;
		JoypadRXFifo rxFifo;
		uint8_t rxInterruptMode;
		bool rxInterruptEnable;
		void sendByte(uint8_t value);
		void byteReceived(uint8_t value);
};