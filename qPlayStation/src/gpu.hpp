#pragma once
#include "helpers.hpp"
#include "peripheral.hpp"

enum class textureColourDepthValue : uint8_t
{
	texDepth4Bit = 0,
	texDepth8Bit = 1,
	texDepth15Bit = 2
};

enum class BlendMode : uint8_t
{
	NoTexture = 0,
	RawTexture = 1,
	BlendTexture = 2
};

#pragma pack(push, 1)
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

struct TexPage
{
	GLushort xBase;
	GLushort yBase;

	static TexPage fromGP0(uint32_t value)
	{
		GLushort x = ((value >> 16) & 0xF) * 64;
		GLushort y = (((value >> 16) >> 4) & 1) * 256;
		return { x, y };
	}
};

struct TexCoord
{
	GLubyte x;
	GLubyte y;

	static TexCoord fromGP0(uint32_t value)
	{
		return { value & 0xFF, (value >> 8) & 0xFF };
	}
};

struct ClutAttr
{
	GLushort x;
	GLushort y;

	static ClutAttr fromGP0(uint32_t value)
	{
		return { ((value >> 16) & 0x3F) * 16, ((value >> 16) >> 6) & 0x1FF };
	}
};

struct TextureColourDepth
{
	GLubyte depth;

	static TextureColourDepth fromGP0(uint32_t value)
	{
		return { ((value >> 16) >> 7) & 0x3 };
	}

	static TextureColourDepth fromValue(textureColourDepthValue value)
	{
		return { (GLubyte)value };
	}
};

struct Vertex
{
	Position position;
	Colour colour;
	TexPage texPage;
	TexCoord texCoord;
	ClutAttr clut;
	TextureColourDepth texDepth;
	GLubyte blendMode;

	Vertex(Position p, Colour c, TexPage t = { 0, 0 }, TexCoord tc = { 0, 0 }, ClutAttr ca = { 0, 0 }, TextureColourDepth td = { 0 }, GLubyte bm = 0)
	{
		position = p;
		colour = c;
		texPage = t;
		texCoord = tc;
		clut = ca;
		texDepth = td;
		blendMode = bm;
	}
};
#pragma pack(pop)

struct RectWidthHeight
{
	GLshort width;
	GLshort height;

	static Position fromGP0(uint32_t value)
	{
		return { (GLshort)(value & 0xFFFF), (GLshort)(value >> 16) };
	}
};

struct Rectangle
{
	Position position;
	Colour colour;
	RectWidthHeight widthHeight;
	TexCoord texCoord;
	ClutAttr clut;
	GLubyte blendMode;

	Rectangle(Position p, Colour c, RectWidthHeight wh, TexCoord tc = { 0, 0 }, ClutAttr ca = { 0, 0 }, GLubyte bm = 0)
	{
		position = p;
		colour = c;
		widthHeight = wh;
		texCoord = tc;
		clut = ca;
		blendMode = bm;
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
		void vramSet16(uint32_t addr, uint16_t value);
		uint16_t vramGet16(uint32_t addr);
		uint8_t* vram;
		uint32_t gpuReadLatch;
		uint32_t gp0commandBuffer[12];
		int gp0commandBufferIndex;
		int gp0remainingCommands;
		gp0Instruction currentGP0Instruction;
		GP0Mode gp0Mode;
		gp0Instruction getGP0Instr(uint32_t value);

		uint32_t vramTransferCurrentX;
		uint32_t vramTransferCurrentY;

		uint8_t texPageXBase;
		uint8_t texPageYBase;
		uint8_t semiTransparency;
		textureColourDepthValue texPageColourDepth;
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
		void gp1_gpuInfo(uint32_t value);

		// GP0 Render Commands
		void gp0_nop();
		void gp0_clearCache();
		void gp0_quad_mono_opaque();
		void gp0_quad_texture_blend_opaque();
		void gp0_tri_shaded_opaque();
		void gp0_quad_shaded_opaque();
		void gp0_rect_mono_1x1_opaque();
		void gp0_copyRectCPUtoVRAM();
		void gp0_copyRectVRAMtoCPU();
		void gp0_drawModeSetting();
		void gp0_textureWindowSetting();
		void gp0_setDrawAreaTopLeft();
		void gp0_setDrawAreaBottomRight();
		void gp0_setDrawOffset();
		void gp0_maskBitSetting();

		void texWindowInfoUpdated();

		// Renderer Stuff
		SDL_Renderer* sdlRenderer;
		SDL_Texture* screenTexture;
		uint8_t* glBuffer;

		SDL_Window* sdlWindow;
		SDL_GLContext glContext;
		GLuint vertexArrayObject;
		GLuint vertexShader;
		GLuint fragmentShader;
		GLuint program;
		GLuint vramTexture;
		GLint texWindowInfo;
		Buffer<Vertex>* vertices;
		uint32_t nVertices;
		void initOpenGL();
		GLuint compileShader(const char* str, GLenum shaderType);
		GLuint linkProgram(std::list<GLuint> shaders);
		void pushTriangle(Vertex v1, Vertex v2, Vertex v3);
		void pushQuad(Vertex v1, Vertex v2, Vertex v3, Vertex v4);
		void pushRect(Rectangle r);
		void draw();
};