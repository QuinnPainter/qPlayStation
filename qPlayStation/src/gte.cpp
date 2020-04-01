#include "gte.hpp"

// The GTE is mostly pulled straight from mednafen.

typedef struct
{
    int16_t MX[3][3];
    int16_t dummy;
} gtematrix;

typedef struct
{
    union
    {
        struct
        {
            uint8_t R;
            uint8_t G;
            uint8_t B;
            uint8_t CD;
        };
        uint8_t Raw8[4];
    };
} gtergb;

typedef struct
{
    int16_t X;
    int16_t Y;
} gtexy;

static uint32_t CR[32];
static uint32_t FLAGS;

typedef union
{
    gtematrix All[4];
    int32_t Raw[4][5];
    int16_t Raw16[4][10];

    struct
    {
        gtematrix Rot;
        gtematrix Light;
        gtematrix Color;
        gtematrix AbbyNormal;
    };
} Matrices_t;

static Matrices_t Matrices;

static union
{
    int32_t All[4][4];	// Really only [4][3], but [4] to ease address calculation.

    struct
    {
        int32_t T[4];
        int32_t B[4];
        int32_t FC[4];
        int32_t Null[4];
    };
} CRVectors;

static int32_t OFX;
static int32_t OFY;
static uint16_t H;
static int16_t DQA;
static int32_t DQB;

static int16_t ZSF3;
static int16_t ZSF4;

// Begin DR
static int16_t Vectors[3][4];
static gtergb RGB;
static uint16_t OTZ;

static int16_t IR[4];

#define IR0 IR[0]
#define IR1 IR[1]
#define IR2 IR[2]
#define IR3 IR[3]

static gtexy XY_FIFO[4];
static uint16_t Z_FIFO[4];
static gtergb RGB_FIFO[3];
static int32_t MAC[4];
static uint32_t LZCS;
static uint32_t LZCR;

static uint32_t Reg23;
// end DR

static uint8_t Sat5(int16_t cc)
{
    if (cc < 0)
        cc = 0;
    if (cc > 0x1F)
        cc = 0x1F;
    return(cc);
}

//
// Newton-Raphson division table.  (Initialized at startup; do NOT save in save states!)
//
static uint8_t DivTable[0x100 + 1];
static uint32_t CalcRecip(uint16_t divisor)
{
    int32_t x = (0x101 + DivTable[(((divisor & 0x7FFF) + 0x40) >> 7)]);
    int32_t tmp = (((int32_t)divisor * -x) + 0x80) >> 8;
    int32_t tmp2 = ((x * (131072 + tmp)) + 0x80) >> 8;

    return(tmp2);
}

static unsigned MDFN_lzcount32_0UD(uint32_t v)
{
#if defined(__GNUC__) || defined(__clang__) || defined(__ICC) || defined(__INTEL_COMPILER)
    return __builtin_clz(v);
#elif defined(_MSC_VER)
    unsigned long idx;

    _BitScanReverse(&idx, v);

    return 31 ^ idx;
#else
    unsigned ret = 0;
    unsigned tmp;

    tmp = !(v & 0xFFFF0000) << 4; v <<= tmp; ret += tmp;
    tmp = !(v & 0xFF000000) << 3; v <<= tmp; ret += tmp;
    tmp = !(v & 0xF0000000) << 2; v <<= tmp; ret += tmp;
    tmp = !(v & 0xC0000000) << 1; v <<= tmp; ret += tmp;
    tmp = !(v & 0x80000000) << 0;            ret += tmp;

    return(ret);
#endif
}
static unsigned MDFN_lzcount32(uint32_t v) { return !v ? 32 : MDFN_lzcount32_0UD(v); }

static uint32_t uint32min(uint32_t a, uint32_t b)
{
    return (b < a) ? a : b;
}

void gte::reset()
{
    memset(CR, 0, sizeof(CR));

    memset(Matrices.All, 0, sizeof(Matrices.All));
    memset(CRVectors.All, 0, sizeof(CRVectors.All));
    OFX = 0;
    OFY = 0;
    H = 0;
    DQA = 0;
    DQB = 0;
    ZSF3 = 0;
    ZSF4 = 0;

    memset(Vectors, 0, sizeof(Vectors));
    memset(&RGB, 0, sizeof(RGB));
    OTZ = 0;
    IR0 = 0;
    IR1 = 0;
    IR2 = 0;
    IR3 = 0;

    memset(XY_FIFO, 0, sizeof(XY_FIFO));
    memset(Z_FIFO, 0, sizeof(Z_FIFO));
    memset(RGB_FIFO, 0, sizeof(RGB_FIFO));
    memset(MAC, 0, sizeof(MAC));
    LZCS = 0;
    LZCR = 0;

    Reg23 = 0;

    // GTE_Init
    for (uint32_t divisor = 0x8000; divisor < 0x10000; divisor += 0x80)
    {
        uint32_t xa = 512;
        for (unsigned i = 1; i < 5; i++)
        {
            xa = (xa * (1024 * 512 - ((divisor >> 7)* xa))) >> 18;
        }
        DivTable[(divisor >> 7) & 0xFF] = ((xa + 1) >> 1) - 0x101;
    }
    // To avoid a bounds limiting if statement in the emulation code:
    DivTable[0x100] = DivTable[0xFF];
}

void gte::writeCR(uint32_t which, uint32_t value)
{
    static const uint32_t mask_table[32] = {
        /* 0x00 */
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,

        /* 0x08 */
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,

        /* 0x10 */
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,

        /* 0x18 */
        0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0x0000FFFF, 0xFFFFFFFF, 0x0000FFFF, 0x0000FFFF, 0xFFFFFFFF
    };

    value &= mask_table[which];

    CR[which] = value | (CR[which] & ~mask_table[which]);

    if (which < 24)
    {
        int we = which >> 3;
        which &= 0x7;

        if (which >= 5)
        {
            CRVectors.All[we][which - 5] = value;
        }
        else
        {
            Matrices.Raw[we][which] = value;
        }
        return;
    }

    switch (which)
    {
    case 24:
        OFX = value;
        break;
    case 25:
        OFY = value;
        break;
    case 26:
        H = value;
        break;
    case 27:
        DQA = value;
        break;
    case 28:
        DQB = value;
        break;
    case 29:
        ZSF3 = value;
        break;
    case 30:
        ZSF4 = value;
        break;
    case 31:
        CR[31] = (value & 0x7ffff000) | ((value & 0x7f87e000) ? (1 << 31) : 0);
        break;
    }
}

uint32_t gte::readCR(uint32_t which)
{
    uint32_t ret = 0;

    switch (which)
    {
    default:
        ret = CR[which];
        if (which == 4 || which == 12 || which == 20) { ret = (int16_t)ret; }
        break;
    case 24:
        ret = OFX;
        break;
    case 25:
        ret = OFY;
        break;
    case 26:
        ret = (int16_t)H;
        break;
    case 27:
        ret = (int16_t)DQA;
        break;
    case 28:
        ret = DQB;
        break;
    case 29:
        ret = (int16_t)ZSF3;
        break;
    case 30:
        ret = (int16_t)ZSF4;
        break;
    case 31:
        ret = CR[31];
        break;
    }

    return(ret);
}

void gte::writeDR(uint32_t which, uint32_t value)
{
    switch (which & 0x1F)
    {
    case 0:
        Vectors[0][0] = value;
        Vectors[0][1] = value >> 16;
        break;

    case 1:
        Vectors[0][2] = value;
        break;

    case 2:
        Vectors[1][0] = value;
        Vectors[1][1] = value >> 16;
        break;

    case 3:
        Vectors[1][2] = value;
        break;

    case 4:
        Vectors[2][0] = value;
        Vectors[2][1] = value >> 16;
        break;

    case 5:
        Vectors[2][2] = value;
        break;

    case 6:
        RGB.R = value >> 0;
        RGB.G = value >> 8;
        RGB.B = value >> 16;
        RGB.CD = value >> 24;
        break;

    case 7:
        OTZ = value;
        break;

    case 8:
        IR0 = value;
        break;

    case 9:
        IR1 = value;
        break;

    case 10:
        IR2 = value;
        break;

    case 11:
        IR3 = value;
        break;

    case 12:
        XY_FIFO[0].X = value;
        XY_FIFO[0].Y = value >> 16;
        break;

    case 13:
        XY_FIFO[1].X = value;
        XY_FIFO[1].Y = value >> 16;
        break;

    case 14:
        XY_FIFO[2].X = value;
        XY_FIFO[2].Y = value >> 16;
        XY_FIFO[3].X = value;
        XY_FIFO[3].Y = value >> 16;
        break;

    case 15:
        XY_FIFO[3].X = value;
        XY_FIFO[3].Y = value >> 16;

        XY_FIFO[0] = XY_FIFO[1];
        XY_FIFO[1] = XY_FIFO[2];
        XY_FIFO[2] = XY_FIFO[3];
        break;

    case 16:
        Z_FIFO[0] = value;
        break;

    case 17:
        Z_FIFO[1] = value;
        break;

    case 18:
        Z_FIFO[2] = value;
        break;

    case 19:
        Z_FIFO[3] = value;
        break;

    case 20:
        RGB_FIFO[0].R = value;
        RGB_FIFO[0].G = value >> 8;
        RGB_FIFO[0].B = value >> 16;
        RGB_FIFO[0].CD = value >> 24;
        break;

    case 21:
        RGB_FIFO[1].R = value;
        RGB_FIFO[1].G = value >> 8;
        RGB_FIFO[1].B = value >> 16;
        RGB_FIFO[1].CD = value >> 24;
        break;

    case 22:
        RGB_FIFO[2].R = value;
        RGB_FIFO[2].G = value >> 8;
        RGB_FIFO[2].B = value >> 16;
        RGB_FIFO[2].CD = value >> 24;
        break;

    case 23:
        Reg23 = value;
        break;

    case 24:
        MAC[0] = value;
        break;

    case 25:
        MAC[1] = value;
        break;

    case 26:
        MAC[2] = value;
        break;

    case 27:
        MAC[3] = value;
        break;

    case 28:
        IR1 = ((value >> 0) & 0x1F) << 7;
        IR2 = ((value >> 5) & 0x1F) << 7;
        IR3 = ((value >> 10) & 0x1F) << 7;
        break;

    case 29:	// Read-only
        break;

    case 30:
        LZCS = value;
        LZCR = MDFN_lzcount32(value ^ ((int32_t)value >> 31));
        break;

    case 31:	// Read-only
        break;
    }
}

uint32_t gte::readDR(uint32_t which)
{
    uint32_t ret = 0;

    switch (which & 0x1F)
    {
    case 0:
        ret = (uint16_t)Vectors[0][0] | ((uint16_t)Vectors[0][1] << 16);
        break;
    case 1:
        ret = (int16_t)Vectors[0][2];
        break;
    case 2:
        ret = (uint16_t)Vectors[1][0] | ((uint16_t)Vectors[1][1] << 16);
        break;
    case 3:
        ret = (int16_t)Vectors[1][2];
        break;
    case 4:
        ret = (uint16_t)Vectors[2][0] | ((uint16_t)Vectors[2][1] << 16);
        break;
    case 5:
        ret = (int16_t)Vectors[2][2];
        break;
    case 6:
        ret = RGB.R | (RGB.G << 8) | (RGB.B << 16) | (RGB.CD << 24);
        break;
    case 7:
        ret = (uint16_t)OTZ;
        break;
    case 8:
        ret = (int16_t)IR0;
        break;
    case 9:
        ret = (int16_t)IR1;
        break;
    case 10:
        ret = (int16_t)IR2;
        break;
    case 11:
        ret = (int16_t)IR3;
        break;
    case 12:
        ret = (uint16_t)XY_FIFO[0].X | ((uint16_t)XY_FIFO[0].Y << 16);
        break;
    case 13:
        ret = (uint16_t)XY_FIFO[1].X | ((uint16_t)XY_FIFO[1].Y << 16);
        break;
    case 14:
        ret = (uint16_t)XY_FIFO[2].X | ((uint16_t)XY_FIFO[2].Y << 16);
        break;
    case 15:
        ret = (uint16_t)XY_FIFO[3].X | ((uint16_t)XY_FIFO[3].Y << 16);
        break;
    case 16:
        ret = (uint16_t)Z_FIFO[0];
        break;
    case 17:
        ret = (uint16_t)Z_FIFO[1];
        break;
    case 18:
        ret = (uint16_t)Z_FIFO[2];
        break;
    case 19:
        ret = (uint16_t)Z_FIFO[3];
        break;
    case 20:
        ret = RGB_FIFO[0].R | (RGB_FIFO[0].G << 8) | (RGB_FIFO[0].B << 16) | (RGB_FIFO[0].CD << 24);
        break;
    case 21:
        ret = RGB_FIFO[1].R | (RGB_FIFO[1].G << 8) | (RGB_FIFO[1].B << 16) | (RGB_FIFO[1].CD << 24);
        break;
    case 22:
        ret = RGB_FIFO[2].R | (RGB_FIFO[2].G << 8) | (RGB_FIFO[2].B << 16) | (RGB_FIFO[2].CD << 24);
        break;
    case 23:
        ret = Reg23;
        break;
    case 24:
        ret = MAC[0];
        break;
    case 25:
        ret = MAC[1];
        break;
    case 26:
        ret = MAC[2];
        break;
    case 27:
        ret = MAC[3];
        break;
    case 28:
    case 29:
        ret = Sat5(IR1 >> 7) | (Sat5(IR2 >> 7) << 5) | (Sat5(IR3 >> 7) << 10);
        break;
    case 30:
        ret = LZCS;
        break;
    case 31:
        ret = LZCR;
        break;
    }
    return(ret);
}

#define sign_x_to_s64(_bits, _value) (((int64_t)((uint64_t)(_value) << (64 - _bits))) >> (64 - _bits))
static int64_t A_MV(unsigned which, int64_t value)
{
    if (value >= (1LL << 43))
        FLAGS |= 1 << (30 - which);

    if (value < -(1LL << 43))
        FLAGS |= 1 << (27 - which);

    return sign_x_to_s64(44, value);
}

static int64_t F(int64_t value)
{
    if (value < -2147483648LL)
    {
        // flag set here
        FLAGS |= 1 << 15;
    }

    if (value > 2147483647LL)
    {
        // flag set here
        FLAGS |= 1 << 16;
    }
    return(value);
}


static int16_t Lm_B(unsigned int which, int32_t value, int lm)
{
    int32_t tmp = lm << 15;

    if (value < (-32768 + tmp))
    {
        // set flag here
        FLAGS |= 1 << (24 - which);
        value = -32768 + tmp;
    }

    if (value > 32767)
    {
        // Set flag here
        FLAGS |= 1 << (24 - which);
        value = 32767;
    }

    return(value);
}


static int16_t Lm_B_PTZ(unsigned int which, int32_t value, int32_t ftv_value, int lm)
{
    int32_t tmp = lm << 15;

    if (ftv_value < -32768)
    {
        FLAGS |= 1 << (24 - which);
    }

    if (ftv_value > 32767)
    {
        FLAGS |= 1 << (24 - which);
    }

    if (value < (-32768 + tmp))
    {
        value = -32768 + tmp;
    }

    if (value > 32767)
    {
        value = 32767;
    }

    return(value);
}

static uint8_t Lm_C(unsigned int which, int32_t value)
{
    if (value & ~0xFF)
    {
        // Set flag here
        FLAGS |= 1 << (21 - which);	// Tested with GPF

        if (value < 0)
            value = 0;

        if (value > 255)
            value = 255;
    }

    return(value);
}

static int32_t Lm_D(int32_t value, int unchained)
{
    // Not sure if we should have it as int64, or just chain on to and special case when the F flags are set.
    if (!unchained)
    {
        if (FLAGS & (1 << 15))
        {
            FLAGS |= 1 << 18;
            return(0);
        }

        if (FLAGS & (1 << 16))
        {
            FLAGS |= 1 << 18;
            return(0xFFFF);
        }
    }

    if (value < 0)
    {
        // Set flag here
        value = 0;
        FLAGS |= 1 << 18;	// Tested with AVSZ3
    }
    else if (value > 65535)
    {
        // Set flag here.
        value = 65535;
        FLAGS |= 1 << 18;	// Tested with AVSZ3
    }

    return(value);
}

static int32_t Lm_G(unsigned int which, int32_t value)
{
    if (value < -1024)
    {
        // Set flag here
        value = -1024;
        FLAGS |= 1 << (14 - which);
    }

    if (value > 1023)
    {
        // Set flag here.
        value = 1023;
        FLAGS |= 1 << (14 - which);
    }

    return(value);
}

// limit to 4096, not 4095
static int32_t Lm_H(int32_t value)
{
    if (value < 0)
    {
        value = 0;
        FLAGS |= 1 << 12;
    }

    if (value > 4096)
    {
        value = 4096;
        FLAGS |= 1 << 12;
    }

    return(value);
}

static void MAC_to_RGB_FIFO(void)
{
    RGB_FIFO[0] = RGB_FIFO[1];
    RGB_FIFO[1] = RGB_FIFO[2];
    RGB_FIFO[2].R = Lm_C(0, MAC[1] >> 4);
    RGB_FIFO[2].G = Lm_C(1, MAC[2] >> 4);
    RGB_FIFO[2].B = Lm_C(2, MAC[3] >> 4);
    RGB_FIFO[2].CD = RGB.CD;
}


static void MAC_to_IR(int lm)
{
    IR1 = Lm_B(0, MAC[1], lm);
    IR2 = Lm_B(1, MAC[2], lm);
    IR3 = Lm_B(2, MAC[3], lm);
}

static void MultiplyMatrixByVector(const gtematrix* matrix, const int16_t* v, const int32_t* crv, uint32_t sf, int lm)
{
    unsigned i;

    for (i = 0; i < 3; i++)
    {
        int64_t tmp;
        int32_t mulr[3];

        tmp = (uint64_t)(int64_t)crv[i] << 12;

        if (matrix == &Matrices.AbbyNormal)
        {
            if (i == 0)
            {
                mulr[0] = -((RGB.R << 4) * v[0]);
                mulr[1] = (RGB.R << 4) * v[1];
                mulr[2] = IR0 * v[2];
            }
            else
            {
                mulr[0] = (int16_t)CR[i] * v[0];
                mulr[1] = (int16_t)CR[i] * v[1];
                mulr[2] = (int16_t)CR[i] * v[2];
            }
        }
        else
        {
            mulr[0] = matrix->MX[i][0] * v[0];
            mulr[1] = matrix->MX[i][1] * v[1];
            mulr[2] = matrix->MX[i][2] * v[2];
        }

        tmp = A_MV(i, tmp + mulr[0]);
        if (crv == CRVectors.FC)
        {
            Lm_B(i, tmp >> sf, false);
            tmp = 0;
        }

        tmp = A_MV(i, tmp + mulr[1]);
        tmp = A_MV(i, tmp + mulr[2]);

        MAC[1 + i] = tmp >> sf;
    }


    MAC_to_IR(lm);
}


static void MultiplyMatrixByVector_PT(const gtematrix* matrix, const int16_t* v, const int32_t* crv, uint32_t sf, int lm)
{
    int64_t tmp[3];
    unsigned i;

    for (i = 0; i < 3; i++)
    {
        int32_t mulr[3];

        tmp[i] = (uint64_t)(int64_t)crv[i] << 12;

        mulr[0] = matrix->MX[i][0] * v[0];
        mulr[1] = matrix->MX[i][1] * v[1];
        mulr[2] = matrix->MX[i][2] * v[2];

        tmp[i] = A_MV(i, tmp[i] + mulr[0]);
        tmp[i] = A_MV(i, tmp[i] + mulr[1]);
        tmp[i] = A_MV(i, tmp[i] + mulr[2]);

        MAC[1 + i] = tmp[i] >> sf;
    }

    IR1 = Lm_B(0, MAC[1], lm);
    IR2 = Lm_B(1, MAC[2], lm);
    //printf("FTV: %08x %08x\n", crv[2], (uint32)(tmp[2] >> 12));
    IR3 = Lm_B_PTZ(2, MAC[3], tmp[2] >> 12, lm);

    Z_FIFO[0] = Z_FIFO[1];
    Z_FIFO[1] = Z_FIFO[2];
    Z_FIFO[2] = Z_FIFO[3];
    Z_FIFO[3] = Lm_D(tmp[2] >> 12, true);
}


#define DECODE_FIELDS							\
    const uint32_t sf = (instr & (1 << 19)) ? 12 : 0;		\
    const uint32_t mx = (instr >> 17) & 0x3;			\
    const uint32_t v_i = (instr >> 15) & 0x3;				\
    const int32_t* cv = CRVectors.All[(instr >> 13) & 0x3];	\
    const int lm = (instr >> 10) & 1;			\
    int16_t v[3];					\
    if(v_i == 3)							\
    {								\
       v[0] = IR1;							\
       v[1] = IR2;							\
       v[2] = IR3;							\
    }								\
    else								\
    {								\
       v[0] = Vectors[v_i][0];					\
       v[1] = Vectors[v_i][1];					\
       v[2] = Vectors[v_i][2];					\
    }


static int32_t SQR(uint32_t instr)
{
    DECODE_FIELDS;

    MAC[1] = ((IR1 * IR1) >> sf);
    MAC[2] = ((IR2 * IR2) >> sf);
    MAC[3] = ((IR3 * IR3) >> sf);

    MAC_to_IR(lm);

    return(5);
}

static int32_t MVMVA(uint32_t instr)
{
    DECODE_FIELDS;

    MultiplyMatrixByVector(&Matrices.All[mx], v, cv, sf, lm);

    return(8);
}

static unsigned CountLeadingZeroU16(uint16_t val)
{
    unsigned ret = 0;

    while (!(val & 0x8000) && ret < 16)
    {
        val <<= 1;
        ret++;
    }

    return ret;
}

static uint32_t Divide(uint32_t dividend, uint32_t divisor)
{
    //if((Z_FIFO[3] * 2) > H)
    if ((divisor * 2) > dividend)
    {
        unsigned shift_bias = CountLeadingZeroU16(divisor);

        dividend <<= shift_bias;
        divisor <<= shift_bias;

        return uint32min(0x1FFFF, ((uint64_t)dividend * CalcRecip(divisor | 0x8000) + 32768) >> 16);
    }
    else
    {
        FLAGS |= 1 << 17;
        return 0x1FFFF;
    }
}

static void TransformXY(int64_t h_div_sz)
{
    MAC[0] = F((int64_t)OFX + IR1 * h_div_sz) >> 16;
    XY_FIFO[3].X = Lm_G(0, MAC[0]);

    MAC[0] = F((int64_t)OFY + IR2 * h_div_sz) >> 16;
    XY_FIFO[3].Y = Lm_G(1, MAC[0]);

    XY_FIFO[0] = XY_FIFO[1];
    XY_FIFO[1] = XY_FIFO[2];
    XY_FIFO[2] = XY_FIFO[3];
}

static void TransformDQ(int64_t h_div_sz)
{
    MAC[0] = F((int64_t)DQB + DQA * h_div_sz);
    IR0 = Lm_H(((int64_t)DQB + DQA * h_div_sz) >> 12);
}

static int32_t RTPS(uint32_t instr)
{
    DECODE_FIELDS;
    int64_t h_div_sz;

    MultiplyMatrixByVector_PT(&Matrices.Rot, Vectors[0], CRVectors.T, sf, lm);
    h_div_sz = Divide(H, Z_FIFO[3]);

    TransformXY(h_div_sz);
    TransformDQ(h_div_sz);

    return(15);
}

static int32_t RTPT(uint32_t instr)
{
    DECODE_FIELDS;
    int i;

    for (i = 0; i < 3; i++)
    {
        int64_t h_div_sz;

        MultiplyMatrixByVector_PT(&Matrices.Rot, Vectors[i], CRVectors.T, sf, lm);
        h_div_sz = Divide(H, Z_FIFO[3]);

        TransformXY(h_div_sz);

        if (i == 2)
            TransformDQ(h_div_sz);
    }

    return(23);
}

static void NormColor(uint32_t sf, int lm, uint32_t v)
{
    int16_t tmp_vector[3];

    MultiplyMatrixByVector(&Matrices.Light, Vectors[v], CRVectors.Null, sf, lm);

    tmp_vector[0] = IR1; tmp_vector[1] = IR2; tmp_vector[2] = IR3;
    MultiplyMatrixByVector(&Matrices.Color, tmp_vector, CRVectors.B, sf, lm);

    MAC_to_RGB_FIFO();
}

static int32_t NCS(uint32_t instr)
{
    DECODE_FIELDS;

    NormColor(sf, lm, 0);

    return(14);
}

static int32_t NCT(uint32_t instr)
{
    DECODE_FIELDS;
    int i;

    for (i = 0; i < 3; i++)
        NormColor(sf, lm, i);

    return(30);
}

static void NormColorColor(uint32_t v, uint32_t sf, int lm)
{
    int16_t tmp_vector[3];

    MultiplyMatrixByVector(&Matrices.Light, Vectors[v], CRVectors.Null, sf, lm);

    tmp_vector[0] = IR1; tmp_vector[1] = IR2; tmp_vector[2] = IR3;
    MultiplyMatrixByVector(&Matrices.Color, tmp_vector, CRVectors.B, sf, lm);

    MAC[1] = ((RGB.R << 4) * IR1) >> sf;
    MAC[2] = ((RGB.G << 4) * IR2) >> sf;
    MAC[3] = ((RGB.B << 4) * IR3) >> sf;

    MAC_to_IR(lm);

    MAC_to_RGB_FIFO();
}

static int32_t NCCS(uint32_t instr)
{
    DECODE_FIELDS;

    NormColorColor(0, sf, lm);
    return(17);
}


static int32_t NCCT(uint32_t instr)
{
    int i;
    DECODE_FIELDS;

    for (i = 0; i < 3; i++)
        NormColorColor(i, sf, lm);

    return(39);
}

static void DepthCue(int mult_IR123, int RGB_from_FIFO, uint32_t sf, int lm)
{
    int32_t RGB_temp[3];
    int32_t IR_temp[3] = { IR1, IR2, IR3 };
    int i;

    //assert(sf);

    if (RGB_from_FIFO)
    {
        RGB_temp[0] = RGB_FIFO[0].R << 4;
        RGB_temp[1] = RGB_FIFO[0].G << 4;
        RGB_temp[2] = RGB_FIFO[0].B << 4;
    }
    else
    {
        RGB_temp[0] = RGB.R << 4;
        RGB_temp[1] = RGB.G << 4;
        RGB_temp[2] = RGB.B << 4;
    }

    if (mult_IR123)
    {
        for (i = 0; i < 3; i++)
        {
            MAC[1 + i] = A_MV(i, ((int64_t)((uint64_t)(int64_t)CRVectors.FC[i] << 12) - RGB_temp[i] * IR_temp[i])) >> sf;
            MAC[1 + i] = A_MV(i, (RGB_temp[i] * IR_temp[i] + IR0 * Lm_B(i, MAC[1 + i], false))) >> sf;
        }
    }
    else
    {
        for (i = 0; i < 3; i++)
        {
            MAC[1 + i] = A_MV(i, ((int64_t)((uint64_t)(int64_t)CRVectors.FC[i] << 12) - (int32_t)((uint32_t)RGB_temp[i] << 12))) >> sf;
            MAC[1 + i] = A_MV(i, ((int64_t)((uint64_t)(int64_t)RGB_temp[i] << 12) + IR0 * Lm_B(i, MAC[1 + i], false))) >> sf;
        }
    }

    MAC_to_IR(lm);

    MAC_to_RGB_FIFO();
}


static int32_t DCPL(uint32_t instr)
{
    DECODE_FIELDS;

    DepthCue(true, false, sf, lm);

    return(8);
}


static int32_t DPCS(uint32_t instr)
{
    DECODE_FIELDS;

    DepthCue(false, false, sf, lm);

    return(8);
}

static int32_t DPCT(uint32_t instr)
{
    int i;
    DECODE_FIELDS;

    for (i = 0; i < 3; i++)
    {
        DepthCue(false, true, sf, lm);
    }

    return(17);
}

static int32_t INTPL(uint32_t instr)
{
    DECODE_FIELDS;

    MAC[1] = A_MV(0, ((int64_t)((uint64_t)(int64_t)CRVectors.FC[0] << 12) - (int32_t)((uint32_t)(int32_t)IR1 << 12))) >> sf;
    MAC[2] = A_MV(1, ((int64_t)((uint64_t)(int64_t)CRVectors.FC[1] << 12) - (int32_t)((uint32_t)(int32_t)IR2 << 12))) >> sf;
    MAC[3] = A_MV(2, ((int64_t)((uint64_t)(int64_t)CRVectors.FC[2] << 12) - (int32_t)((uint32_t)(int32_t)IR3 << 12))) >> sf;

    MAC[1] = A_MV(0, ((int64_t)((uint64_t)(int64_t)IR1 << 12) + IR0 * Lm_B(0, MAC[1], false)) >> sf);
    MAC[2] = A_MV(1, ((int64_t)((uint64_t)(int64_t)IR2 << 12) + IR0 * Lm_B(1, MAC[2], false)) >> sf);
    MAC[3] = A_MV(2, ((int64_t)((uint64_t)(int64_t)IR3 << 12) + IR0 * Lm_B(2, MAC[3], false)) >> sf);

    MAC_to_IR(lm);

    MAC_to_RGB_FIFO();

    return(8);
}


static void NormColorDepthCue(uint32_t v, uint32_t sf, int lm)
{
    int16_t tmp_vector[3];

    MultiplyMatrixByVector(&Matrices.Light, Vectors[v], CRVectors.Null, sf, lm);

    tmp_vector[0] = IR1; tmp_vector[1] = IR2; tmp_vector[2] = IR3;
    MultiplyMatrixByVector(&Matrices.Color, tmp_vector, CRVectors.B, sf, lm);

    DepthCue(true, false, sf, lm);
}

static int32_t NCDS(uint32_t instr)
{
    DECODE_FIELDS;

    NormColorDepthCue(0, sf, lm);

    return(19);
}

static int32_t NCDT(uint32_t instr)
{
    int i;
    DECODE_FIELDS;

    for (i = 0; i < 3; i++)
    {
        NormColorDepthCue(i, sf, lm);
    }

    return(44);
}

static int32_t CC(uint32_t instr)
{
    DECODE_FIELDS;
    int16_t tmp_vector[3];

    tmp_vector[0] = IR1; tmp_vector[1] = IR2; tmp_vector[2] = IR3;
    MultiplyMatrixByVector(&Matrices.Color, tmp_vector, CRVectors.B, sf, lm);

    MAC[1] = ((RGB.R << 4) * IR1) >> sf;
    MAC[2] = ((RGB.G << 4) * IR2) >> sf;
    MAC[3] = ((RGB.B << 4) * IR3) >> sf;

    MAC_to_IR(lm);

    MAC_to_RGB_FIFO();

    return(11);
}

static int32_t CDP(uint32_t instr)
{
    DECODE_FIELDS;
    int16_t tmp_vector[3];

    tmp_vector[0] = IR1; tmp_vector[1] = IR2; tmp_vector[2] = IR3;
    MultiplyMatrixByVector(&Matrices.Color, tmp_vector, CRVectors.B, sf, lm);

    DepthCue(true, false, sf, lm);

    return(13);
}

static int32_t NCLIP(uint32_t instr)
{
    DECODE_FIELDS;

    MAC[0] = F((int64_t)(XY_FIFO[0].X * (XY_FIFO[1].Y - XY_FIFO[2].Y)) + (XY_FIFO[1].X * (XY_FIFO[2].Y - XY_FIFO[0].Y)) + (XY_FIFO[2].X * (XY_FIFO[0].Y - XY_FIFO[1].Y))
    );

    return(8);
}

static int32_t AVSZ3(uint32_t instr)
{
    DECODE_FIELDS;

    MAC[0] = F(((int64_t)ZSF3 * (Z_FIFO[1] + Z_FIFO[2] + Z_FIFO[3])));

    OTZ = Lm_D(MAC[0] >> 12, false);

    return(5);
}

static int32_t AVSZ4(uint32_t instr)
{
    DECODE_FIELDS;

    MAC[0] = F(((int64_t)ZSF4 * (Z_FIFO[0] + Z_FIFO[1] + Z_FIFO[2] + Z_FIFO[3])));

    OTZ = Lm_D(MAC[0] >> 12, false);

    return(5);
}


// -32768 * -32768 - 32767 * -32768 = 2147450880
// (2 ^ 31) - 1 =		      2147483647
static int32_t OP(uint32_t instr)
{
    DECODE_FIELDS;

    MAC[1] = ((Matrices.Rot.MX[1][1] * IR3) - (Matrices.Rot.MX[2][2] * IR2)) >> sf;
    MAC[2] = ((Matrices.Rot.MX[2][2] * IR1) - (Matrices.Rot.MX[0][0] * IR3)) >> sf;
    MAC[3] = ((Matrices.Rot.MX[0][0] * IR2) - (Matrices.Rot.MX[1][1] * IR1)) >> sf;

    MAC_to_IR(lm);

    return(6);
}

static int32_t GPF(uint32_t instr)
{
    DECODE_FIELDS;

    MAC[1] = (IR0 * IR1) >> sf;
    MAC[2] = (IR0 * IR2) >> sf;
    MAC[3] = (IR0 * IR3) >> sf;

    MAC_to_IR(lm);

    MAC_to_RGB_FIFO();

    return(5);
}

static int32_t GPL(uint32_t instr)
{
    DECODE_FIELDS;

    MAC[1] = A_MV(0, (int64_t)((uint64_t)(int64_t)MAC[1] << sf) + (IR0 * IR1)) >> sf;
    MAC[2] = A_MV(1, (int64_t)((uint64_t)(int64_t)MAC[2] << sf) + (IR0 * IR2)) >> sf;
    MAC[3] = A_MV(2, (int64_t)((uint64_t)(int64_t)MAC[3] << sf) + (IR0 * IR3)) >> sf;

    MAC_to_IR(lm);

    MAC_to_RGB_FIFO();

    return(5);
}

int32_t gte::instruction(uint32_t instr)
{
    const unsigned code = instr & 0x3F;
    int32_t ret = 1;

    FLAGS = 0;

    switch (code)
    {
    default:
        logging::fatal("Unimplemented GTE command: " + helpers::intToHex(instr), logging::logSource::GTE);
        break;
    case 0x00:	// alternate?
    case 0x01:
        ret = RTPS(instr);
        break;
    case 0x06:
        ret = NCLIP(instr);
        break;
    case 0x0C:
        ret = OP(instr);
        break;
    case 0x10:
        ret = DPCS(instr);
        break;
    case 0x11:
        ret = INTPL(instr);
        break;
    case 0x12:
        ret = MVMVA(instr);
        break;
    case 0x13:
        ret = NCDS(instr);
        break;
    case 0x14:
        ret = CDP(instr);
        break;
    case 0x16:
        ret = NCDT(instr);
        break;
    case 0x1B:
        ret = NCCS(instr);
        break;
    case 0x1C:
        ret = CC(instr);
        break;
    case 0x1E:
        ret = NCS(instr);
        break;
    case 0x20:
        ret = NCT(instr);
        break;
    case 0x28:
        ret = SQR(instr);
        break;
    case 0x1A:	// Alternate for 0x29?
    case 0x29:
        ret = DCPL(instr);
        break;
    case 0x2A:
        ret = DPCT(instr);
        break;
    case 0x2D:
        ret = AVSZ3(instr);
        break;
    case 0x2E:
        ret = AVSZ4(instr);
        break;
    case 0x30:
        ret = RTPT(instr);
        break;
    case 0x3D:
        ret = GPF(instr);
        break;
    case 0x3E:
        ret = GPL(instr);
        break;
    case 0x3F:
        ret = NCCT(instr);
        break;
    }

    if (FLAGS & 0x7f87e000)
        FLAGS |= 1 << 31;

    CR[31] = FLAGS;

    return(ret - 1);
}

#undef IR0
#undef IR1
#undef IR2
#undef IR3
#undef sign_x_to_s64
#undef DECODE_FIELDS