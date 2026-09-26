/********************************** (C) COPYRIGHT *******************************
 * File Name          : board.h
 * Description        : Board level drivers for CH32X035G8U6 PD test board.
 *                      (et 重构版: 按键只提供电平采样, 消抖/事件归 et_key;
 *                       LED 为 et_led 的输出回调; VBUS/背光与 V1.7 相同)
 *
 *                      Schematic mapping (see board-schematic-notes.md):
 *                        LED2    - PB12  (cathode on PB12, active LOW)
 *                        KEY0    - PB1   (USER_KEY0, active HIGH, no ext. pull-down)
 *                        KEY1    - PB6   (USER_KEY1, active HIGH, no ext. pull-down)
 *                        KEY2    - PB9   (USER_KEY2, active HIGH, no ext. pull-down)
 *                        LCD_BLK - PB4   (TIM3_CH1 via PartialRemap1_TIM3, backlight PWM;
 *                                         TIM2_RM CH4->PB4 documented but dead on this part)
 *                        ADC_VOL - PB8 (O1P1) -> OPA1(PGA x4) -> PA3 (A3/ADC_CH3)
 *                                  VBUS/16 at PB8, Vout = VBUS/4, range <= ~13.2V
 *******************************************************************************/
#ifndef USER_BOARD_H
#define USER_BOARD_H

#include "debug.h"
#include <stdint.h>
#include <stdbool.h>

/* --------------------------------- LED ------------------------------------ */
/* LED2 cathode is driven by PB12: LOW = ON.
 * et_led 接线: write 回调把亮度(0~255)阈值化为开关电平。 */
void Board_LED_Init(void);
void Board_LED_Set(uint8_t on);
void Board_LED_Toggle(void);

/* --------------------------------- Keys ----------------------------------- */
/* PB pins do not support internal pull-down, keys are sampled with a
 * software pull-down: drive pin LOW, release, then read (HIGH = pressed).
 * 消抖/短按长按事件由 et_key 状态机负责(本层只做一次电平采样)。 */
#define BOARD_KEYn      3
#define BOARD_KEY0      0           /* PB1, USER_KEY0 */
#define BOARD_KEY1      1           /* PB6, USER_KEY1 */
#define BOARD_KEY2      2           /* PB9, USER_KEY2 */

void Board_Key_Init(void);
bool Board_Key_Read_Raw(uint8_t idx);   /* 一次采样: 1 = 按下(高有效) */

/* ------------------------------- Backlight -------------------------------- */
/* LCD_BLK on PB4, TIM3_CH1 @1kHz, duty in percent 0..100 */
void Board_Backlight_Init(uint8_t duty_percent);
void Board_Backlight_Set(uint8_t duty_percent);

/* ------------------------------- VBUS sense ------------------------------- */
/* OPA1: PSEL=CHP1 (PB8/O1P1 = ADC_VOL = VBUS/16), output routed to PA3
 * (O1O0) which is sampled on ADC_Channel_3. Software follower mode:
 * VBUS[mV] = raw * 3300 / 4096 * 16, range ~52V.                        */
void Board_VBUS_Init(void);
uint16_t Board_VBUS_Read_mV(void);      /* single conversion, VDDA=3.3V assumed */
uint8_t Board_VBUS_OverRange(void);     /* 1 if last reading was saturated   */

#endif
