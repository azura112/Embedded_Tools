/********************************** (C) COPYRIGHT *******************************
 * File Name          : lcd.h
 * Description        : ST7735S 160x80 SPI TFT driver (landscape) for the
 *                      CH32X035G8U6 PD test board.
 *
 *                      Schematic mapping (see board-schematic-notes.md):
 *                        LCD_CS    - PA4  (software chip select, idle high)
 *                        SPI1_SCK  - PA5  (SPI1, 48MHz/4 = 12MHz, mode 0)
 *                        SPI1_MOSI - PA7  (SPI1, write-only, FFC has no MISO)
 *                        LCD_RS    - PB0  (0 = command, 1 = data)
 *                        LCD_RESET - PB3  (hardware reset, active low)
 *                        LCD_BLK   - PB4  (backlight PWM via Board_Backlight_*)
 *                      Panel: ST7735S, 160x80, wired native-landscape
 *                      (MV=0: CASET = X, RASET = Y - see lcd.c).
 *******************************************************************************/
#ifndef USER_LCD_H
#define USER_LCD_H

#include "debug.h"

#define LCD_W               160
#define LCD_H               80

/* RGB565 colors (logical; panel BGR order handled by MADCTL bit) */
#define LCD_RGB565(r, g, b) ((u16)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define LCD_COLOR_BLACK     0x0000
#define LCD_COLOR_NAVY      0x000F
#define LCD_COLOR_DGREEN    0x03E0
#define LCD_COLOR_DCYAN     0x03EF
#define LCD_COLOR_MAROON    0x7800
#define LCD_COLOR_PURPLE    0x780F
#define LCD_COLOR_OLIVE     0x7BE0
#define LCD_COLOR_LGRAY     0xC618
#define LCD_COLOR_DGRAY     0x7BEF
#define LCD_COLOR_BLUE      0x001F
#define LCD_COLOR_GREEN     0x07E0
#define LCD_COLOR_CYAN      0x07FF
#define LCD_COLOR_RED       0xF800
#define LCD_COLOR_MAGENTA   0xF81F
#define LCD_COLOR_YELLOW    0xFFE0
#define LCD_COLOR_WHITE     0xFFFF
#define LCD_COLOR_ORANGE    0xFD20
#define LCD_COLOR_GRAY      0x8410

void LCD_Init(void);
void LCD_SetWindow(u16 x0, u16 y0, u16 x1, u16 y1);

/* Runtime orientation tuning (see lcd.c calibration block):
 * rotation states 0..7 = 4 MV=0 (native-landscape glass) + 4 MV=1
 * (portrait glass) variants; the window mapping follows the state.
 * Cycle with the 'o' command over USB CDC, toggle BGR with 'c'. */
void LCD_SetRotation(u8 rot);
u8   LCD_GetRotation(void);
void LCD_SetBgr(u8 on);
u8   LCD_GetBgr(void);
void LCD_SetOffsets(u8 cas_off, u8 ras_off);
void LCD_GetOffsets(u8 *cas_off, u8 *ras_off);

void LCD_Fill(u16 color);
void LCD_FillRect(u16 x0, u16 y0, u16 x1, u16 y1, u16 color);
void LCD_DrawPixel(u16 x, u16 y, u16 color);
void LCD_DrawHLine(u16 x0, u16 x1, u16 y, u16 color);
void LCD_DrawVLine(u16 x, u16 y0, u16 y1, u16 color);
void LCD_DrawLine(u16 x0, u16 y0, u16 x1, u16 y1, u16 color);
void LCD_DrawRect(u16 x0, u16 y0, u16 x1, u16 y1, u16 color);
void LCD_DrawCircle(u16 xm, u16 ym, u16 r, u16 color);

/* Text: fixed 5x7 font, scale 1..n. Coordinates are top-left. Opaque
 * background. Each function returns the x just past the drawn text, so
 * calls can be chained. */
u16  LCD_DrawChar(u16 x, u16 y, char ch, u16 fg, u16 bg, u8 scale);
u16  LCD_DrawString(u16 x, u16 y, const char *s, u16 fg, u16 bg, u8 scale);
u16  LCD_DrawUInt(u16 x, u16 y, u32 val, u16 fg, u16 bg, u8 scale);

#endif
