#include "gpu.hpp"

gpu::gpu(SDL_Window* window, interruptController* i)
{
	InterruptController = i;
	sdlWindow = window;
	vram = new uint8_t[2048 * 512];
	glBuffer = new uint8_t[2048 * 512];
	memset(vram, 0, 2048 * 512);

	sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED/* | SDL_RENDERER_PRESENTVSYNC*/);
	if (sdlRenderer == NULL)
	{
		logging::fatal("Renderer could not be created! SDL_Error: " + std::string(SDL_GetError()), logging::logSource::GPU);
	}
	screenTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING, 1024, 512);

	initOpenGL();
	reset();
}

gpu::~gpu()
{
	glDeleteVertexArrays(1, &vertexArrayObject);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	glDeleteProgram(program);
	SDL_GL_DeleteContext(glContext);
	delete[] vram;
	delete[] glBuffer;
}

void gpu::reset()
{
	texPageXBase = 0;
	texPageYBase = 0;
	semiTransparency = 0;
	texPageColourDepth = textureColourDepthValue::texDepth4Bit;
	dithering = false;
	canDrawToDisplay = false;
	setMask = false;
	preserveMaskedPixels = false;
	interlaceField = false;
	reverseFlag = false;
	texDisable = false;
	hRes = hResFromFields(0);
	vRes = verticalRes::VRes240;
	vMode = videoMode::NTSC;
	dispColourDepth = displayColourDepth::dispDepth15Bit;
	interlace = false;
	displayDisabled = true;
	interrupt = false;
	dmaDir = dmaDirection::Off;

	texturedRectangleXFlip = false;
	texturedRectangleYFlip = false;

	textureWindowXMask = 0;
	textureWindowYMask = 0;
	textureWindowXOffset = 0;
	textureWindowYOffset = 0;
	drawingAreaLeft = 0;
	drawingAreaTop = 0;
	drawingAreaRight = 0;
	drawingAreaBottom = 0;
	drawingXOffset = 0;
	drawingYOffset = 0;
	displayVRAMXStart = 0;
	displayVRAMYStart = 0;
	displayHorizontalStart = 0;
	displayHorizontalEnd = 0;
	displayLineStart = 0;
	displayLineEnd = 0;

	gpuReadLatch = 0;

	gp1_resetCommandBuffer();

	texWindowInfoUpdated();
}

void gpu::set32(uint32_t addr, uint32_t value)
{
	switch (addr)
	{
		case 0: // GP0 - for draw commands (triangles, rects etc.)
		{
			if (gp0remainingCommands == 0)
			{
				currentGP0Instruction = getGP0Instr(value);
				gp0remainingCommands = currentGP0Instruction.numArguments;
				gp0commandBufferIndex = 0;
			}
			gp0remainingCommands--;
			switch (gp0Mode)
			{
				case GP0Mode::Command:
				{
					gp0commandBuffer[gp0commandBufferIndex++] = value;
					if (gp0remainingCommands == 0)
					{
						(this->*(currentGP0Instruction.func))();
					}
					break;
				}
				case GP0Mode::CopyCPUtoVRAM:
				{
					uint32_t destCoord = gp0commandBuffer[1];
					uint16_t destX = destCoord & 0xFFFF;
					uint16_t destY = destCoord >> 16;
					uint32_t widthheight = gp0commandBuffer[2];
					uint16_t width = widthheight & 0xFFFF;
					uint16_t height = widthheight >> 16;

					uint32_t destAddr = (vramTransferCurrentY * 2048) + (vramTransferCurrentX * 2);
					vramSet16(destAddr, value & 0xFFFF);
					vramTransferCurrentX++;
					if (vramTransferCurrentX >= ((uint32_t)destX) + width)
					{
						vramTransferCurrentX = destX;
						vramTransferCurrentY++;
					}

					if (!((((widthheight & 0xFFFF) * (widthheight >> 16)) & 1) && gp0remainingCommands == 0))
					{
						destAddr = (vramTransferCurrentY * 2048) + (vramTransferCurrentX * 2);
						vramSet16(destAddr, value >> 16);
						vramTransferCurrentX++;
						if (vramTransferCurrentX >= ((uint32_t)destX) + width)
						{
							vramTransferCurrentX = destX;
							vramTransferCurrentY++;
						}
					}

					if (gp0remainingCommands == 0)
					{
						gp0Mode = GP0Mode::Command;
					}
					break;
				}
				case GP0Mode::CopyVRAMtoCPU:
				{
					// Should commands be ignored during a VRAM to CPU transfer? hmm...
					break;
				}
			}
			break;
		}
		case 4: // GP1 - for GPU commands to set various GPU state stuff
		{
			switch ((value >> 24) & 0x3F)
			{
				case 0x00: gp1_softReset(); break;
				case 0x01: gp1_resetCommandBuffer(); break;
				case 0x02: gp1_acknowledgeInterrupt(); break;
				case 0x03: gp1_displayEnable(value); break;
				case 0x04: gp1_dmaDirection(value); break;
				case 0x05: gp1_startDisplayArea(value); break;
				case 0x06: gp1_horizontalDisplayRange(value); break;
				case 0x07: gp1_verticalDisplayRange(value); break;
				case 0x08: gp1_displayMode(value); break;
				case 0x09: logging::fatal("Unhandled GP1 opcode: Texture Disable - " + helpers::intToHex(value), logging::logSource::GPU); break;
				case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
					gp1_gpuInfo(value); break;
				default: logging::fatal("Invalid GP1 opcode: " + helpers::intToHex(value), logging::logSource::GPU); break;
			}
			break;
		}
	}
}

uint32_t gpu::get32(uint32_t addr)
{
	switch (addr)
	{
		case 0: // GPUREAD - for reading back info from the GPU to the CPU
		{
			if (gp0Mode != GP0Mode::CopyVRAMtoCPU)
			{
				return gpuReadLatch;
			}
			else
			{
				gp0remainingCommands--; // just use gp0remainingCommands for the remaining words to transfer

				uint32_t srcCoord = gp0commandBuffer[1];
				uint16_t srcX = srcCoord & 0xFFFF;
				uint16_t srcY = srcCoord >> 16;
				uint32_t widthheight = gp0commandBuffer[2];
				uint16_t width = widthheight & 0xFFFF;
				uint16_t height = widthheight >> 16;

				uint32_t srcAddr = (vramTransferCurrentY * 2048) + (vramTransferCurrentX * 2);
				uint16_t ret = vramGet16(srcAddr);
				vramTransferCurrentX++;
				if (vramTransferCurrentX >= ((uint32_t)srcX) + width)
				{
					vramTransferCurrentX = srcX;
					vramTransferCurrentY++;
				}

				if (!((((widthheight & 0xFFFF) * (widthheight >> 16)) & 1) && gp0remainingCommands == 0))
				{
					srcAddr = (vramTransferCurrentY * 2048) + (vramTransferCurrentX * 2);
					ret |= (vramGet16(srcAddr) << 16);
					vramTransferCurrentX++;
					if (vramTransferCurrentX >= ((uint32_t)srcX) + width)
					{
						vramTransferCurrentX = srcX;
						vramTransferCurrentY++;
					}
				}

				if (gp0remainingCommands == 0)
				{
					gp0Mode = GP0Mode::Command;
				}

				return ret;
			}
		}
		case 4: // GPUSTAT - GPU status register
		{
			uint32_t ret = 0;
			ret |= (uint32_t)texPageXBase;
			ret |= ((uint32_t)texPageYBase) << 4;
			ret |= ((uint32_t)semiTransparency) << 5;
			ret |= ((uint32_t)texPageColourDepth) << 7;
			ret |= ((uint32_t)dithering) << 9;
			ret |= ((uint32_t)canDrawToDisplay) << 10;
			ret |= ((uint32_t)setMask) << 11;
			ret |= ((uint32_t)preserveMaskedPixels) << 12;
			ret |= ((uint32_t)interlaceField) << 13;
			ret |= ((uint32_t)reverseFlag) << 14;
			ret |= ((uint32_t)texDisable) << 15;
			ret |= ((uint32_t)hResToFields(hRes)) << 16;
			ret |= ((uint32_t)vRes) << 19;
			ret |= ((uint32_t)vMode) << 20;
			ret |= ((uint32_t)dispColourDepth) << 21;
			ret |= ((uint32_t)interlace) << 22;
			ret |= ((uint32_t)displayDisabled) << 23;
			ret |= ((uint32_t)interrupt) << 24;
			// For now we pretend that the GPU is always ready:
			// Ready to receive command
			ret |= 1 << 26;
			// Ready to send VRAM to CPU
			ret |= 1 << 27;
			// Ready to receive DMA block
			ret |= 1 << 28;

			ret |= ((uint32_t)dmaDir) << 29;
			
			// should change based on line number
			ret |= 0 << 31;

			bool dmaRequest = false;
			switch (dmaDir)
			{
				case dmaDirection::Off: dmaRequest = false; break;
				case dmaDirection::FIFO: dmaRequest = true; break; // should be false if FIFO is full
				case dmaDirection::CPUtoGP0: dmaRequest = ret & (1 << 28); break;
				case dmaDirection::GPUREADtoCPU: dmaRequest = ret & (1 << 27); break;
			}
			ret |= ((uint32_t)dmaRequest) << 25;
			return ret;
		}
	}
}

void gpu::set16(uint32_t addr, uint16_t value) { logging::fatal("unimplemented 16 bit GPU write" + helpers::intToHex(addr), logging::logSource::GPU); }
uint16_t gpu::get16(uint32_t addr) { logging::fatal("unimplemented 16 bit GPU read" + helpers::intToHex(addr), logging::logSource::GPU); return 0; }
void gpu::set8(uint32_t addr, uint8_t value) { logging::fatal("unimplemented 8 bit GPU write " + helpers::intToHex(addr), logging::logSource::GPU); }
uint8_t gpu::get8(uint32_t addr) { logging::fatal("unimplemented 8 bit GPU read" + helpers::intToHex(addr), logging::logSource::GPU); return 0; }

void gpu::vramSet16(uint32_t addr, uint16_t value)
{
	vram[addr] = value & 0xFF;
	vram[addr + 1] = (value >> 8) & 0xFF;
}

uint16_t gpu::vramGet16(uint32_t addr)
{
	return vram[addr] | (vram[addr + 1] << 8);
}

gp0Instruction gpu::getGP0Instr(uint32_t value)
{
	switch (value >> 24)
	{
		case 0x00: return { 1, &gpu::gp0_nop };
		case 0x01: return { 1, &gpu::gp0_clearCache };
		case 0x02: return { 3, &gpu::gp0_fillRectVRAM };
		case 0x1F: return { 1, &gpu::gp0_interruptRequest };
		case 0x28: return { 5, &gpu::gp0_quad_mono_opaque };
		case 0x2C: return { 9, &gpu::gp0_quad_texture_blend_opaque };
		case 0x30: return { 6, &gpu::gp0_tri_shaded_opaque };
		case 0x38: return { 8, &gpu::gp0_quad_shaded_opaque };
		case 0x3C: return { 12, &gpu::gp0_quad_shaded_texture_blend_opaque };
		case 0x60: return { 3, &gpu::gp0_rect_mono_opaque };
		case 0x64: return { 4, &gpu::gp0_rect_texture_blend_opaque };
		case 0x68: return { 2, &gpu::gp0_rect_mono_1x1_opaque };
		case 0x74: return { 3, &gpu::gp0_rect_texture_blend_8x8_opaque };
		case 0xA0: return { 3, &gpu::gp0_copyRectCPUtoVRAM };
		case 0xC0: return { 3, &gpu::gp0_copyRectVRAMtoCPU };
		case 0xE1: return { 1, &gpu::gp0_drawModeSetting };
		case 0xE2: return { 1, &gpu::gp0_textureWindowSetting };
		case 0xE3: return { 1, &gpu::gp0_setDrawAreaTopLeft };
		case 0xE4: return { 1, &gpu::gp0_setDrawAreaBottomRight };
		case 0xE5: return { 1, &gpu::gp0_setDrawOffset };
		case 0xE6: return { 1, &gpu::gp0_maskBitSetting };
		default: logging::fatal("Unhandled GP0 opcode: " + helpers::intToHex(value), logging::logSource::GPU); return { 1, &gpu::gp0_nop };
	}
}

horizontalRes gpu::hResFromFields(uint8_t fields)
{
	if (fields & 1)
	{
		return horizontalRes::XRes368;
	}
	else
	{
		switch ((fields >> 1) & 0x3)
		{
			case 0: return horizontalRes::XRes256;
			case 1: return horizontalRes::XRes320;
			case 2: return horizontalRes::XRes512;
			case 3: return horizontalRes::XRes640;
		}
	}
}

uint8_t gpu::hResToFields(horizontalRes hr)
{
	switch (hr)
	{
		case horizontalRes::XRes256: return 0;
		case horizontalRes::XRes320: return 2;
		case horizontalRes::XRes512: return 4;
		case horizontalRes::XRes640: return 6;
		case horizontalRes::XRes368: return 1;
	}
}

// -------------------------- GP1 Display Control Commands --------------------------

void gpu::gp1_softReset()
{
	texPageXBase = 0;
	texPageYBase = 0;
	semiTransparency = 0;
	texPageColourDepth = textureColourDepthValue::texDepth4Bit;
	dithering = false;
	canDrawToDisplay = false;
	texDisable = false;
	texturedRectangleXFlip = false;
	texturedRectangleYFlip = false;
	setMask = false;
	preserveMaskedPixels = false;
	dmaDir = dmaDirection::Off;
	displayDisabled = true;
	hRes = hResFromFields(0);
	vRes = verticalRes::VRes240;
	vMode = videoMode::NTSC;
	interlace = true;
	dispColourDepth = displayColourDepth::dispDepth15Bit;

	textureWindowXMask = 0;
	textureWindowYMask = 0;
	textureWindowXOffset = 0;
	textureWindowYOffset = 0;
	drawingAreaLeft = 0;
	drawingAreaTop = 0;
	drawingAreaRight = 0;
	drawingAreaBottom = 0;
	drawingXOffset = 0;
	drawingYOffset = 0;
	displayVRAMXStart = 0;
	displayVRAMYStart = 0;
	displayHorizontalStart = 0x200;
	displayHorizontalEnd = 0xC00;
	displayLineStart = 0x10;
	displayLineEnd = 0x100;

	texWindowInfoUpdated();

	gp1_resetCommandBuffer();
	//should also clear command FIFO and texture cache
}

void gpu::gp1_resetCommandBuffer()
{
	gp0commandBufferIndex = 0;
	gp0remainingCommands = 0;
	gp0Mode = GP0Mode::Command;

	//should also clear FIFO
}

void gpu::gp1_acknowledgeInterrupt()
{
	interrupt = false;
}

void gpu::gp1_displayEnable(uint32_t value)
{
	displayDisabled = value & 1;
}

void gpu::gp1_dmaDirection(uint32_t value)
{
	dmaDir = (dmaDirection)(value & 3);
}

void gpu::gp1_startDisplayArea(uint32_t value)
{
	displayVRAMXStart = value & 0x3FC;
	displayVRAMYStart = (value >> 10) & 0x1FF;
}

void gpu::gp1_horizontalDisplayRange(uint32_t value)
{
	displayHorizontalStart = value & 0xFFF;
	displayHorizontalEnd = (value >> 12) & 0xFFF;
}

void gpu::gp1_verticalDisplayRange(uint32_t value)
{
	displayLineStart = value & 0xFFF;
	displayLineEnd = (value >> 12) & 0xFFF;
}

void gpu::gp1_displayMode(uint32_t value)
{
	hRes = hResFromFields(((value & 3) << 1) | ((value >> 6) & 1));
	vRes = (verticalRes)(value & 0x4);
	vMode = (videoMode)(value & 0x8);
	dispColourDepth = (displayColourDepth)(value & 0x10);
	interlace = value & 0x20;
	reverseFlag = value & 0x80;
}

void gpu::gp1_gpuInfo(uint32_t value)
{
	switch (value & 0x7)
	{
		case 2: gpuReadLatch = textureWindowXMask |
			(((uint32_t)textureWindowYMask) << 5) |
			(((uint32_t)textureWindowXOffset) << 10) |
			(((uint32_t)textureWindowYOffset) << 15);
		case 3: gpuReadLatch = drawingAreaLeft | (((uint32_t)drawingAreaTop) << 10);
		case 4: gpuReadLatch = drawingAreaRight | (((uint32_t)drawingAreaBottom) << 10);
		case 5: gpuReadLatch = drawingXOffset | (((uint32_t)drawingYOffset) << 11);
	}
}

// -------------------------- GP0 Render Commands --------------------------

void gpu::gp0_nop()
{
	// do nothing
}

void gpu::gp0_clearCache()
{
	// No cache yet
}

void gpu::gp0_fillRectVRAM()
{
	uint32_t colour24 = gp0commandBuffer[0] & 0xFFFFFF;
	uint16_t r = (colour24 & 0xFF) >> 3;
	uint16_t g = ((colour24 >> 8) & 0xFF) >> 3;
	uint16_t b = ((colour24 >> 16) & 0xFF) >> 3;
	uint16_t colour15 = r | (g << 5) | (b << 10); // should this be RGB, or BGR...

	uint16_t left = gp0commandBuffer[1] & 0x3F0;
	uint16_t top = (gp0commandBuffer[1] >> 16) & 0x1FF;
	uint16_t width = gp0commandBuffer[2] & 0x7FF;
	uint16_t height = (gp0commandBuffer[2] >> 16) & 0x1FF;

	if (width == 0x400)
	{
		width = 0;
	}
	else
	{
		width = ((width + 0xF) & 0x3F0); // round up to multiples of 0x10
	}

	// these should wrap, but instead I'm just clamping them to the edges
	if (left + width > 0x400)
	{
		width = 0x400 - left;
	}
	if (top + height > 0x200)
	{
		height = 0x200 - top;
	}

	for (uint16_t line = top; line < top + height; line++)
	{
		for (uint16_t x = left; x < left + width; x++)
		{
			vram[(line * 2048) + (x * 2)] = colour15 & 0xFF;
			vram[(line * 2048) + (x * 2) + 1] = colour15 >> 8;
		}
	}
}

void gpu::gp0_interruptRequest()
{
	InterruptController->requestInterrupt(interruptType::GPU);
}

void gpu::gp0_quad_mono_opaque()
{
	Colour c = Colour::fromGP0(gp0commandBuffer[0]);
	pushQuad({ Position::fromGP0(gp0commandBuffer[1]), c },
		{ Position::fromGP0(gp0commandBuffer[2]), c },
		{ Position::fromGP0(gp0commandBuffer[3]), c },
		{ Position::fromGP0(gp0commandBuffer[4]), c });
}

void gpu::gp0_quad_texture_blend_opaque()
{
	Colour c = Colour::fromGP0(gp0commandBuffer[0]);
	ClutAttr clut = ClutAttr::fromGP0(gp0commandBuffer[2]);
	TexPage texPage = TexPage::fromGP0(gp0commandBuffer[4]);
	TextureColourDepth texDepth = TextureColourDepth::fromGP0(gp0commandBuffer[4]);
	GLubyte blend = (GLubyte)BlendMode::BlendTexture;
	pushQuad({ Position::fromGP0(gp0commandBuffer[1]), c, texPage, TexCoord::fromGP0(gp0commandBuffer[2]), clut, texDepth, blend },
		{ Position::fromGP0(gp0commandBuffer[3]), c, texPage, TexCoord::fromGP0(gp0commandBuffer[4]), clut, texDepth, blend },
		{ Position::fromGP0(gp0commandBuffer[5]), c, texPage, TexCoord::fromGP0(gp0commandBuffer[6]), clut, texDepth, blend },
		{ Position::fromGP0(gp0commandBuffer[7]), c, texPage, TexCoord::fromGP0(gp0commandBuffer[8]), clut, texDepth, blend });
}

void gpu::gp0_tri_shaded_opaque()
{
	pushTriangle({ Position::fromGP0(gp0commandBuffer[1]), Colour::fromGP0(gp0commandBuffer[0]) },
		{ Position::fromGP0(gp0commandBuffer[3]), Colour::fromGP0(gp0commandBuffer[2]) },
		{ Position::fromGP0(gp0commandBuffer[5]), Colour::fromGP0(gp0commandBuffer[4]) });
}

void gpu::gp0_quad_shaded_opaque()
{
	pushQuad({ Position::fromGP0(gp0commandBuffer[1]), Colour::fromGP0(gp0commandBuffer[0]) },
		{ Position::fromGP0(gp0commandBuffer[3]), Colour::fromGP0(gp0commandBuffer[2]) },
		{ Position::fromGP0(gp0commandBuffer[5]), Colour::fromGP0(gp0commandBuffer[4]) },
		{ Position::fromGP0(gp0commandBuffer[7]), Colour::fromGP0(gp0commandBuffer[6]) });
}

void gpu::gp0_quad_shaded_texture_blend_opaque()
{
	ClutAttr clut = ClutAttr::fromGP0(gp0commandBuffer[2]);
	TexPage texPage = TexPage::fromGP0(gp0commandBuffer[5]);
	TextureColourDepth texDepth = TextureColourDepth::fromGP0(gp0commandBuffer[5]);
	GLubyte blend = (GLubyte)BlendMode::BlendTexture;
	pushQuad({ Position::fromGP0(gp0commandBuffer[1]), Colour::fromGP0(gp0commandBuffer[0]), texPage, TexCoord::fromGP0(gp0commandBuffer[2]), clut, texDepth, blend },
		{ Position::fromGP0(gp0commandBuffer[4]), Colour::fromGP0(gp0commandBuffer[3]), texPage, TexCoord::fromGP0(gp0commandBuffer[5]), clut, texDepth, blend },
		{ Position::fromGP0(gp0commandBuffer[7]), Colour::fromGP0(gp0commandBuffer[6]), texPage, TexCoord::fromGP0(gp0commandBuffer[8]), clut, texDepth, blend },
		{ Position::fromGP0(gp0commandBuffer[10]), Colour::fromGP0(gp0commandBuffer[9]), texPage, TexCoord::fromGP0(gp0commandBuffer[11]), clut, texDepth, blend });
}

void gpu::gp0_rect_mono_opaque()
{
	pushRect({ Position::fromGP0(gp0commandBuffer[1]), Colour::fromGP0(gp0commandBuffer[0]), RectWidthHeight::fromGP0(gp0commandBuffer[2]) });
}

void gpu::gp0_rect_texture_blend_opaque()
{
	pushRect({ Position::fromGP0(gp0commandBuffer[1]),
		Colour::fromGP0(gp0commandBuffer[0]),
		RectWidthHeight::fromGP0(gp0commandBuffer[3]),
		TexCoord::fromGP0(gp0commandBuffer[2]),
		ClutAttr::fromGP0(gp0commandBuffer[2]),
		(GLubyte)BlendMode::BlendTexture });
}

void gpu::gp0_rect_mono_1x1_opaque()
{
	pushRect({ Position::fromGP0(gp0commandBuffer[1]), Colour::fromGP0(gp0commandBuffer[0]), { 1, 1 } });
}

void gpu::gp0_rect_texture_blend_8x8_opaque()
{
	pushRect({ Position::fromGP0(gp0commandBuffer[1]),
		Colour::fromGP0(gp0commandBuffer[0]),
		{ 8, 8 },
		TexCoord::fromGP0(gp0commandBuffer[2]),
		ClutAttr::fromGP0(gp0commandBuffer[2]),
		(GLubyte)BlendMode::BlendTexture});
}

void gpu::gp0_copyRectCPUtoVRAM()
{
	uint32_t destCoord = gp0commandBuffer[1];
	uint16_t destX = destCoord & 0xFFFF;
	uint16_t destY = destCoord >> 16;
	uint32_t widthheight = gp0commandBuffer[2];
	uint32_t rectSize = (widthheight & 0xFFFF) * (widthheight >> 16);

	vramTransferCurrentX = destX;
	vramTransferCurrentY = destY;

	// Pixels are 16 bits, so account for the padding at the end if there's an odd number
	gp0remainingCommands = ((rectSize + 1) & ~1) / 2;
	gp0Mode = GP0Mode::CopyCPUtoVRAM;
}

void gpu::gp0_copyRectVRAMtoCPU()
{
	uint32_t srcCoord = gp0commandBuffer[1];
	uint16_t srcX = srcCoord & 0xFFFF;
	uint16_t srcY = srcCoord >> 16;
	uint32_t widthheight = gp0commandBuffer[2];
	uint32_t rectSize = (widthheight & 0xFFFF) * (widthheight >> 16);

	vramTransferCurrentX = srcX;
	vramTransferCurrentY = srcY;

	// Pixels are 16 bits, so account for the padding at the end if there's an odd number
	gp0remainingCommands = ((rectSize + 1) & ~1) / 2;
	gp0Mode = GP0Mode::CopyVRAMtoCPU;
}

void gpu::gp0_drawModeSetting()
{
	uint32_t value = gp0commandBuffer[0];
	texPageXBase = value & 0xF;
	texPageYBase = (value >> 4) & 1;
	semiTransparency = (value >> 5) & 3;
	texPageColourDepth = (textureColourDepthValue)((value >> 7) & 3);
	dithering = (value >> 9) & 1;
	canDrawToDisplay = (value >> 10) & 1;
	texDisable = (value >> 11) & 1;
	texturedRectangleXFlip = (value >> 12) & 1;
	texturedRectangleYFlip = (value >> 13) & 1;
}

void gpu::gp0_textureWindowSetting()
{
	uint32_t value = gp0commandBuffer[0];
	textureWindowXMask = value & 0x1F;
	textureWindowYMask = (value >> 5) & 0x1F;
	textureWindowXOffset = (value >> 10) & 0x1F;
	textureWindowYOffset = (value >> 15) & 0x1F;
	texWindowInfoUpdated();
}

void gpu::gp0_setDrawAreaTopLeft()
{
	uint32_t value = gp0commandBuffer[0];
	drawingAreaLeft = value & 0x3FF;
	drawingAreaTop = (value >> 10) & 0x3FF;
}

void gpu::gp0_setDrawAreaBottomRight()
{
	uint32_t value = gp0commandBuffer[0];
	drawingAreaRight = value & 0x3FF;
	drawingAreaBottom = (value >> 10) & 0x3FF;
}

void gpu::gp0_setDrawOffset()
{
	uint32_t value = gp0commandBuffer[0];
	uint16_t x = value & 0x7FF;
	uint16_t y = (value >> 11) & 0x7FF;
	drawingXOffset = ((int16_t)(x << 5)) >> 5; // force sign extend
	drawingYOffset = ((int16_t)(y << 5)) >> 5;
}

void gpu::gp0_maskBitSetting()
{
	uint32_t value = gp0commandBuffer[0];
	setMask = value & 1;
	preserveMaskedPixels = value & 2;
}

void gpu::texWindowInfoUpdated()
{
	glUniform4ui(texWindowInfo, textureWindowXMask, textureWindowXOffset, textureWindowYMask, textureWindowYOffset);
}

template <class T> Buffer<T>::Buffer()
{
	glGenBuffers(1, &bufObject);
	glBindBuffer(GL_ARRAY_BUFFER, bufObject);

	GLsizeiptr elementSize = sizeof(T);
	GLsizeiptr bufferSize = elementSize * VERTEX_BUFFER_LEN;

	glBufferStorage(GL_ARRAY_BUFFER, bufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
	map = (T*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bufferSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

	memset(map, 0, bufferSize);
}

template <class T> Buffer<T>::~Buffer()
{
	glBindBuffer(GL_ARRAY_BUFFER, bufObject);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glDeleteBuffers(1, &bufObject);
}

template <class T> void Buffer<T>::set(uint32_t index, T value)
{
	if (index >= VERTEX_BUFFER_LEN)
	{
		logging::fatal("Vertex buffer overflow", logging::logSource::GPU);
	}
	map[index] = value;
}

void GLAPIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	logging::warning(std::string(message), logging::logSource::GPU);
}

void gpu::initOpenGL()
{
	const char* vertexShaderSrc =
		"#version 330\n"
		"in ivec2 vertex_position;\n"
		"in uvec3 vertex_color;\n"
		"in uvec2 texture_page;\n"
		"in uvec2 texture_coord;\n"
		"in uvec2 clut;\n"
		"in uint texture_depth;\n"
		"in uint texture_blend_mode;\n"
		"out vec3 frag_color;\n"
		"flat out uvec2 frag_texture_page;\n"
		"out vec2 frag_texture_coord;\n"
		"flat out uvec2 frag_clut;\n"
		"flat out uint frag_texture_depth;\n"
		"flat out uint frag_blend_mode;\n"
		"void main() {\n"
		"	float xpos = (float(vertex_position.x) / 512) - 1.0;\n"
		"	float ypos = 1.0 - (float(vertex_position.y) / 256);\n"
		"	gl_Position.xyzw = vec4(xpos, ypos, 0.0, 1.0);\n"
		"	frag_color = vec3(float(vertex_color.r) / 255, float(vertex_color.g) / 255, float(vertex_color.b) / 255);\n"
		"	frag_texture_page = texture_page;\n"
		"	frag_texture_coord = vec2(texture_coord);\n"
		"	frag_clut = clut;\n"
		"	frag_texture_depth = texture_depth;\n"
		"	frag_blend_mode = texture_blend_mode;\n"
		"}\n";

	const char* fragmentShaderSrc =
		"#version 330\n"
		"uniform sampler2D vramTexture;\n"
		"uniform uvec4 texWindowInfo;\n"
		"in vec3 frag_color;\n"
		"flat in uvec2 frag_texture_page;\n"
		"in vec2 frag_texture_coord;\n"
		"flat in uvec2 frag_clut;\n"
		"flat in uint frag_texture_depth;\n"
		"flat in uint frag_blend_mode;\n"
		"out vec4 o_color;\n"
		"const uint BLEND_MODE_NO_TEXTURE = 0U;\n"
		"const uint BLEND_MODE_RAW_TEXTURE = 1U;\n"
		"const uint BLEND_MODE_TEXTURE_BLEND = 2U;\n"
		"vec4 vram_get_pixel(uint x, uint y) {\n"
		"	return texelFetch(vramTexture, ivec2(x & 0x3ffU, y & 0x1ffU), 0);\n"
		"}\n"
		"uint rebuild_psx_color(vec4 color) {\n"
		"	uint a = uint(floor(color.a + 0.5));\n"
		"	uint r = uint(floor(color.r * 31. + 0.5));\n"
		"	uint g = uint(floor(color.g * 31. + 0.5));\n"
		"	uint b = uint(floor(color.b * 31. + 0.5));\n"
		"	return (a << 15) | (b << 10) | (g << 5) | r;\n"
		"}\n"
		"void main() {\n"
		"	if (frag_blend_mode == BLEND_MODE_NO_TEXTURE) {\n"
		"		o_color = vec4(frag_color, 1.0);\n"
		"	} else {\n"
		"		uint frag_texture_depth_new = frag_texture_depth;\n"
		"		if ((frag_texture_depth & 1U) != 1U) { // Flip frag_texture_depth from 0, 1, 2 to 2, 1, 0\n"
		"			frag_texture_depth_new ^= 0x2U;\n"
		"		}\n"
		"		uint pix_per_hw = 1U << frag_texture_depth_new;\n"
		"		uint tex_x = uint(frag_texture_coord.x) & 0xffU;\n"
		"		uint tex_y = uint(frag_texture_coord.y) & 0xffU;\n"
		"		tex_x = (tex_x & (~(texWindowInfo.x << 3U))) | ((texWindowInfo.z & texWindowInfo.x) << 3U);\n"
		"		tex_y = (tex_y & (~(texWindowInfo.y << 3U))) | ((texWindowInfo.w & texWindowInfo.x) << 3U);\n"
		"		uint tex_x_pix = tex_x / pix_per_hw;\n"
		"		tex_x_pix += frag_texture_page.x;\n"
		"		tex_y += frag_texture_page.y;\n"
		"		vec4 texel = vram_get_pixel(tex_x_pix, tex_y);\n"
		"		if (frag_texture_depth_new > 0U) {\n"
		"			uint icolor = rebuild_psx_color(texel);\n"
		"			uint bpp = 16U >> frag_texture_depth_new;\n"
		"			uint mask = ((1U << bpp) - 1U);\n"
		"			uint align = tex_x & ((1U << frag_texture_depth_new) - 1U);\n"
		"			uint shift = (align * bpp);\n"
		"			uint index = (icolor >> shift) & mask;\n"
		"			uint clut_x = frag_clut.x + index;\n"
		"			uint clut_y = frag_clut.y;\n"
		"			texel = vram_get_pixel(clut_x, clut_y);\n"
		"		}\n"
		"		if (rebuild_psx_color(texel) == 0U) {\n"
		"			discard;\n"
		"		}\n"
		"		if (frag_blend_mode == BLEND_MODE_RAW_TEXTURE) {\n"
		"			o_color = vec4(texel.rgb, 1.0);\n"
		"		} else {\n"
		"			o_color = vec4(frag_color * 2. * texel.rgb, 1.0);\n"
		"		}\n"
		"	}\n"
		"}\n";

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	glContext = SDL_GL_CreateContext(sdlWindow);
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		logging::fatal("GLEW init error: " + std::string((char*)glewGetErrorString(err)), logging::logSource::GPU);
	}
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(GLDebugCallback, 0);

	vertexShader = compileShader(vertexShaderSrc, GL_VERTEX_SHADER);
	fragmentShader = compileShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);

	program = linkProgram(std::list<GLuint>{vertexShader, fragmentShader});

	glUseProgram(program);

	glDisable(GL_DEPTH_TEST);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glViewport(0, 0, 1024, 512);

	glGenVertexArrays(1, &vertexArrayObject);
	glBindVertexArray(vertexArrayObject);

	GLsizei stride = sizeof(Vertex);
	uint64_t offset = 0;
	vertices = new Buffer<Vertex>();

	glBindAttribLocation(program, 0, "vertex_position");
	glEnableVertexAttribArray(0);
	glVertexAttribIPointer(0, 2, GL_SHORT, stride, (void*)offset);
	offset += sizeof(Position);

	glBindAttribLocation(program, 1, "vertex_color");
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 3, GL_UNSIGNED_BYTE, stride, (void*)offset);
	offset += sizeof(Colour);

	glBindAttribLocation(program, 2, "texture_page");
	glEnableVertexAttribArray(2);
	glVertexAttribIPointer(2, 2, GL_UNSIGNED_SHORT, stride, (void*)offset);
	offset += sizeof(TexPage);

	glBindAttribLocation(program, 3, "texture_coord");
	glEnableVertexAttribArray(3);
	glVertexAttribIPointer(3, 2, GL_UNSIGNED_BYTE, stride, (void*)offset);
	offset += sizeof(TexCoord);

	glBindAttribLocation(program, 4, "clut");
	glEnableVertexAttribArray(4);
	glVertexAttribIPointer(4, 2, GL_UNSIGNED_SHORT, stride, (void*)offset);
	offset += sizeof(ClutAttr);

	glBindAttribLocation(program, 5, "texture_depth");
	glEnableVertexAttribArray(5);
	glVertexAttribIPointer(5, 1, GL_UNSIGNED_BYTE, stride, (void*)offset);
	offset += sizeof(TextureColourDepth);

	glBindAttribLocation(program, 6, "texture_blend_mode");
	glEnableVertexAttribArray(6);
	glVertexAttribIPointer(6, 1, GL_UNSIGNED_BYTE, stride, (void*)offset);
	offset += sizeof(GLubyte);

	glGenTextures(1, &vramTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, vramTexture);
	glUniform1i(glGetUniformLocation(program, "vramTexture"), 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	texWindowInfo = glGetUniformLocation(program, "texWindowInfo");

	nVertices = 0;
}

GLuint gpu::compileShader(const char* str, GLenum shaderType)
{
	GLuint shader = glCreateShader(shaderType);

	int length = (int)strlen(str);
	glShaderSource(shader, 1, (const GLchar**)&str, &length);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		logging::fatal("Shader compilation failed: " + std::string(str), logging::logSource::GPU);
	}
	return shader;
}

GLuint gpu::linkProgram(std::list<GLuint> shaders)
{
	GLuint program = glCreateProgram();
	for (GLuint shader : shaders)
	{
		glAttachShader(program, shader);
	}
	glLinkProgram(program);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		logging::fatal("OpenGL program linking failed", logging::logSource::GPU);
	}
	return program;
}

void gpu::pushTriangle(Vertex v1, Vertex v2, Vertex v3)
{
	if (nVertices + 3 > VERTEX_BUFFER_LEN)
	{
		logging::warning("Vertex buffers full, forcing draw", logging::logSource::GPU);
		draw();
	}
	vertices->set(nVertices, v1);
	nVertices++;
	vertices->set(nVertices, v2);
	nVertices++;
	vertices->set(nVertices, v3);
	nVertices++;
}

void gpu::pushQuad(Vertex v1, Vertex v2, Vertex v3, Vertex v4)
{
	pushTriangle(v1, v2, v3);
	pushTriangle(v2, v3, v4);
}

void gpu::pushRect(Rectangle r)
{
	// for widths and heights greater that 255, textures should repeat
	// right now, it's just being clamped
	Vertex v1 = { r.position, r.colour, { texPageXBase, texPageYBase }, r.texCoord, r.clut, TextureColourDepth::fromValue(texPageColourDepth), r.blendMode };
	Vertex v2 = { { r.position.x + r.widthHeight.width, r.position.y }, r.colour, { texPageXBase, texPageYBase }, { (GLubyte)(r.texCoord.x + (GLubyte)(r.widthHeight.width)), r.texCoord.y }, r.clut, TextureColourDepth::fromValue(texPageColourDepth), r.blendMode };
	Vertex v3 = { { r.position.x, r.position.y + r.widthHeight.height }, r.colour, { texPageXBase, texPageYBase }, { r.texCoord.x, (GLubyte)(r.texCoord.y + (GLubyte)r.widthHeight.height) }, r.clut, TextureColourDepth::fromValue(texPageColourDepth), r.blendMode };
	Vertex v4 = { { r.position.x + r.widthHeight.width, r.position.y + r.widthHeight.height }, r.colour, { texPageXBase, texPageYBase }, { (GLubyte)(r.texCoord.x + (GLubyte)r.widthHeight.width), (GLubyte)(r.texCoord.y + (GLubyte)r.widthHeight.height) }, r.clut, TextureColourDepth::fromValue(texPageColourDepth), r.blendMode };
	pushQuad(v1, v2, v3, v4);
}

void gpu::draw()
{
	if (nVertices == 0) { return; }
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 512, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, vram);
	glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, nVertices);

	// Wait for GPU (should probably change this later)
	GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	bool drawingDone = false;
	while (!drawingDone)
	{
		GLenum wait = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 10000000);
		if (wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED)
		{
			drawingDone = true;
		}
	}
	nVertices = 0;
}

void gpu::display()
{
	draw();
	//SDL_GL_SwapWindow(sdlWindow);
	glReadPixels(0, 0, 1024, 512, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, glBuffer);
	for (int32_t i = 0; i < 2048 * 512; i += 2)
	{
		int32_t row = i / 2048;
		int32_t col = i % 2048;
		row = (512 - row) - 1;
		if (glBuffer[i + 1] & 0x80)
		{
			int32_t yPos = row + drawingYOffset;
			int32_t xPos = col + (drawingXOffset * 2);
			if (xPos >= drawingAreaLeft * 2 && xPos <= drawingAreaRight * 2 && yPos >= drawingAreaTop && yPos <= drawingAreaBottom)
			{
				vram[(yPos * 2048) + xPos] = glBuffer[i];
				vram[(yPos * 2048) + xPos + 1] = glBuffer[i + 1];
			}
		}
	}
	SDL_UpdateTexture(screenTexture, NULL, vram, 2048);
	SDL_RenderCopy(sdlRenderer, screenTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);

	InterruptController->requestInterrupt(interruptType::VBLANK); // temporary
}