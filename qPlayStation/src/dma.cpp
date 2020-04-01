#include "dma.hpp"

dma::dma(ram* r, gpu* g)
{
	RAM = r;
	GPU = g;
	for (int i = 0; i < 7; i++)
	{
		channels[i] = new dmaChannel();
	}
	reset();
}

dma::~dma()
{
	for (int i = 0; i < 7; i++)
	{
		delete(channels[i]);
	}
}

void dma::reset()
{
	control = 0x07654321;
	irq_enable = 0;
	irq_masterEnable = false;
	irq_flags = 0;
	irq_force = false;
	for (int i = 0; i < 7; i++)
	{
		channels[i]->setControl(0);
		channels[i]->setBase(0);
		channels[i]->setBlockCtrl(0);
	}
}

void dma::set32(uint32_t addr, uint32_t value)
{
	uint8_t channel = (addr >> 4) & 0xF;
	uint8_t reg = addr & 0xF;

	switch (channel)
	{
		case 0: case 1: case 2: case 3: case 4: case 5: case 6:
		{
			dmaChannel* chan = channels[channel];
			switch (reg)
			{
				case 0: chan->setBase(value); break;
				case 4: chan->setBlockCtrl(value); break;
				case 8: chan->setControl(value); break;
				default: logging::fatal("unimplemented DMA write" + helpers::intToHex(addr), logging::logSource::DMA); break;
			}
			if (chan->isActive())
			{
				doDMA(channel);
			}
			break;
		}
		case 7:
		{
			switch (reg)
			{
				case 0:
				{
					control = value;
					break;
				}
				case 4:
				{
					irq_force = value & (1 << 15);
					irq_enable = ((value >> 16) & 0x7F);
					irq_masterEnable = value & (1 << 23);

					// Writing 1 to a flag resets it
					irq_flags &= ~((value >> 24) & 0x3F);
					break;
				}
				default: logging::fatal("unimplemented DMA write" + helpers::intToHex(addr), logging::logSource::DMA); break;
			}
			break;
		}
		default: logging::fatal("unimplemented DMA write" + helpers::intToHex(addr), logging::logSource::DMA); break;
	}
}

uint32_t dma::get32(uint32_t addr)
{
	uint8_t channel = (addr >> 4) & 0xF;
	uint8_t reg = addr & 0xF;

	switch (channel)
	{
		case 0: case 1: case 2: case 3: case 4: case 5: case 6:
		{
			dmaChannel* chan = channels[channel];
			switch (reg)
			{
				case 0: return chan->getBase();
				case 4: return chan->getBlockCtrl();
				case 8: return chan->getControl();
				default: logging::fatal("unimplemented DMA read" + helpers::intToHex(addr), logging::logSource::DMA); return 0;
			}
			break;
		}
		case 7:
		{
			switch (reg)
			{
				case 0:
				{
					return control;
				}
				case 4:
				{
					bool anyIRQ = irq_force || (irq_masterEnable && ((irq_flags & irq_enable) != 0));
					return (((uint32_t)irq_force) << 15) |
						(((uint32_t)irq_enable) << 16) |
						(((uint32_t)irq_masterEnable) << 23) |
						(((uint32_t)irq_flags) << 24) |
						(((uint32_t)anyIRQ) << 31);
				}
				default: logging::fatal("unimplemented DMA read" + helpers::intToHex(addr), logging::logSource::DMA); return 0;
			}
			break;
		}
		default: logging::fatal("unimplemented DMA read" + helpers::intToHex(addr), logging::logSource::DMA); return 0;
	}
}

void dma::set16(uint32_t addr, uint16_t value) { logging::fatal("unimplemented 16 bit DMA write" + helpers::intToHex(addr), logging::logSource::DMA); }
uint16_t dma::get16(uint32_t addr) { logging::fatal("unimplemented 16 bit DMA read" + helpers::intToHex(addr), logging::logSource::DMA); return 0; }
void dma::set8(uint32_t addr, uint8_t value) { logging::fatal("unimplemented 8 bit DMA write " + helpers::intToHex(addr), logging::logSource::DMA); }
uint8_t dma::get8(uint32_t addr) { logging::fatal("unimplemented 8 bit DMA read" + helpers::intToHex(addr), logging::logSource::DMA); return 0; }

void dma::doDMA(uint8_t port)
{
	dmaChannel* chan = channels[port];
	if ((*chan).syncMode == 2) // Linked List Copy
	{
		if (port != 2)
		{
			logging::fatal("can't do linked list copy to device that's not the GPU", logging::logSource::DMA);
		}
		if ((*chan).direction == false)
		{
			logging::fatal("can't do linked list copy from Device to RAM", logging::logSource::DMA);
		}
		uint32_t currentAddr = chan->getBase() & 0x1FFFFC;
		bool atEnd = false;
		while (!atEnd)
		{
			// First byte of header = number of words in the packet
			// Remaining 24 bits - address of next packet
			uint32_t header = RAM->get32(currentAddr);
			uint8_t numWords = header >> 24;

			while (numWords > 0)
			{
				currentAddr = (currentAddr + 4) & 0x1FFFFC;
				GPU->set32(0, RAM->get32(currentAddr));
				numWords--;
			}

			// End of table marker is supposed to be 0xFFFFFF but it seems that only the top bit matters
			if (header & 0x800000)
			{
				atEnd = true;
			}
			currentAddr = header & 0x1FFFFC;
		}
	}
	else // Block Copy
	{
		uint32_t addrIncrement = (*chan).addrMode ? -4 : 4;
		uint32_t currentAddr = chan->getBase();
		uint32_t wordsToTransfer = 0;

		switch ((*chan).syncMode)
		{
			case 0: wordsToTransfer = (*chan).blockSize; break;
			case 1: wordsToTransfer = (*chan).blockSize * (*chan).blockCount; break;
		}

		while (wordsToTransfer > 0)
		{
			uint32_t adjAddr = currentAddr & 0x1FFFFC;
			if ((*chan).direction) // RAM to Device
			{
				uint32_t srcWord = RAM->get32(adjAddr);
				switch (port)
				{
					case (uint8_t)dmaPort::GPU:
					{
						GPU->set32(0, srcWord);
						break;
					}
					default: logging::fatal("unhandled DMA (RAM to Device) port: " + std::to_string(port), logging::logSource::DMA);
				}
			}
			else // Device to RAM
			{
				uint32_t srcWord = 0;
				switch (port)
				{
					case (uint8_t)dmaPort::OTC:
					{
						srcWord = (wordsToTransfer == 1) ? 0xFFFFFF : (currentAddr - 4) & 0x1FFFFF;
						break;
					}
					case (uint8_t)dmaPort::GPU:
					{
						srcWord = GPU->get32(0);
						break;
					}
					default: logging::fatal("unhandled DMA (device to RAM) port: " + std::to_string(port), logging::logSource::DMA);
				}
				RAM->set32(adjAddr, srcWord);
			}
			currentAddr += addrIncrement;
			wordsToTransfer--;
		}
	}
	(*chan).enabled = false;
	(*chan).trigger = false;
}

void dmaChannel::setControl(uint32_t value)
{
	direction = value & 1;
	addrMode = value & (1 << 1);
	chop = value & (1 << 8);
	syncMode = (value >> 9) & 0x3;
	if (syncMode > 2)
	{
		logging::fatal("Invalid DMA sync mode", logging::logSource::DMA);
	}
	chopDMAsize = ((value >> 16) & 0x7);
	chopCPUsize = ((value >> 20) & 0x7);
	enabled = value & (1 << 24);
	trigger = value & (1 << 28);
}

uint32_t dmaChannel::getControl()
{
	return ((uint32_t)direction) |
		(((uint32_t)addrMode) << 1) |
		(((uint32_t)chop) << 8) |
		(((uint32_t)syncMode) << 9) |
		(((uint32_t)chopDMAsize) << 16) |
		(((uint32_t)chopCPUsize) << 20) |
		(((uint32_t)enabled) << 24) |
		(((uint32_t)trigger) << 28);
}

void dmaChannel::setBase(uint32_t value)
{
	baseAddr = value & 0xFFFFFF;
}

uint32_t dmaChannel::getBase()
{
	return baseAddr;
}

void dmaChannel::setBlockCtrl(uint32_t value)
{
	blockSize = value & 0xFFFF;
	blockCount = (value >> 16) & 0xFFFF;
}

uint32_t dmaChannel::getBlockCtrl()
{
	return (((uint32_t)blockCount) << 16) | ((uint32_t)blockSize);
}

bool dmaChannel::isActive()
{
	if (syncMode == 0)
	{
		return enabled && trigger;
	}
	else
	{
		return enabled;
	}
}