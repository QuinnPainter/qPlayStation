#include "cdrom.hpp"

CDROMFIFO::CDROMFIFO()
{
	reset();
}

void CDROMFIFO::reset()
{
	writeIndex = 0;
	readIndex = 0;
	for (int i = 0; i < sizeof(buffer); i++)
	{
		buffer[i] = 0;
	}
}

bool CDROMFIFO::isEmpty()
{
	return writeIndex == readIndex;
}

bool CDROMFIFO::isFull()
{
	return writeIndex == (readIndex ^ 0x10);
}

void CDROMFIFO::push(uint8_t value)
{
	buffer[writeIndex & 0xF] = value;
	writeIndex = (writeIndex + 1) & 0x1F;
}

uint8_t CDROMFIFO::pop()
{
	uint8_t ret = buffer[readIndex & 0xF];
	readIndex = (readIndex + 1) & 0x1F;
	return ret;
}

uint8_t CDROMFIFO::numElements()
{
	return (writeIndex - readIndex) & 0x1F;
}

cdrom::cdrom(interruptController* i)
{
	InterruptController = i;
	portIndex = 0;
	interruptEnable = 0;
	responseReceived = 0;
	commandStartInterrupt = false;
}

void cdrom::set32(uint32_t addr, uint32_t value) { logging::fatal("unimplemented 32 bit CDROM write" + helpers::intToHex(addr), logging::logSource::CDROM); }
uint32_t cdrom::get32(uint32_t addr) { logging::fatal("unimplemented 32 bit CDROM read" + helpers::intToHex(addr), logging::logSource::CDROM); return 0; }
void cdrom::set16(uint32_t addr, uint16_t value) { logging::fatal("unimplemented 16 bit CDROM write" + helpers::intToHex(addr), logging::logSource::CDROM); }
uint16_t cdrom::get16(uint32_t addr) { logging::fatal("unimplemented 32 bit CDROM read" + helpers::intToHex(addr), logging::logSource::CDROM); return 0; }

void cdrom::set8(uint32_t addr, uint8_t value)
{
	logging::info("CDROM Write: " + helpers::intToHex(addr) + " " + helpers::intToHex(value), logging::logSource::CDROM);
	switch (addr)
	{
		case 0: // Status Register
		{
			portIndex = value & 0x3;
			break;
		}
		case 1:
		{
			switch (portIndex)
			{
				case 0: writeCommandRegister(value); break;
				case 1: break; // Sound Map Data Out
				case 2: break; // Sound Map Coding Info
				case 3: break; // Audio Volume - Right CD out to Right SPU input
			}
			break;
		}
		case 2:
		{
			switch (portIndex)
			{
				case 0: parameterFifo.push(value); break;
				case 1: interruptEnable = value & 0x1F; break;
				case 2: break; // Audio Volume - Left CD out to Left SPU input
				case 3: break; // Audio Volume - Right CD out to Left SPU input
			}
			break;
		}
		case 3:
		{
			switch (portIndex)
			{
				case 0: break; // Request Register
				case 1: // Interrupt Flag Register
				{
					responseReceived &= ~(value & 0x7); // Acknowledge interrupts
					commandStartInterrupt = value & 0x10;
					if (value & 0x40)
					{
						parameterFifo.reset();
					}
					break;
				}
				case 2: break; // Audio Volume - Left CD out to Right SPU input
				case 3: break; // Audio Volume - Apply Changes
			}
			break;
		}
	}
}

uint8_t cdrom::get8(uint32_t addr)
{
	logging::info("CDROM Read: " + helpers::intToHex(addr), logging::logSource::CDROM);
	switch (addr)
	{
		case 0: // Status Register
		{
			return portIndex |
				0 << 2 | // XA_ADCPM FIFO empty (0 = Empty)
				((uint8_t)parameterFifo.isEmpty()) << 3 |
				((uint8_t)(!parameterFifo.isFull())) << 4 |
				((uint8_t)responseFifo.isEmpty()) << 5 |
				0 << 6 | // Data FIFO empty (0 = Empty)
				0 << 7; // Command / Parameter transmission busy (1 = Busy)
		}
		case 1: return responseFifo.pop();
		case 2: return 0; // Data Fifo
		case 3:
		{
			switch (portIndex)
			{
				case 0: case 2: return interruptEnable;
				case 1: case 3: // Interrupt Flag Register
				{
					return responseReceived | (((uint8_t)commandStartInterrupt) << 4) | 0xE0;
				}
			}
		}
	}
}

void cdrom::writeCommandRegister(uint8_t value)
{
	executeCommand(value);
}

void cdrom::executeCommand(uint8_t command)
{
	switch (command)
	{
		case 0x01: // Getstat
		{
			responseFifo.push(0b00010000); // temporary - says "everything is ok but the lid is open"
			responseReceived = 3;
			break;
		}
		case 0x19: // Test
		{
			if (parameterFifo.numElements() != 1)
			{
				logging::fatal("Incorrect number of parameters for CDROM Test command: " + helpers::intToHex(parameterFifo.numElements()), logging::logSource::CDROM);
			}
			uint8_t subfunction = parameterFifo.pop();
			switch (subfunction)
			{
				case 0x20: // Get CDROM BIOS date / version
				{
					// Taken from rustation. Apparently came from a PAL SCPH-7502 console.
					responseFifo.push(0x98); // Year
					responseFifo.push(0x06); // Month
					responseFifo.push(0x10); // Day
					responseFifo.push(0xc3); // Version
					responseReceived = 3;
					break;
				}
				default: logging::fatal("Unimplemented CDROM Test Sub-Function: " + helpers::intToHex(subfunction), logging::logSource::CDROM); break;
			}
			break;
		}
		default: logging::fatal("Unimplemented CDROM command: " + helpers::intToHex(command), logging::logSource::CDROM); break;
	}

	InterruptController->requestInterrupt(interruptType::CDROM);
}