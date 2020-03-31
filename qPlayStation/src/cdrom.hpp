#pragma once
#include "helpers.hpp"
#include "peripheral.hpp"
#include "interrupt.hpp"

class CDROMFIFO // Used for command arguments and responses
{
	public:
		CDROMFIFO();
		void reset();
		bool isEmpty();
		bool isFull();
		void push(uint8_t value);
		uint8_t pop();
		uint8_t numElements();
	private:
		uint8_t buffer[16];
		uint8_t writeIndex;
		uint8_t readIndex;
};

class cdrom : public peripheral
{
	public:
		cdrom(interruptController* i);
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		interruptController* InterruptController;
		uint8_t portIndex;
		uint8_t interruptEnable;
		uint8_t responseReceived;
		bool commandStartInterrupt;
		CDROMFIFO parameterFifo;
		CDROMFIFO responseFifo;
		void executeCommand(uint8_t command);

		void writeCommandRegister(uint8_t value);
};