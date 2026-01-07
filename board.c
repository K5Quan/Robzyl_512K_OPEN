/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Modified work Copyright 2024 kamilsss655
 * https://github.com/kamilsss655
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>

#include "app/fm.h"

#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "bsp/dp32g030/saradc.h"
#include "bsp/dp32g030/syscon.h"
#include "driver/adc.h"
#include "driver/backlight.h"
#include "driver/bk1080.h"
#include "driver/bk4819.h"
#include "driver/crc.h"
#include "driver/eeprom.h"
#include "driver/flash.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/st7565.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#if defined(ENABLE_OVERLAY)
	#include "sram-overlay.h"
#endif
#include "ui/menu.h"
#include "ARMCM0.h"
#include "app/spectrum.h"

//#include "debugging.h"

#if defined(ENABLE_OVERLAY)
	void BOARD_FLASH_Init(void)
	{
		FLASH_Init(FLASH_READ_MODE_1_CYCLE);
		FLASH_ConfigureTrimValues();
		SYSTEM_ConfigureClocks();

		overlay_FLASH_MainClock       = 48000000;
		overlay_FLASH_ClockMultiplier = 48;

		FLASH_Init(FLASH_READ_MODE_2_CYCLE);
	}
#endif

void BOARD_GPIO_Init(void)
{
	GPIOA->DIR |= 0
		// A7 = UART1 TX default as OUTPUT from bootloader!
		// A8 = UART1 RX default as INPUT from bootloader!
		// Key pad + I2C
		| GPIO_DIR_10_BITS_OUTPUT
		// Key pad + I2C
		| GPIO_DIR_11_BITS_OUTPUT
		// Key pad + Voice chip
		| GPIO_DIR_12_BITS_OUTPUT
		// Key pad + Voice chip
		| GPIO_DIR_13_BITS_OUTPUT
		;
	GPIOA->DIR &= ~(0
		// Key pad
		| GPIO_DIR_3_MASK // INPUT
		// Key pad
		| GPIO_DIR_4_MASK // INPUT
		// Key pad
		| GPIO_DIR_5_MASK // INPUT
		// Key pad
		| GPIO_DIR_6_MASK // INPUT
		);
	GPIOB->DIR |= 0
		// ST7565
		| GPIO_DIR_9_BITS_OUTPUT
		// ST7565 + SWD IO
		| GPIO_DIR_11_BITS_OUTPUT
		// B14 = SWD_CLK assumed INPUT by default
		// BK1080
		| GPIO_DIR_15_BITS_OUTPUT
		;
	GPIOC->DIR |= 0
		// BK4819 SCN
		| GPIO_DIR_0_BITS_OUTPUT
		// BK4819 SCL
		| GPIO_DIR_1_BITS_OUTPUT
		// BK4819 SDA
		| GPIO_DIR_2_BITS_OUTPUT
		// Flash light
		| GPIO_DIR_3_BITS_OUTPUT
		// Speaker
		| GPIO_DIR_4_BITS_OUTPUT
		;
	GPIOC->DIR &= ~(0
		// PTT button
		| GPIO_DIR_5_MASK // INPUT
		);

	#if defined(ENABLE_FMRADIO)
		GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BK1080);
	#endif
}

void BOARD_PORTCON_Init(void)
{
	// PORT A pin selection

	PORTCON_PORTA_SEL0 &= ~(0
		// Key pad
		| PORTCON_PORTA_SEL0_A3_MASK
		// Key pad
		| PORTCON_PORTA_SEL0_A4_MASK
		// Key pad
		| PORTCON_PORTA_SEL0_A5_MASK
		// Key pad
		| PORTCON_PORTA_SEL0_A6_MASK
		);
	PORTCON_PORTA_SEL0 |= 0
		// Key pad
		| PORTCON_PORTA_SEL0_A3_BITS_GPIOA3
		// Key pad
		| PORTCON_PORTA_SEL0_A4_BITS_GPIOA4
		// Key pad
		| PORTCON_PORTA_SEL0_A5_BITS_GPIOA5
		// Key pad
		| PORTCON_PORTA_SEL0_A6_BITS_GPIOA6
		// UART1 TX, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTA_SEL0_A7_BITS_UART1_TX
		;

	PORTCON_PORTA_SEL1 &= ~(0
		// Key pad + I2C
		| PORTCON_PORTA_SEL1_A10_MASK
		// Key pad + I2C
		| PORTCON_PORTA_SEL1_A11_MASK
		// Key pad + Voice chip
		| PORTCON_PORTA_SEL1_A12_MASK
		// Key pad + Voice chip
		| PORTCON_PORTA_SEL1_A13_MASK
		);
	PORTCON_PORTA_SEL1 |= 0
		// UART1 RX, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTA_SEL1_A8_BITS_UART1_RX
		// Battery voltage, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTA_SEL1_A9_BITS_SARADC_CH4
		// Key pad + I2C
		| PORTCON_PORTA_SEL1_A10_BITS_GPIOA10
		// Key pad + I2C
		| PORTCON_PORTA_SEL1_A11_BITS_GPIOA11
		// Key pad + Voice chip
		| PORTCON_PORTA_SEL1_A12_BITS_GPIOA12
		// Key pad + Voice chip
		| PORTCON_PORTA_SEL1_A13_BITS_GPIOA13
		// Battery Current, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTA_SEL1_A14_BITS_SARADC_CH9
		;

	// PORT B pin selection

	PORTCON_PORTB_SEL0 &= ~(0
		// SPI0 SSN
		| PORTCON_PORTB_SEL0_B7_MASK
		);
	PORTCON_PORTB_SEL0 |= 0
		// SPI0 SSN
		| PORTCON_PORTB_SEL0_B7_BITS_SPI0_SSN
		;

	PORTCON_PORTB_SEL1 &= ~(0
		// ST7565
		| PORTCON_PORTB_SEL1_B9_MASK
		// ST7565 + SWD IO
		| PORTCON_PORTB_SEL1_B11_MASK
		// SWD CLK
		| PORTCON_PORTB_SEL1_B14_MASK
		// BK1080
		| PORTCON_PORTB_SEL1_B15_MASK
		);
	PORTCON_PORTB_SEL1 |= 0
		// SPI0 CLK, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTB_SEL1_B8_BITS_SPI0_CLK
		// ST7565
		| PORTCON_PORTB_SEL1_B9_BITS_GPIOB9
		// SPI0 MOSI, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTB_SEL1_B10_BITS_SPI0_MOSI
#if defined(ENABLE_SWD)
		// SWD IO
		| PORTCON_PORTB_SEL1_B11_BITS_SWDIO
		// SWD CLK
		| PORTCON_PORTB_SEL1_B14_BITS_SWCLK
#else
		// ST7565
		| PORTCON_PORTB_SEL1_B11_BITS_GPIOB11
#endif
		;

	// PORT C pin selection

	PORTCON_PORTC_SEL0 &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_SEL0_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_SEL0_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_SEL0_C2_MASK
		// Flash light
		| PORTCON_PORTC_SEL0_C3_MASK
		// Speaker
		| PORTCON_PORTC_SEL0_C4_MASK
		// PTT button
		| PORTCON_PORTC_SEL0_C5_MASK
		);

	// PORT A pin configuration

	PORTCON_PORTA_IE |= 0
		// Keypad
		| PORTCON_PORTA_IE_A3_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_IE_A4_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_IE_A5_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_IE_A6_BITS_ENABLE
		// A7 = UART1 TX disabled by default
		// UART1 RX
		| PORTCON_PORTA_IE_A8_BITS_ENABLE
		;
	PORTCON_PORTA_IE &= ~(0
		// Keypad + I2C
		| PORTCON_PORTA_IE_A10_MASK
		// Keypad + I2C
		| PORTCON_PORTA_IE_A11_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_IE_A12_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_IE_A13_MASK
		);

	PORTCON_PORTA_PU |= 0
		// Keypad
		| PORTCON_PORTA_PU_A3_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_PU_A4_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_PU_A5_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_PU_A6_BITS_ENABLE
		;
	PORTCON_PORTA_PU &= ~(0
		// Keypad + I2C
		| PORTCON_PORTA_PU_A10_MASK
		// Keypad + I2C
		| PORTCON_PORTA_PU_A11_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_PU_A12_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_PU_A13_MASK
		);

	PORTCON_PORTA_PD &= ~(0
		// Keypad
		| PORTCON_PORTA_PD_A3_MASK
		// Keypad
		| PORTCON_PORTA_PD_A4_MASK
		// Keypad
		| PORTCON_PORTA_PD_A5_MASK
		// Keypad
		| PORTCON_PORTA_PD_A6_MASK
		// Keypad + I2C
		| PORTCON_PORTA_PD_A10_MASK
		// Keypad + I2C
		| PORTCON_PORTA_PD_A11_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_PD_A12_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_PD_A13_MASK
		);

	PORTCON_PORTA_OD |= 0
		// Keypad
		| PORTCON_PORTA_OD_A3_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_OD_A4_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_OD_A5_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_OD_A6_BITS_ENABLE
		;
	PORTCON_PORTA_OD &= ~(0
		// Keypad + I2C
		| PORTCON_PORTA_OD_A10_MASK
		// Keypad + I2C
		| PORTCON_PORTA_OD_A11_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_OD_A12_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_OD_A13_MASK
		);

	// PORT B pin configuration

	PORTCON_PORTB_IE |= 0
		| PORTCON_PORTB_IE_B14_BITS_ENABLE
		;
	PORTCON_PORTB_IE &= ~(0
		// Back light
		| PORTCON_PORTB_IE_B6_MASK
		// UART1
		| PORTCON_PORTB_IE_B7_MASK
		| PORTCON_PORTB_IE_B8_MASK
		// ST7565
		| PORTCON_PORTB_IE_B9_MASK
		// SPI0 MOSI
		| PORTCON_PORTB_IE_B10_MASK
#if !defined(ENABLE_SWD)
		// ST7565
		| PORTCON_PORTB_IE_B11_MASK
#endif
		// BK1080
		| PORTCON_PORTB_IE_B15_MASK
		);

	PORTCON_PORTB_PU &= ~(0
		// Back light
		| PORTCON_PORTB_PU_B6_MASK
		// ST7565
		| PORTCON_PORTB_PU_B9_MASK
		// ST7565 + SWD IO
		| PORTCON_PORTB_PU_B11_MASK
		// SWD CLK
		| PORTCON_PORTB_PU_B14_MASK
		// BK1080
		| PORTCON_PORTB_PU_B15_MASK
		);

	PORTCON_PORTB_PD &= ~(0
		// Back light
		| PORTCON_PORTB_PD_B6_MASK
		// ST7565
		| PORTCON_PORTB_PD_B9_MASK
		// ST7565 + SWD IO
		| PORTCON_PORTB_PD_B11_MASK
		// SWD CLK
		| PORTCON_PORTB_PD_B14_MASK
		// BK1080
		| PORTCON_PORTB_PD_B15_MASK
		);

	PORTCON_PORTB_OD &= ~(0
		// Back light
		| PORTCON_PORTB_OD_B6_MASK
		// ST7565
		| PORTCON_PORTB_OD_B9_MASK
		// ST7565 + SWD IO
		| PORTCON_PORTB_OD_B11_MASK
		// BK1080
		| PORTCON_PORTB_OD_B15_MASK
		);

	PORTCON_PORTB_OD |= 0
		// SWD CLK
		| PORTCON_PORTB_OD_B14_BITS_ENABLE
		;

	// PORT C pin configuration

	PORTCON_PORTC_IE |= 0
		// PTT button
		| PORTCON_PORTC_IE_C5_BITS_ENABLE
		;
	PORTCON_PORTC_IE &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_IE_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_IE_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_IE_C2_MASK
		// Flash Light
		| PORTCON_PORTC_IE_C3_MASK
		// Speaker
		| PORTCON_PORTC_IE_C4_MASK
		);

	PORTCON_PORTC_PU |= 0
		// PTT button
		| PORTCON_PORTC_PU_C5_BITS_ENABLE
		;
	PORTCON_PORTC_PU &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_PU_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_PU_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_PU_C2_MASK
		// Flash Light
		| PORTCON_PORTC_PU_C3_MASK
		// Speaker
		| PORTCON_PORTC_PU_C4_MASK
		);

	PORTCON_PORTC_PD &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_PD_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_PD_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_PD_C2_MASK
		// Flash Light
		| PORTCON_PORTC_PD_C3_MASK
		// Speaker
		| PORTCON_PORTC_PD_C4_MASK
		// PTT Button
		| PORTCON_PORTC_PD_C5_MASK
		);

	PORTCON_PORTC_OD &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_OD_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_OD_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_OD_C2_MASK
		// Flash Light
		| PORTCON_PORTC_OD_C3_MASK
		// Speaker
		| PORTCON_PORTC_OD_C4_MASK
		);
	PORTCON_PORTC_OD |= 0
		// BK4819 SCN
		| PORTCON_PORTC_OD_C0_BITS_DISABLE
		// BK4819 SCL
		| PORTCON_PORTC_OD_C1_BITS_DISABLE
		// BK4819 SDA
		| PORTCON_PORTC_OD_C2_BITS_DISABLE
		// Flash Light
		| PORTCON_PORTC_OD_C3_BITS_DISABLE
		// Speaker
		| PORTCON_PORTC_OD_C4_BITS_DISABLE
		// PTT button
		| PORTCON_PORTC_OD_C5_BITS_ENABLE
		;
}

void BOARD_ADC_Init(void)
{
	ADC_Config_t Config;

	Config.CLK_SEL            = SYSCON_CLK_SEL_W_SARADC_SMPL_VALUE_DIV2;
	Config.CH_SEL             = ADC_CH4 | ADC_CH9;
	Config.AVG                = SARADC_CFG_AVG_VALUE_8_SAMPLE;
	Config.CONT               = SARADC_CFG_CONT_VALUE_SINGLE;
	Config.MEM_MODE           = SARADC_CFG_MEM_MODE_VALUE_CHANNEL;
	Config.SMPL_CLK           = SARADC_CFG_SMPL_CLK_VALUE_INTERNAL;
	Config.SMPL_WIN           = SARADC_CFG_SMPL_WIN_VALUE_15_CYCLE;
	Config.SMPL_SETUP         = SARADC_CFG_SMPL_SETUP_VALUE_1_CYCLE;
	Config.ADC_TRIG           = SARADC_CFG_ADC_TRIG_VALUE_CPU;
	Config.CALIB_KD_VALID     = SARADC_CALIB_KD_VALID_VALUE_YES;
	Config.CALIB_OFFSET_VALID = SARADC_CALIB_OFFSET_VALID_VALUE_YES;
	Config.DMA_EN             = SARADC_CFG_DMA_EN_VALUE_DISABLE;
	Config.IE_CHx_EOC         = SARADC_IE_CHx_EOC_VALUE_NONE;
	Config.IE_FIFO_FULL       = SARADC_IE_FIFO_FULL_VALUE_DISABLE;
	Config.IE_FIFO_HFULL      = SARADC_IE_FIFO_HFULL_VALUE_DISABLE;

	ADC_Configure(&Config);
	ADC_Enable();
	ADC_SoftReset();
}

void BOARD_ADC_GetBatteryInfo(uint16_t *pVoltage)
{
	ADC_Start();	
	int safety = 120; // max 100 interruptions
	while (!ADC_CheckEndOfConversion(ADC_CH9)&& --safety > 0) {}
	*pVoltage = ADC_GetValue(ADC_CH4);
}

void BOARD_Init(void)
{
	BOARD_PORTCON_Init();
	BOARD_GPIO_Init();
	BACKLIGHT_InitHardware();
	BOARD_ADC_Init();
	ST7565_Init(true);
	#ifdef ENABLE_UART
	CRC_Init();
	#endif
}

void BOARD_EEPROM_Init(void)
{
	uint8_t      Data[16];

	memset(Data, 0, sizeof(Data));

	// 0E70..0E77
	EEPROM_ReadBuffer(0x0E70, Data, 8);
	gEeprom.SQUELCH_LEVEL        = (Data[1] < 10) ? Data[1] : 1;
	gEeprom.TX_TIMEOUT_TIMER     = (Data[2] < 11) ? Data[2] : 1;
	gEeprom.KEY_LOCK             = (Data[4] <  2) ? Data[4] : false;
	gEeprom.MIC_SENSITIVITY      = (Data[7] <  5) ? Data[7] : 0;

	// 0E78..0E7F
	EEPROM_ReadBuffer(0x0E78, Data, 8);
	gEeprom.BACKLIGHT_MAX 		  = (Data[0] & 0xF) <= 10 ? (Data[0] & 0xF) : 10;
	gEeprom.BACKLIGHT_MIN 		  = (Data[0] >> 4) < gEeprom.BACKLIGHT_MAX ? (Data[0] >> 4) : 0;
#ifdef ENABLE_BLMIN_TMP_OFF
	gEeprom.BACKLIGHT_MIN_STAT	  = BLMIN_STAT_ON;
#endif
	gEeprom.BATTERY_SAVE          = (Data[3] < 5) ? Data[3] : 4;
	gEeprom.BACKLIGHT_TIME        = (Data[5] < ARRAY_SIZE(gSubMenu_BACKLIGHT)) ? Data[5] : 3;
	gEeprom.VFO_OPEN              = (Data[7] < 2) ? Data[7] : true;

	// 0E80..0E87
	uint16_t      Data16[4];
	EEPROM_ReadBuffer(0x0E80, Data16, sizeof(Data16));
	gEeprom.ScreenChannel = Data16[0];
	gEeprom.MrChannel 	  = Data16[1];
	gEeprom.FreqChannel   = Data16[2];
	gEeprom.FM_FrequencyPlaying = Data16[3];
	
	// validate that its within the supported range
	//if(gEeprom.FM_FrequencyPlaying < FM_RADIO_MIN_FREQ || gEeprom.FM_FrequencyPlaying > FM_RADIO_MAX_FREQ)
	//	gEeprom.FM_FrequencyPlaying = FM_RADIO_MIN_FREQ;

	// 0E90..0E99
	EEPROM_ReadBuffer(0x0E90, Data, 8);
	gEeprom.KEY_1_SHORT_PRESS_ACTION     = (Data[1] < ACTION_OPT_LEN) ? Data[1] : ACTION_OPT_MONITOR;
	gEeprom.KEY_1_LONG_PRESS_ACTION      = (Data[2] < ACTION_OPT_LEN) ? Data[2] : ACTION_OPT_FLASHLIGHT;
	gEeprom.KEY_2_SHORT_PRESS_ACTION     = (Data[3] < ACTION_OPT_LEN) ? Data[3] : ACTION_OPT_TOGGLE_PTT;
	gEeprom.KEY_2_LONG_PRESS_ACTION      = (Data[4] < ACTION_OPT_LEN) ? Data[4] : ACTION_OPT_VFO_MR;
	gEeprom.POWER_ON_DISPLAY_MODE        = (Data[7] < 4)              ? Data[7] : POWER_ON_DISPLAY_MODE_MESSAGE;
	// 0EA8..0EAF
	EEPROM_ReadBuffer(0x0EA8, Data, 8);
	gEeprom.ROGER                          = (Data[1] <  7) ? Data[1] : ROGER_MODE_OFF;
	// Data[2] empty slot
	
	gEeprom.BATTERY_TYPE                   = (Data[4] < BATTERY_TYPE_UNKNOWN) ? Data[4] : BATTERY_TYPE_1600_MAH;
	gEeprom.SQL_TONE                       = (Data[5] <  ARRAY_SIZE(CTCSS_Options)) ? Data[5] : 50;
	// 0ED0..0ED7

	// 0F40..0F47
	EEPROM_ReadBuffer(0x0F40, Data, 8);
	gSetting_F_LOCK            = (Data[0] < F_LOCK_LEN) ? Data[0] : F_UNLOCK_PMR;
	gSetting_ScrambleEnable    = (Data[6] < 2) ? Data[6] : true;
	gSetting_battery_text      = (((Data[7] >> 2) & 3u) <= 2) ? (Data[7] >> 2) & 3 : 2;
	gSetting_backlight_on_tx_rx = (Data[7] >> 6) & 3u;
	// Read RxOffset setting
	EEPROM_ReadBuffer(0x0EA0, Data, 4);
    memmove(&gEeprom.RX_OFFSET, Data, 4);
	// Make sure it inits with some sane value
	gEeprom.RX_OFFSET = gEeprom.RX_OFFSET > RX_OFFSET_MAX ? 0 : gEeprom.RX_OFFSET;

	if (!gEeprom.VFO_OPEN)
	{
		gEeprom.ScreenChannel = gEeprom.MrChannel;
	}
#ifdef ENABLE_EEPROM_512K
	EEPROM_ReadBuffer(ADRESS_ATTRIBUTES         , gMR_ChannelAttributes			, 200);
	EEPROM_ReadBuffer(ADRESS_ATTRIBUTES + 0x00C8, gMR_ChannelAttributes + 0x00C8, 200);
	EEPROM_ReadBuffer(ADRESS_ATTRIBUTES + 0x0190, gMR_ChannelAttributes + 0x0190, 200);
	EEPROM_ReadBuffer(ADRESS_ATTRIBUTES + 0x0258, gMR_ChannelAttributes + 0x0258, 200);
	EEPROM_ReadBuffer(ADRESS_ATTRIBUTES + 0x0320, gMR_ChannelAttributes + 0x0320, 200);
#else
	EEPROM_ReadBuffer(ADRESS_ATTRIBUTES			, gMR_ChannelAttributes, 		  200);
#endif
	for(uint16_t i = 0; i < MR_CHANNEL_LAST+1; i++) {
		ChannelAttributes_t *att = &gMR_ChannelAttributes[i];
		if(att->__val == 0xff){
			att->__val = 0;
			att->band = 0xf;
		}
	}
	
	BOARD_gMR_LoadChannels();
	
}

// Load Chanel frequencies, names into global memory lookup table
void BOARD_gMR_LoadChannels() {
	uint16_t  i;
	uint32_t freq_buf;

	for (i = MR_CHANNEL_FIRST; i <= MR_CHANNEL_LAST; i++)
	{
		freq_buf = BOARD_fetchChannelFrequency(i);

		gMR_ChannelFrequencyAttributes[i].Frequency = RX_freq_check(freq_buf) == 0xFF ? 0 : freq_buf;
	}
}


void BOARD_EEPROM_LoadCalibration(void)
{
//	uint8_t Mic;

	EEPROM_ReadBuffer(0x1EC0, gEEPROM_RSSI_CALIB[3], 8);
	memcpy(gEEPROM_RSSI_CALIB[4], gEEPROM_RSSI_CALIB[3], 8);
	memcpy(gEEPROM_RSSI_CALIB[5], gEEPROM_RSSI_CALIB[3], 8);
	memcpy(gEEPROM_RSSI_CALIB[6], gEEPROM_RSSI_CALIB[3], 8);

	EEPROM_ReadBuffer(0x1EC8, gEEPROM_RSSI_CALIB[0], 8);
	memcpy(gEEPROM_RSSI_CALIB[1], gEEPROM_RSSI_CALIB[0], 8);
	memcpy(gEEPROM_RSSI_CALIB[2], gEEPROM_RSSI_CALIB[0], 8);

	EEPROM_ReadBuffer(0x1F40, gBatteryCalibration, 12);
	if (gBatteryCalibration[0] >= 5000)
	{
		gBatteryCalibration[0] = 1900;
		gBatteryCalibration[1] = 2000;
	}
	gBatteryCalibration[5] = 2300;

	gEeprom.MIC_SENSITIVITY_TUNING = gMicGain_dB2[gEeprom.MIC_SENSITIVITY];

	{
		struct
		{
			int16_t  BK4819_XtalFreqLow;
			uint16_t EEPROM_1F8A;
			uint16_t EEPROM_1F8C;
			uint8_t  VOLUME_GAIN;
			uint8_t  DAC_GAIN;
		} __attribute__((packed)) Misc;

		// radio 1 .. 04 00 46 00 50 00 2C 0E
		// radio 2 .. 05 00 46 00 50 00 2C 0E
		EEPROM_ReadBuffer(0x1F88, &Misc, 8);

		gEeprom.BK4819_XTAL_FREQ_LOW = (Misc.BK4819_XtalFreqLow >= -1000 && Misc.BK4819_XtalFreqLow <= 1000) ? Misc.BK4819_XtalFreqLow : 0;
		gEEPROM_1F8A                 = Misc.EEPROM_1F8A & 0x01FF;
		gEEPROM_1F8C                 = Misc.EEPROM_1F8C & 0x01FF;
		gEeprom.VOLUME_GAIN          = (Misc.VOLUME_GAIN < 64) ? Misc.VOLUME_GAIN : 58;
		gEeprom.DAC_GAIN             = (Misc.DAC_GAIN    < 16) ? Misc.DAC_GAIN    : 8;

		BK4819_WriteRegister(BK4819_REG_3B, 22656 + gEeprom.BK4819_XTAL_FREQ_LOW);
//		BK4819_WriteRegister(BK4819_REG_3C, gEeprom.BK4819_XTAL_FREQ_HIGH);
	}
}

uint32_t BOARD_fetchChannelFrequency(const uint16_t Channel)
{
	struct
	{
		uint32_t frequency;
		uint32_t offset;
	} __attribute__((packed)) info;

	EEPROM_ReadBuffer(ADRESS_FREQ_PARAMS + Channel * 16, &info, sizeof(info));
	if (info.frequency == 0xFFFFFFFF) return 0;
	else return info.frequency;
}
uint16_t BOARD_gMR_fetchChannel(const uint32_t freq)
	{
		for (uint16_t i = MR_CHANNEL_FIRST; i <= MR_CHANNEL_LAST; i++) {
			if (gMR_ChannelFrequencyAttributes[i].Frequency == freq)
				return i;
		}
		// Return if no Chanel found
		return 0xFFFF;
	}

static const uint32_t gDefaultFrequencyTable[] =
{
	14500000,    //
	14550000,    //
	43300000,    //
	43320000,    //
	43350000     //
};

void BOARD_FactoryReset(int32_t gSubMenuSelection)
{
	uint16_t i;
	uint8_t  Template[8];
	memset(Template, 0xFF, sizeof(Template));
    
	if (gSubMenuSelection) { //Erase ALL but calibration
	
		for (i = 0x0000; i < 0x1E00; i += 8) EEPROM_WriteBuffer(i, Template);
		#ifdef ENABLE_EEPROM_512K
			for (i = ADRESS_FREQ_PARAMS; i < ADRESS_HISTORY; i += 8) EEPROM_WriteBuffer(i, Template);
		#endif
	}
	else { //Erase ALL but calibration and memories
		for (i = 0x0C80; i < 0xD5F; i += 8) EEPROM_WriteBuffer(i, Template);
		for (i = 0x0E28; i < 0xF4F; i += 8) EEPROM_WriteBuffer(i, Template);
		for (i = 0x1BD0; i < 0x1DFF; i += 8) EEPROM_WriteBuffer(i, Template);
#ifdef ENABLE_EEPROM_512K
		for (i = ADRESS_HISTORY; i < ADRESS_HISTORY + 100 * 6; i += 8) EEPROM_WriteBuffer(i, Template); //sizeof(HistoryStruct) = 6
#endif
	}

	RADIO_InitInfo(gTxVfo, FREQ_CHANNEL_FIRST + BAND6_400MHz, 44600625);
	gEeprom.RX_OFFSET = 0;
	SETTINGS_SaveSettings();
	// set the first few memory channels
	for (i = 0; i < ARRAY_SIZE(gDefaultFrequencyTable); i++)
	{
		const uint32_t Frequency   = gDefaultFrequencyTable[i];
		gTxVfo->freq_config_RX.Frequency = Frequency;
		gTxVfo->freq_config_TX.Frequency = Frequency;
		gTxVfo->Band               = FREQUENCY_GetBand(Frequency);
		SETTINGS_SaveChannel(FREQ_CHANNEL_FIRST + i, 0, 2);
	}
	ClearSettings();
}