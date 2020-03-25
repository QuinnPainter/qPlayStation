#pragma once
#include "helpers.hpp"
#include "peripheral.hpp"

struct Position
{
	GLshort x;
	GLshort y;

	static Position fromGP0(uint32_t value)
	{
		return { (GLshort)(value & 0xFFFF), (GLshort)(value >> 16) };
	}
};

struct Colour
{
	GLubyte r;
	GLubyte g;
	GLubyte b;

	static Colour fromGP0(uint32_t value)
	{
		return { (GLubyte)(value & 0xFF), (GLubyte)((value >> 8) & 0xFF), (GLubyte)((value >> 16) & 0xFF) };
	}
};

#define VERTEX_BUFFER_LEN 65536
template <class T> struct Buffer
{
	GLuint bufObject;
	T* map;

	Buffer();
	~Buffer();
	void set(uint32_t index, T value);
};

enum class textureColourDepth : uint8_t
{
	texDepth4Bit = 0,
	texDepth8Bit = 1,
	texDepth15Bit = 2
};

enum class horizontalRes
{
	XRes256,
	XRes320,
	XRes512,
	XRes640,
	XRes368
};

enum class verticalRes : bool
{
	VRes240 = false,
	VRes480 = true
};

enum class videoMode : bool
{
	NTSC = false,	// 480i 60hz
	PAL = true		// 576i 50hz
};

enum class displayColourDepth : bool
{
	dispDepth15Bit = false,
	dispDepth24Bit = true
};

enum class dmaDirection : uint8_t
{
	Off = 0,
	FIFO = 1,
	CPUtoGP0 = 2,
	GPUREADtoCPU = 3,
};

enum class GP0Mode
{
	Command,
	CopyCPUtoVRAM
};

class gpu;

struct gp0Instruction
{
	int numArguments;
	void (gpu::* func)();
};

class gpu : public peripheral
{
	public:
		gpu(SDL_Window* window);
		~gpu();
		void reset();
		void display();
		void set32(uint32_t addr, uint32_t value);
		uint32_t get32(uint32_t addr);
		void set16(uint32_t addr, uint16_t value);
		uint16_t get16(uint32_t addr);
		void set8(uint32_t addr, uint8_t value);
		uint8_t get8(uint32_t addr);
	private:
		uint32_t gp0commandBuffer[12];
		int gp0commandBufferIndex;
		int gp0remainingCommands;
		gp0Instruction currentGP0Instruction;
		GP0Mode gp0Mode;
		gp0Instruction getGP0Instr(uint32_t value);

		uint8_t texPageXBase;
		uint8_t texPageYBase;
		uint8_t semiTransparency;
		textureColourDepth texPageColourDepth;
		bool dithering;
		bool canDrawToDisplay;
		bool setMask;
		bool preserveMaskedPixels;
		bool interlaceField;
		bool reverseFlag;
		bool texDisable;
		horizontalRes hRes;
		verticalRes vRes;
		videoMode vMode;
		displayColourDepth dispColourDepth;
		bool interlace;
		bool displayDisabled;
		bool interrupt;
		dmaDirection dmaDir;

		bool texturedRectangleXFlip;
		bool texturedRectangleYFlip;

		uint8_t textureWindowXMask;
		uint8_t textureWindowYMask;
		uint8_t textureWindowXOffset;
		uint8_t textureWindowYOffset;
		uint16_t drawingAreaLeft;
		uint16_t drawingAreaTop;
		uint16_t drawingAreaRight;
		uint16_t drawingAreaBottom;
		int16_t drawingXOffset;
		int16_t drawingYOffset;
		uint16_t displayVRAMXStart;
		uint16_t displayVRAMYStart;
		uint16_t displayHorizontalStart;
		uint16_t displayHorizontalEnd;
		uint16_t displayLineStart;
		uint16_t displayLineEnd;

		horizontalRes hResFromFields(uint8_t fields);
		uint8_t hResToFields(horizontalRes hr);

		// GP1 Display Control Commands
		void gp1_softReset();
		void gp1_resetCommandBuffer();
		void gp1_acknowledgeInterrupt();
		void gp1_displayEnable(uint32_t value);
		void gp1_dmaDirection(uint32_t value);
		void gp1_startDisplayArea(uint32_t value);
		void gp1_horizontalDisplayRange(uint32_t value);
		void gp1_verticalDisplayRange(uint32_t value);
		void gp1_displayMode(uint32_t value);

		// GP0 Render Commands
		void gp0_nop();
		void gp0_clearCache();
		void gp0_quad_mono_opaque();
		void gp0_quad_texture_blend_opaque();
		void gp0_tri_shaded_opaque();
		void gp0_quad_shaded_opaque();
		void gp0_copyRectCPUtoVRAM();
		void gp0_copyRectVRAMtoCPU();
		void gp0_drawModeSetting();
		void gp0_textureWindowSetting();
		void gp0_setDrawAreaTopLeft();
		void gp0_setDrawAreaBottomRight();
		void gp0_setDrawOffset();
		void gp0_maskBitSetting();

		// Renderer Stuff
		SDL_Window* sdlWindow;
		SDL_GLContext glContext;
		GLuint vertexArrayObject;
		GLuint vertexShader;
		GLuint fragmentShader;
		GLuint program;
		Buffer<Position>* positions;
		Buffer<Colour>* colours;
		uint32_t nVertices;
		void initOpenGL();
		GLuint compileShader(const char* str, GLenum shaderType);
		GLuint linkProgram(std::list<GLuint> shaders);
		GLuint findProgramAttrib(GLuint program, const char* attribute);
		void pushTriangle(Position p1, Position p2, Position p3, Colour c1, Colour c2, Colour c3);
		void pushQuad(Position p1, Position p2, Position p3, Position p4, Colour c1, Colour c2, Colour c3, Colour c4);
		void draw();
};