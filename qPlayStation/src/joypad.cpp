#include "joypad.hpp"

/*     ___                      ___           ___                      ___
    __/_L_\__   Analog Pad   __/_R_\__     __/_L_\__  Digital Pad   __/_R_\__
   /    _    \--------------/         \   /    _    \--------------/         \
  |   _| |_   |            |     /\    | |   _| |_   |            |     /\    |
  |  |_ X _|  |SEL      STA|  []    () | |  |_ X _|  |            |  []    () |
  |    |_|  ___   ANALOG   ___   ><    | |    |_|    |  SEL  STA  |     ><    |
  |\______ / L \   LED    / R \ ______/| |\_________/--------------\_________/|
  |       | Joy |--------| Joy |       | |       |                    |       |
  |      / \___/          \___/ \      | |      /                      \      |
   \____/                        \____/   \____/                        \____/
*/

const std::map<SDL_Keycode, joypadButton> bindings = {
	{SDLK_UP, joypadButton::DpadUp},
	{SDLK_RIGHT, joypadButton::DpadRight},
	{SDLK_DOWN, joypadButton::DpadDown},
	{SDLK_LEFT, joypadButton::DpadLeft},
	{SDLK_RETURN, joypadButton::Start},
	{SDLK_RSHIFT, joypadButton::Select},
	{SDLK_z, joypadButton::Cross},
	{SDLK_a, joypadButton::Square},
	{SDLK_s, joypadButton::Triangle},
	{SDLK_x, joypadButton::Circle},
	{SDLK_q, joypadButton::L2},
	{SDLK_w, joypadButton::L1},
	{SDLK_e, joypadButton::R1},
	{SDLK_r, joypadButton::R2}
};

JoypadRXFifo::JoypadRXFifo()
{
	reset();
}

void JoypadRXFifo::reset()
{
	writeIndex = 0;
	readIndex = 0;
	for (int i = 0; i < sizeof(buffer); i++)
	{
		buffer[i] = 0;
	}
}

bool JoypadRXFifo::isEmpty()
{
	return writeIndex == readIndex;
}

void JoypadRXFifo::push(uint8_t value)
{
	buffer[writeIndex] = value;
	writeIndex = (writeIndex + 1) & 0x7;
}

uint8_t JoypadRXFifo::pop()
{
	uint8_t ret = buffer[readIndex];
	readIndex = (readIndex + 1) & 0x7;
	return ret;
}

uint8_t JoypadRXFifo::numElements()
{
	return writeIndex - readIndex;
}

joypad::joypad(interruptController* i)
{
	InterruptController = i;
	buttonState = 0xFFFF;
	interruptRequest = false;
	communicationSequenceIndex = 0;
	rxInterruptMode = 0;
	rxInterruptEnable = false;
}

void joypad::set32(uint32_t addr, uint32_t value) { logging::fatal("unimplemented 32 bit joypad write" + helpers::intToHex(addr), logging::logSource::Joypad); }
uint32_t joypad::get32(uint32_t addr) { logging::fatal("unimplemented 32 bit joypad read" + helpers::intToHex(addr), logging::logSource::Joypad); return 0; }
void joypad::set16(uint32_t addr, uint16_t value)
{
	switch (addr)
	{
		case 0x8: // JOY_MODE
		{
			break;
		}
		case 0xA: // JOY_CTRL
		{
			// Acknowledge interrupt
			interruptRequest = (value & (1 << 4)) ? 0 : interruptRequest;
			rxInterruptMode = (value >> 8) & 0x3;
			rxInterruptEnable = (value >> 11) & 1;
			break;
		}
		case 0xE: // JOY_BAUD
		{
			break;
		}
		default:logging::fatal("unhandled 16 bit joypad write" + helpers::intToHex(addr), logging::logSource::Joypad); break;
	}
}
uint16_t joypad::get16(uint32_t addr)
{
	switch (addr)
	{
		case 0x4: // JOY_STAT
		{
			return 1 | // TX Ready Flag 1
				(((uint16_t)!rxFifo.isEmpty()) << 1) |
				(1 << 2) | // TX Ready Flag 2
				(((uint16_t)interruptRequest) << 9);
		}
		case 0xA: // JOY_CTRL
		{
			return 1 | // TX Enable
				0 << 1 | // JOYn Output
				1 << 2 | // RX Enable
				(((uint16_t)rxInterruptMode) << 8) |
				0 << 10 | // TX Interrupt Enable
				(((uint16_t)rxInterruptEnable) << 11);
				0 << 12 | // ACK Interrupt Enable
				0 << 13; // Desired Slot Number
		}
		default: logging::fatal("unhandled 16 bit joypad read" + helpers::intToHex(addr), logging::logSource::Joypad); return 0;
	}
}
void joypad::set8(uint32_t addr, uint8_t value)
{
	switch (addr)
	{
		case 0x0: // JOY_TX_DATA
		{
			sendByte(value);
			break;
		}
		default:logging::fatal("unhandled 8 bit joypad write" + helpers::intToHex(addr), logging::logSource::Joypad); break;
	}
}
uint8_t joypad::get8(uint32_t addr)
{
	switch (addr)
	{
		case 0x0: // JOY_RX_DATA
		{
			if (!rxFifo.isEmpty())
			{
				return rxFifo.pop();
			}
			else
			{
				return 0;
			}
		}
		default: logging::fatal("unhandled 8 bit joypad read" + helpers::intToHex(addr), logging::logSource::Joypad); return 0;
	}
}

void joypad::sendByte(uint8_t value)
{
	switch (communicationSequenceIndex)
	{
		case 0: byteReceived(0x41); communicationSequenceIndex++; break; // 5A41h = digital pad ID
		case 1: byteReceived(0x5A); communicationSequenceIndex++; break;
		case 2: byteReceived(buttonState >> 8); communicationSequenceIndex++; break;
		case 3: byteReceived(buttonState & 0xFF); communicationSequenceIndex = 0; break;
	}
}

void joypad::byteReceived(uint8_t value)
{
	rxFifo.push(value);
	if (rxInterruptEnable)
	{
		uint8_t fifoSizeForInterrupt = 0;
		switch (rxInterruptMode)
		{
			case 0: fifoSizeForInterrupt = 1; break;
			case 1: fifoSizeForInterrupt = 2; break;
			case 2: fifoSizeForInterrupt = 4; break;
			case 3: fifoSizeForInterrupt = 8; break;
		}
		if (rxFifo.numElements() == fifoSizeForInterrupt)
		{
			InterruptController->requestInterrupt(interruptType::CONTROLLERMEMCARD);
		}
	}
}

void joypad::keyChanged(SDL_Keycode key, bool value)
{
	auto buttonIterator = bindings.find(key);
	if (buttonIterator != bindings.end())
	{
		uint8_t button = (uint8_t)buttonIterator->second;
		buttonState &= ~(((uint16_t)!value) << button);
		buttonState |= (((uint16_t)!value) << button);
	}
}