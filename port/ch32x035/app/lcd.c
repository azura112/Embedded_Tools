/********************************** (C) COPYRIGHT *******************************
 * File Name          : lcd.c
 * Description        : ST7735S 160x80 SPI TFT driver (landscape) for the
 *                      CH32X035G8U6 PD test board.
 *
 *                      Geometry: ST7735S GRAM is 132 cols x 162 rows. On
 *                      160x80 modules the visible area covers GRAM columns
 *                      2..81 (80-pixel axis) and rows 1..160 (160-pixel
 *                      axis). Landscape selects MADCTL MV=1, which exchanges
 *                      the axes: screen X -> GRAM rows, screen Y -> GRAM
 *                      columns. Data streamed after 0x2C always fills the
 *                      window row-major on screen (X fastest), for any
 *                      MADCTL orientation.
 *******************************************************************************/
#include "lcd.h"
#include "ch32x035_spi.h"

/* ------------------------- panel calibration (tune here) ------------------ */
/* Calibrated on-board 2026-09-24 for the current 160x80 ST7735S FFC module:
 *   rotation state 5 (MADCTL 0x60|BGR = 0x68), CASET offset 1, RASET offset 26
 *   -> the 80-px axis is GRAM rows centered in 132 (visible 26..105).
 * (State 6 is the same image rotated 180 deg - both read fine.)
 * Window mapping is FIXED (x -> CASET, y -> RASET); orientation is MADCTL
 * only. Swapping the window together with the MV bit cancels out - do NOT
 * re-couple them.
 * If you swap in a different module, re-tune live over the USB CDC port:
 *   o = cycle 8 orientations, c = toggle BGR, , . < > = nudge CASET/RASET
 * offsets by 1 px. No reflash needed.                                     */
#define LCD_OFF_160AXIS   1           /* default CASET offset (calibrated)     */
#define LCD_OFF_80AXIS    26          /* default RASET offset (calibrated)     */
#define LCD_INVERT        1           /* 1 = IPS panel (INVON), 0 = TN         */
#define LCD_MADCTL_BGR    0x08

/* Rotation states 0..7 = 8 DISTINCT orientations: fixed window mapping
 * (x -> CASET cols, y -> RASET rows) crossed with the 8 MADCTL combos
 * (MV=0: 0x00/0x40/0x80/0xC0, MV=1: 0x20/0x60/0xA0/0xE0). */
static const u8 lcd_rot_mad[8] = { 0x00, 0x40, 0x80, 0xC0, 0x20, 0x60, 0xA0, 0xE0 };
static u8 lcd_rot = 5;                 /* calibrated default (see block above) */
static u8 lcd_bgr = 1;

/* runtime window offsets (tuned live with the , . < > commands) */
static u8 lcd_off_c = LCD_OFF_160AXIS;   /* CASET offset */
static u8 lcd_off_r = LCD_OFF_80AXIS;    /* RASET offset */

void LCD_SetOffsets(u8 cas_off, u8 ras_off)
{
    lcd_off_c = cas_off;
    lcd_off_r = ras_off;
}

void LCD_GetOffsets(u8 *cas_off, u8 *ras_off)
{
    *cas_off = lcd_off_c;
    *ras_off = lcd_off_r;
}

static u8 lcd_madctl(void)
{
    return (u8)(lcd_rot_mad[lcd_rot & 7] | (lcd_bgr ? LCD_MADCTL_BGR : 0));
}

/* SPI1 clock divider from APB2 48MHz. 4 -> 12MHz (within ST7735S 15MHz spec).
 * If the panel does not respond at all on a long FFC, try 8 (6MHz). */
#define LCD_SPI_PRESCALER   SPI_BaudRatePrescaler_4

/* --------------------------------- pins ---------------------------------- */
#define LCD_CS_LOW()        GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define LCD_CS_HIGH()       GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define LCD_RS_CMD()        GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define LCD_RS_DATA()       GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define LCD_RST_LOW()       GPIO_ResetBits(GPIOB, GPIO_Pin_3)
#define LCD_RST_HIGH()      GPIO_SetBits(GPIOB, GPIO_Pin_3)

/* ------------------------- init / gamma parameter tables ------------------ */

static const u8 LCD_GammaP[16] = {
    0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
    0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10
};

static const u8 LCD_GammaN[16] = {
    0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
    0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10
};

/* Classic 5x7 GLCD font, printable ASCII 0x20..0x7E, bit0 = top row. */
static const u8 LCD_Font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x00,0x08,0x14,0x22,0x41}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x41,0x22,0x14,0x08,0x00}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x01,0x01}, /* F */
    {0x3E,0x41,0x41,0x51,0x32}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x26,0x49,0x49,0x49,0x32}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x41,0x41,0x7F,0x00,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x08,0x14,0x54,0x54,0x3C}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* ~ */
};

/* -------------------------------- low level ------------------------------- */

static void LCD_SPI_Tx(u8 d)
{
    while(!SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE));
    SPI_I2S_SendData(SPI1, d);
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY));
    SPI_I2S_ReceiveData(SPI1);          /* discard echo, keeps OVR clear */
}

static void LCD_WrCmd(u8 c)
{
    LCD_RS_CMD();
    LCD_SPI_Tx(c);
}

static void LCD_WrDat(u8 d)
{
    LCD_RS_DATA();
    LCD_SPI_Tx(d);
}

static void LCD_Cmd(u8 cmd)
{
    LCD_CS_LOW();
    LCD_WrCmd(cmd);
    LCD_CS_HIGH();
}

static void LCD_CmdData(u8 cmd, const u8 *dat, u8 len)
{
    LCD_CS_LOW();
    LCD_WrCmd(cmd);
    while(len--)
    {
        LCD_WrDat(*dat++);
    }
    LCD_CS_HIGH();
}

static void LCD_CmdByte(u8 cmd, u8 dat)
{
    LCD_CmdData(cmd, &dat, 1);
}

static void LCD_LowLevel_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef  SPI_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_SPI1, ENABLE);

    /* PA4 = CS (software), PA5 = SCK, PA7 = MOSI; PA6/MISO not wired on FFC */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PB0 = RS, PB3 = RESET. TIM2 PartialRemap2 routes CH3 AF to PB3 as
     * well, but OC3 is never enabled, so PB3 stays a plain output. */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    LCD_CS_HIGH();
    LCD_RST_HIGH();

    /* SPI1 master, mode 0, 8-bit, 48MHz/4 = 12MHz (within 15MHz spec) */
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);
    SPI_Cmd(SPI1, ENABLE);
}

/* ---------------------------------- init ---------------------------------- */

void LCD_Init(void)
{
    LCD_LowLevel_Init();

    /* hardware reset pulse */
    LCD_RST_LOW();
    Delay_Ms(20);
    LCD_RST_HIGH();
    Delay_Ms(120);

    LCD_Cmd(0x01);                      /* SWRESET */
    Delay_Ms(120);
    LCD_Cmd(0x11);                      /* SLPOUT */
    Delay_Ms(120);

    LCD_CmdData(0xB1, (const u8[]){0x01, 0x2C, 0x2D}, 3);          /* FRMCTR1 */
    LCD_CmdData(0xB2, (const u8[]){0x01, 0x2C, 0x2D}, 3);          /* FRMCTR2 */
    LCD_CmdData(0xB3, (const u8[]){0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6); /* FRMCTR3 */
    LCD_CmdByte(0xB4, 0x07);            /* INVCTR: line inversion */
    LCD_CmdData(0xC0, (const u8[]){0xA2, 0x02, 0x84}, 3);          /* PWCTR1 */
    LCD_CmdByte(0xC1, 0xC5);            /* PWCTR2 */
    LCD_CmdData(0xC2, (const u8[]){0x0A, 0x00}, 2);                /* PWCTR3 */
    LCD_CmdData(0xC3, (const u8[]){0x8A, 0x2A}, 2);                /* PWCTR4 */
    LCD_CmdData(0xC4, (const u8[]){0x8A, 0xEE}, 2);                /* PWCTR5 */
    LCD_CmdByte(0xC5, 0x0E);            /* VMCTR1: VCOMH */
#if LCD_INVERT
    LCD_Cmd(0x21);                      /* INVON: IPS panel */
#endif
    LCD_CmdByte(0x36, lcd_madctl());      /* MADCTL: landscape, BGR */
    LCD_CmdByte(0x3A, 0x05);            /* COLMOD: 16 bit/pixel */
    /* ST7735S quirk: pixel-order adjust before gamma */
    LCD_CmdData(0x2A, (const u8[]){0x00, 0x00, 0x00, 0x7F}, 4);
    LCD_CmdData(0x2B, (const u8[]){0x00, 0x00, 0x00, 0x9F}, 4);
    LCD_CmdData(0xE0, LCD_GammaP, 16);  /* GMCTRP1 */
    LCD_CmdData(0xE1, LCD_GammaN, 16);  /* GMCTRN1 */
    LCD_Cmd(0x13);                      /* NORON */
    Delay_Ms(10);
    LCD_Cmd(0x29);                      /* DISPON */
    Delay_Ms(50);

    LCD_Fill(LCD_COLOR_BLACK);
}

/* ------------------------------- window / fill ---------------------------- */

void LCD_SetRotation(u8 rot)
{
    lcd_rot = (u8)(rot & 7);
    LCD_CmdByte(0x36, lcd_madctl());
}

u8 LCD_GetRotation(void)
{
    return lcd_rot;
}

void LCD_SetBgr(u8 on)
{
    lcd_bgr = on ? 1 : 0;
    LCD_CmdByte(0x36, lcd_madctl());
}

u8 LCD_GetBgr(void)
{
    return lcd_bgr;
}

void LCD_SetWindow(u16 x0, u16 y0, u16 x1, u16 y1)
{
    u8 cas[4], ras[4];

    if(x0 >= LCD_W) x0 = LCD_W - 1;
    if(x1 >= LCD_W) x1 = LCD_W - 1;
    if(y0 >= LCD_H) y0 = LCD_H - 1;
    if(y1 >= LCD_H) y1 = LCD_H - 1;
    if(x1 < x0) { u16 t = x0; x0 = x1; x1 = t; }
    if(y1 < y0) { u16 t = y0; y0 = y1; y1 = t; }

    /* fixed window mapping; orientation comes from MADCTL alone */
    cas[0] = 0;
    cas[1] = (u8)(lcd_off_c + x0);
    cas[2] = 0;
    cas[3] = (u8)(lcd_off_c + x1);
    ras[0] = 0;
    ras[1] = (u8)(lcd_off_r + y0);
    ras[2] = 0;
    ras[3] = (u8)(lcd_off_r + y1);

    LCD_CmdData(0x2A, cas, 4);
    LCD_CmdData(0x2B, ras, 4);
    LCD_Cmd(0x2C);
}

static void LCD_TxBurst(u16 color, u32 n)
{
    u8 hi = (u8)(color >> 8);
    u8 lo = (u8)color;

    LCD_CS_LOW();
    LCD_RS_DATA();
    while(n--)
    {
        LCD_SPI_Tx(hi);
        LCD_SPI_Tx(lo);
    }
    LCD_CS_HIGH();
}

void LCD_FillRect(u16 x0, u16 y0, u16 x1, u16 y1, u16 color)
{
    if((x0 >= LCD_W) || (y0 >= LCD_H)) return;
    if(x1 >= LCD_W) x1 = LCD_W - 1;
    if(y1 >= LCD_H) y1 = LCD_H - 1;

    LCD_SetWindow(x0, y0, x1, y1);
    LCD_TxBurst(color, (u32)(x1 - x0 + 1) * (y1 - y0 + 1));
}

void LCD_Fill(u16 color)
{
    LCD_FillRect(0, 0, LCD_W - 1, LCD_H - 1, color);
}

/* ------------------------------ primitives -------------------------------- */

void LCD_DrawPixel(u16 x, u16 y, u16 color)
{
    if((x >= LCD_W) || (y >= LCD_H)) return;

    LCD_SetWindow(x, y, x, y);
    LCD_TxBurst(color, 1);
}

void LCD_DrawHLine(u16 x0, u16 x1, u16 y, u16 color)
{
    LCD_FillRect(x0, y, x1, y, color);
}

void LCD_DrawVLine(u16 x, u16 y0, u16 y1, u16 color)
{
    LCD_FillRect(x, y0, x, y1, color);
}

void LCD_DrawRect(u16 x0, u16 y0, u16 x1, u16 y1, u16 color)
{
    LCD_DrawHLine(x0, x1, y0, color);
    LCD_DrawHLine(x0, x1, y1, color);
    LCD_DrawVLine(x0, y0, y1, color);
    LCD_DrawVLine(x1, y0, y1, color);
}

void LCD_DrawLine(u16 x0, u16 y0, u16 x1, u16 y1, u16 color)
{
    int dx = (int)x1 - (int)x0;
    int dy = (int)y1 - (int)y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    int err, e2;

    if(dx < 0) dx = -dx;
    if(dy < 0) dy = -dy;
    err = dx - dy;

    for(;;)
    {
        LCD_DrawPixel(x0, y0, color);
        if((x0 == x1) && (y0 == y1)) break;
        e2 = 2 * err;
        if(e2 > -dy) { err -= dy; x0 += (u16)sx; }
        if(e2 <  dx) { err += dx; y0 += (u16)sy; }
    }
}

void LCD_DrawCircle(u16 xm, u16 ym, u16 r, u16 color)
{
    int x = r;
    int y = 0;
    int err = 0;

    while(x >= y)
    {
        LCD_DrawPixel(xm + x, ym + y, color);
        LCD_DrawPixel(xm + y, ym + x, color);
        LCD_DrawPixel(xm - y, ym + x, color);
        LCD_DrawPixel(xm - x, ym + y, color);
        LCD_DrawPixel(xm - x, ym - y, color);
        LCD_DrawPixel(xm - y, ym - x, color);
        LCD_DrawPixel(xm + y, ym - x, color);
        LCD_DrawPixel(xm + x, ym - y, color);

        if(err <= 0) { y += 1; err += 2 * y + 1; }
        if(err >  0) { x -= 1; err -= 2 * x + 1; }
    }
}

/* ---------------------------------- text ---------------------------------- */

u16 LCD_DrawChar(u16 x, u16 y, char ch, u16 fg, u16 bg, u8 scale)
{
    const u8 *glyph;
    u8 col, row, rend;
    u16 nx = x + 6 * (u16)scale;

    if((u8)ch < 0x20 || (u8)ch > 0x7E) ch = '?';
    glyph = LCD_Font5x7[(u8)ch - 0x20];

    if((x >= LCD_W) || (y >= LCD_H)) return nx;

    if(scale == 1)
    {
        /* fast path: one column strip per font column (X fixed, Y spans) */
        rend = 6;
        if(y + 7 > LCD_H) rend = (u8)(LCD_H - 1 - y);

        for(col = 0; col < 5; col++)
        {
            if((u16)(x + col) >= LCD_W) break;

            LCD_SetWindow(x + col, y, x + col, y + rend);
            LCD_CS_LOW();
            LCD_RS_DATA();
            for(row = 0; row <= rend; row++)
            {
                u16 p = ((glyph[col] >> row) & 1) ? fg : bg;
                LCD_SPI_Tx((u8)(p >> 8));
                LCD_SPI_Tx((u8)p);
            }
            LCD_CS_HIGH();
        }

        if((u16)(x + 5) < LCD_W)
        {
            LCD_SetWindow(x + 5, y, x + 5, y + rend);
            LCD_TxBurst(bg, rend + 1);
        }
    }
    else
    {
        for(col = 0; col < 5; col++)
        {
            if((u16)(x + col * scale) >= LCD_W) break;
            for(row = 0; row < 7; row++)
            {
                if((u16)(y + row * scale) >= LCD_H) break;

                LCD_FillRect(x + col * scale, y + row * scale,
                             x + col * scale + scale - 1,
                             y + row * scale + scale - 1,
                             ((glyph[col] >> row) & 1) ? fg : bg);
            }
        }

        LCD_FillRect(x + 5 * scale, y, x + 6 * scale - 1, y + 7 * scale - 1, bg);
    }

    return nx;
}

u16 LCD_DrawString(u16 x, u16 y, const char *s, u16 fg, u16 bg, u8 scale)
{
    while(*s)
    {
        x = LCD_DrawChar(x, y, *s++, fg, bg, scale);
    }
    return x;
}

u16 LCD_DrawUInt(u16 x, u16 y, u32 val, u16 fg, u16 bg, u8 scale)
{
    char buf[11];
    u8 i = 10;

    buf[10] = '\0';
    do
    {
        buf[--i] = '0' + (char)(val % 10);
        val /= 10;
    } while(val && i);

    return LCD_DrawString(x, y, &buf[i], fg, bg, scale);
}
