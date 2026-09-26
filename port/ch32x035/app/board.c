/********************************** (C) COPYRIGHT *******************************
 * File Name          : board.c
 * Description        : Board level drivers for CH32X035G8U6 PD test board.
 *                      (et 重构版: 与 V1.7 的差异仅在按键层 —— 去掉自带消抖,
 *                       提供单次电平采样 Board_Key_Read_Raw 供 et_key 调用;
 *                       LED/背光/VBUS 逐行同源 V1.7)
 *******************************************************************************/
#include "board.h"
#include "ch32x035_opa.h"

/* --------------------------------- LED ------------------------------------ */

void Board_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_SetBits(GPIOB, GPIO_Pin_12);           /* LED off (active low) */
}

void Board_LED_Set(uint8_t on)
{
    if(on)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_12);     /* cathode low -> LED on */
    }
    else
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_12);
    }
}

void Board_LED_Toggle(void)
{
    GPIO_WriteBit(GPIOB, GPIO_Pin_12,
                  (BitAction)!GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_12));
}

/* --------------------------------- Keys ----------------------------------- */
/* PB1/PB6/PB9 float when idle (no external pull-down, PB has no internal
 * pull-down). Sampling trick: drive the pin LOW for a moment, switch back to
 * input floating and read immediately. If the key contact closes the pin to
 * the common (A, pulled up by R10 4.7k to 3.3V) the pin reads HIGH.
 * 注意: 每次采样都会短暂把引脚切为推挽输出再切回浮空输入, 因此采样
 * 只能在主循环上下文调用(et_key_scan 的调用方), 不可在 ISR 里做。   */

static GPIO_TypeDef *KEY_PORT[BOARD_KEYn] = { GPIOB, GPIOB, GPIOB };
static const uint16_t KEY_PIN[BOARD_KEYn] = { GPIO_Pin_1, GPIO_Pin_6, GPIO_Pin_9 };

void Board_Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_6 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   /* idle weak high */
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

bool Board_Key_Read_Raw(uint8_t idx)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    uint8_t raw;

    if(idx >= BOARD_KEYn)
    {
        return false;
    }

    /* 1. pull the pin low to discharge stray capacitance */
    GPIO_InitStructure.GPIO_Pin = KEY_PIN[idx];
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEY_PORT[idx], &GPIO_InitStructure);
    GPIO_ResetBits(KEY_PORT[idx], KEY_PIN[idx]);

    /* 2. back to floating input and read at once */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(KEY_PORT[idx], &GPIO_InitStructure);
    raw = GPIO_ReadInputDataBit(KEY_PORT[idx], KEY_PIN[idx]);

    return (raw != 0) ? true : false;
}

/* ------------------------------- Backlight -------------------------------- */
/* NOTE: on this silicon sample the datasheet route TIM2_RM=010/011 -> CH4/PB4
 * does NOT actually output (verified on board: TIM2 path dark, TIM3 path and
 * plain GPIO light). Backlight PWM therefore uses TIM3_CH1 at TIM3_RM=01
 * (RM mapping table: "01: CH1/PB4, CH2/PB5"; PB5 unused, OC2 never enabled). */

void Board_Backlight_Init(uint8_t duty_percent)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* TIM3_RM = 01: CH1/PB4, CH2/PB5 (only CH1 is used) */
    GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM3, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 48MHz / 48 = 1MHz, /1000 = 1kHz PWM */
    TIM_TimeBaseInitStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 48 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0x00;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);

    Board_Backlight_Set(duty_percent);
}

void Board_Backlight_Set(uint8_t duty_percent)
{
    u16 pulse;

    if(duty_percent > 100) duty_percent = 100;
    pulse = (u16)((uint32_t)duty_percent * 1000 / 100);
    TIM_SetCompare1(TIM3, pulse);
}

/* ------------------------------- VBUS sense ------------------------------- */

/* PB8 (O1P1) has no ADC channel, so VBUS/16 is routed to PA3 (ADC ch3) via
 * OPA1. Three selectable configurations:
 *   0 = PGA x4: NSEL=CHN_PGA_4xIN, FB=ON (internal 64K to GND, gain 1+192/64=4)
 *       -> range ~13.2V, 15/20V PDOs saturate (Board_VBUS_OverRange flags it)
 *   1 = SOFTWARE FOLLOWER (no hardware change): NSEL=CHN0 (PA6 pin, left
 *       floating/high-Z) + FB=ON -> internal 192K closes the loop, Rg=inf,
 *       gain = 1 + 192K/inf = 1 -> range ~52.8V (RM 17.2.1 / OPA_CTLR1 notes)
 *   2 = FLYWIRE follower: PA6(U1.11)<->PA3(U1.8) soldered, loop closed
 *       externally, FB=OFF -> same ~52.8V range (kept as a fallback)
 * Mode 1 accuracy caveat: PA6 input bias current x 192K adds a small output
 * offset; negligible for a test board, calibrate if it matters. */
#define VBUS_SENSE_MODE     1

#define VBUS_ADC_CH          ADC_Channel_3    /* PA3 = OPA1 OUT0 */
#define VBUS_FULLSCALE_MV    3284UL           /* ADC reference = VDD 3.3V */
#if VBUS_SENSE_MODE == 0
#define VBUS_GAIN_NUM        4                /* PGA x4: divider ratio / gain */
#else
#define VBUS_GAIN_NUM        16               /* follower: divider ratio only */
#endif
#define VBUS_DIV_DEN         4096             /* 12-bit ADC */
#define VBUS_OVER_RAW        4060             /* near full-scale = saturated */

static uint8_t vbus_over = 0;

void Board_VBUS_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    OPA_InitTypeDef  OPA_InitStructure = {0};
    ADC_InitTypeDef  ADC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_ADC1, ENABLE);

    /* PB8 = OPA1 positive input option 1 (O1P1), analog function */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PA3 = OPA1 output option 0 (O1O0), sampled by ADC_Channel_3;
     * PA6 (O1N0) = inverting input node, must be high-Z analog in
     * follower modes (floating for mode 1, flywired to PA3 for mode 2) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
#if VBUS_SENSE_MODE != 0
    GPIO_InitStructure.GPIO_Pin |= GPIO_Pin_6;
#endif
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* OPA1 buffers the VBUS/16 divider node:
     * mode 0: internal PGA x4 feedback, range ~13.2V
     * mode 1: software follower (floating PA6 + internal 192K), range ~52.8V
     * mode 2: flywire follower (PA6<->PA3), range ~52.8V                   */
    OPA_Unlock();
    OPA_InitStructure.OPA_NUM = OPA1;
    OPA_InitStructure.PSEL = CHP1;              /* PB8 (O1P1) */
#if VBUS_SENSE_MODE == 0
    OPA_InitStructure.NSEL = CHN_PGA_4xIN;      /* internal 64K, gain 4 */
    OPA_InitStructure.FB = FB_ON;
#elif VBUS_SENSE_MODE == 1
    OPA_InitStructure.NSEL = CHN0;              /* PA6 pin, left floating */
    OPA_InitStructure.FB = FB_ON;               /* internal 192K -> unity */
#else
    OPA_InitStructure.NSEL = CHN0;              /* PA6 (O1N0) */
    OPA_InitStructure.FB = FB_OFF;              /* loop closed by flywire */
#endif
    OPA_InitStructure.Mode = OUT_IO_OUT0;       /* route output to PA3 */
    OPA_Init(&OPA_InitStructure);
    OPA_Cmd(OPA1, ENABLE);

    /* ADC1 single conversion mode */
    ADC_DeInit(ADC1);
    ADC_CLKConfig(ADC1, ADC_CLK_Div6);          /* 48/6 = 8MHz ADC clock */

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
}

/* ---------------- VBUS reading: plain single conversion ------------------- */
/* Simplest scheme: assume VDDA = 3.3V exactly, one conversion per read, no
 * filtering. (The V1.5 VREFINT-tracking + trimmed-mean + EMA experiment is
 * archived in archive/board_v1.5_vrefint_filtered.c.bak if ever needed.) */

static u16 VBUS_Get_Raw(void)
{
    u16 val;

    ADC_RegularChannelConfig(ADC1, VBUS_ADC_CH, 1, ADC_SampleTime_11Cycles);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    val = ADC_GetConversionValue(ADC1);

    return val;
}

uint16_t Board_VBUS_Read_mV(void)
{
    u32 sum = 0;
    for(int i=0;i<100;++i) {
        sum+=VBUS_Get_Raw();
    }
    sum/=100;
    uint32_t mv;

    vbus_over = (sum >= VBUS_OVER_RAW) ? 1 : 0;

    /* VBUS[mV] = raw * 3300 / 4096 * VBUS_GAIN_NUM */
    mv = (sum * VBUS_FULLSCALE_MV * VBUS_GAIN_NUM) / VBUS_DIV_DEN;
    if(mv > 0xFFFF) mv = 0xFFFF;

    return (uint16_t)mv;
}


uint8_t Board_VBUS_OverRange(void)
{
    return vbus_over;
}
