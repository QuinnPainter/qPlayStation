#include "gpu.hpp"

gpu::gpu(SDL_Window* window)
{
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
}

void gpu::reset()
{
	texPageXBase = 0;
	texPageYBase = 0;
	semiTransparency = 0;
	texPageColourDepth = textureColourDepth::texDepth4Bit;
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

	gp1_resetCommandBuffer();
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
					// once vram exists, this should copy data into it
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
					logging::fatal("Unhandled GP1 opcode: Get GPU Info - " + helpers::intToHex(value), logging::logSource::GPU); break;
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
			logging::info("GPUREAD", logging::logSource::GPU);
			return 0;
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
		case 0x28: return { 5, &gpu::gp0_quad_mono_opaque };
		case 0x2C: return { 9, &gpu::gp0_quad_texture_blend_opaque };
		case 0x30: return { 6, &gpu::gp0_tri_shaded_opaque };
		case 0x38: return { 8, &gpu::gp0_quad_shaded_opaque };
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
	texPageColourDepth = textureColourDepth::texDepth4Bit;
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

// -------------------------- GP0 Render Commands --------------------------

void gpu::gp0_nop()
{
	// do nothing
}

void gpu::gp0_clearCache()
{
	// No cache yet
}

void gpu::gp0_quad_mono_opaque()
{
	Colour c = Colour::fromGP0(gp0commandBuffer[0]);
	pushQuad(Position::fromGP0(gp0commandBuffer[1]),
		Position::fromGP0(gp0commandBuffer[2]),
		Position::fromGP0(gp0commandBuffer[3]),
		Position::fromGP0(gp0commandBuffer[4]),
		c, c, c, c);
}

void gpu::gp0_quad_texture_blend_opaque()
{
	Colour c = Colour{ 255, 0, 255 };
	pushQuad(Position::fromGP0(gp0commandBuffer[1]),
		Position::fromGP0(gp0commandBuffer[3]),
		Position::fromGP0(gp0commandBuffer[5]),
		Position::fromGP0(gp0commandBuffer[7]),
		c, c, c, c);
}

void gpu::gp0_tri_shaded_opaque()
{
	pushTriangle(Position::fromGP0(gp0commandBuffer[1]),
		Position::fromGP0(gp0commandBuffer[3]),
		Position::fromGP0(gp0commandBuffer[5]),
		Colour::fromGP0(gp0commandBuffer[0]),
		Colour::fromGP0(gp0commandBuffer[2]),
		Colour::fromGP0(gp0commandBuffer[4]));
}

void gpu::gp0_quad_shaded_opaque()
{
	pushQuad(Position::fromGP0(gp0commandBuffer[1]),
		Position::fromGP0(gp0commandBuffer[3]),
		Position::fromGP0(gp0commandBuffer[5]),
		Position::fromGP0(gp0commandBuffer[7]),
		Colour::fromGP0(gp0commandBuffer[0]),
		Colour::fromGP0(gp0commandBuffer[2]),
		Colour::fromGP0(gp0commandBuffer[4]),
		Colour::fromGP0(gp0commandBuffer[6]));
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
	// No VRAM yet
}

void gpu::gp0_drawModeSetting()
{
	uint32_t value = gp0commandBuffer[0];
	texPageXBase = value & 0xF;
	texPageYBase = (value >> 4) & 1;
	semiTransparency = (value >> 5) & 3;
	texPageColourDepth = (textureColourDepth)((value >> 7) & 3);
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

void gpu::initOpenGL()
{
	const char* vertexShaderSrc =
		"#version 330\n"
		"in ivec2 vertex_position;\n"
		"in uvec3 vertex_color;\n"
		"out vec3 v_color;\n"
		"void main() {\n"
		"	float xpos = (float(vertex_position.x) / 512) - 1.0;\n"
		"	float ypos = 1.0 - (float(vertex_position.y) / 256);\n"
		"	gl_Position.xyzw = vec4(xpos, ypos, 0.0, 1.0);\n"
		"	v_color = vec3(float(vertex_color.r) / 255, float(vertex_color.g) / 255, float(vertex_color.b) / 255);\n"
		"}\n";

	const char* fragmentShaderSrc =
		"#version 330\n"
		"in vec3 v_color;\n"
		"out vec4 o_color;\n"
		"void main() {\n"
		"	o_color = vec4(v_color, 1.0);\n"
		"}\n";

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	glContext = SDL_GL_CreateContext(sdlWindow);
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		logging::fatal("GLEW init error: " + std::string((char*)glewGetErrorString(err)), logging::logSource::GPU);
	}

	vertexShader = compileShader(vertexShaderSrc, GL_VERTEX_SHADER);
	fragmentShader = compileShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);

	program = linkProgram(std::list<GLuint>{vertexShader, fragmentShader});

	glUseProgram(program);

	glDisable(GL_DEPTH_TEST);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glViewport(0, 0, 1024, 512);

	glGenVertexArrays(1, &vertexArrayObject);
	glBindVertexArray(vertexArrayObject);

	// Setup position attribute
	positions = new Buffer<Position>();
	GLuint index = findProgramAttrib(program, "vertex_position");
	glEnableVertexAttribArray(index);
	glVertexAttribIPointer(index, 2, GL_SHORT, 0, nullptr);

	// Setup colour attribute
	colours = new Buffer<Colour>();
	index = findProgramAttrib(program, "vertex_color");
	glEnableVertexAttribArray(index);
	glVertexAttribIPointer(index, 3, GL_UNSIGNED_BYTE, 0, nullptr);

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
		logging::fatal("Shader compilation failed", logging::logSource::GPU);
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

GLuint gpu::findProgramAttrib(GLuint program, const char* attribute)
{
	int index = glGetAttribLocation(program, attribute);
	if (index < 0)
	{
		logging::fatal("Attribute: \"" + std::string(attribute) + "\" not found", logging::logSource::GPU);
	}
	return (GLuint)index;
}

void gpu::pushTriangle(Position p1, Position p2, Position p3, Colour c1, Colour c2, Colour c3)
{
	if (nVertices + 3 > VERTEX_BUFFER_LEN)
	{
		logging::warning("Vertex buffers full, forcing draw", logging::logSource::GPU);
		draw();
	}
	positions->set(nVertices, p1);
	colours->set(nVertices, c1);
	nVertices++;
	positions->set(nVertices, p2);
	colours->set(nVertices, c2);
	nVertices++;
	positions->set(nVertices, p3);
	colours->set(nVertices, c3);
	nVertices++;
}

void gpu::pushQuad(Position p1, Position p2, Position p3, Position p4, Colour c1, Colour c2, Colour c3, Colour c4)
{
	pushTriangle(p1, p2, p3, c1, c2, c3);
	pushTriangle(p2, p3, p4, c2, c3, c4);
}

void gpu::draw()
{
	if (nVertices == 0) { return; }
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
	for (uint32_t i = 0; i < 2048 * 512; i += 2)
	{
		uint32_t row = i / 2048;
		uint32_t col = i % 2048;
		row = (512 - row) - 1;
		if (glBuffer[i + 1] & 0x80)
		{
			vram[(row * 2048) + col] = glBuffer[i];
			vram[(row * 2048) + col + 1] = glBuffer[i + 1];
		}
	}
	SDL_UpdateTexture(screenTexture, NULL, vram, 2048);
	SDL_RenderCopy(sdlRenderer, screenTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);
}