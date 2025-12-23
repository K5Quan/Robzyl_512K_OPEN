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

#include <stdio.h>   // NULL


#include "bk4819.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "settings.h"
//#include "debugging.h"
#include "app/spectrum.h"

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define scnsclsda() do { \
    GPIOC->DATA |= (1U << GPIOC_PIN_BK4819_SCN) | \
                   (1U << GPIOC_PIN_BK4819_SCL) | \
                   (1U << GPIOC_PIN_BK4819_SDA); \
} while(0)


#define SCN_SCL_SCN_SEQ_OPT() do { \
    GPIOC->DATA |= (1U << GPIOC_PIN_BK4819_SCN); \
    GPIOC->DATA &= ~(1U << GPIOC_PIN_BK4819_SCL); \
	__asm volatile ( "nop \n" "nop \n"); \
    GPIOC->DATA &= ~(1U << GPIOC_PIN_BK4819_SCN); \
} while(0)

uint16_t regs_cache[128] = {[0 ... 127] = 0xFFFF};

static uint16_t gBK4819_GpioOutState;

bool gRxIdleMode;

void AUDIO_AudioPathOn(void) {
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
}

void AUDIO_AudioPathOff(void) {
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
}

__inline uint16_t scale_freq(const uint16_t freq)
{
//	return (((uint32_t)freq * 1032444u) + 50000u) / 100000u;   // with rounding
	return (((uint32_t)freq * 1353245u) + (1u << 16)) >> 17;   // with rounding
}

void BK4819_Init(void)
{
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

	BK4819_WriteRegister(BK4819_REG_00, 0x8000);
	BK4819_WriteRegister(BK4819_REG_00, 0x0000);

	BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
	BK4819_WriteRegister(BK4819_REG_36, 0x0022);

	BK4819_SetDefaultAmplifierSettings();

	BK4819_WriteRegister(BK4819_REG_19, 0b0001000001000001);   // <15> MIC AGC  1 = disable  0 = enable

	BK4819_WriteRegister(BK4819_REG_7D, 0xE940);

	// REG_48 .. RX AF level
	//
	// <15:12> 11  ???  0 to 15
	//
	// <11:10> 0 AF Rx Gain-1
	//         0 =   0dB
	//         1 =  -6dB
	//         2 = -12dB
	//         3 = -18dB
	//
	// <9:4>   60 AF Rx Gain-2  -26dB ~ 5.5dB   0.5dB/step
	//         63 = max
	//          0 = mute
	//
	// <3:0>   15 AF DAC Gain (after Gain-1 and Gain-2) approx 2dB/step
	//         15 = max
	//          0 = min
	//
	BK4819_WriteRegister(BK4819_REG_48,	//  0xB3A8);     // 1011 00 111010 1000
		(11u << 12) |     // ??? 0..15
		( 0u << 10) |     // AF Rx Gain-1
		(58u <<  4) |     // AF Rx Gain-2
		( 8u <<  0));     // AF DAC Gain (after Gain-1 and Gain-2)


	BK4819_WriteRegister(BK4819_REG_1F, 0x5454);
	BK4819_WriteRegister(BK4819_REG_3E, 0xA037);

	gBK4819_GpioOutState = 0x9000;

	BK4819_WriteRegister(BK4819_REG_33, 0x9000);
	BK4819_WriteRegister(BK4819_REG_3F, 0);
	BK4819_WriteRegister(BK4819_REG_73, 0x4692);

}

static uint16_t BK4819_ReadU16(void) {
	uint8_t i;
	uint16_t Value;

	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_ENABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_INPUT;

	Value = 0;
	for (i = 0; i < 16; ++i) {
	SYSTICK_DelayUs(0);//
	Value <<= 1;
	Value |= GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

	}
	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) |
					 PORTCON_PORTC_IE_C2_BITS_DISABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_OUTPUT;

	return Value;
}

static void BK4819_WriteU8(uint8_t Data) {
	uint8_t i;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (i = 0; i < 8; ++i) {
	if ((Data & 0x80U) == 0) {
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
	} else {
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
	}
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	Data <<= 1;
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	}
}


static void BK4819_WriteU16(uint16_t Data) {
	uint8_t i;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (i = 0; i < 16; ++i) {
	if ((Data & 0x8000U) == 0U) {
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
	} else {
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
	}
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	Data <<= 1;
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	}
}

uint16_t BK4819_ReadRegister(BK4819_REGISTER_t Register) {
	uint16_t Value;

	SCN_SCL_SCN_SEQ_OPT();
	BK4819_WriteU8(Register | 0x80);
	Value = BK4819_ReadU16();
	scnsclsda();
	regs_cache[Register] = Value;

	return Value;
}



void BK4819_WriteRegister(BK4819_REGISTER_t Register, uint16_t Data) {
	
	if(Data == regs_cache[Register])return;
	regs_cache[Register] = Data;

	SCN_SCL_SCN_SEQ_OPT();
	BK4819_WriteU8(Register);
	BK4819_WriteU16(Data);
	scnsclsda();
}

int16_t BK4819_GetAFCValue() { //from Hawk5
  int16_t signedAfc = (int16_t)BK4819_ReadRegister(0x6D);
  // * 3.3(3)
  return (signedAfc * 10) / 3;
}

void BK4819_SetAGC(bool enable)
{
	uint16_t regVal = BK4819_ReadRegister(BK4819_REG_7E);
	if(!(regVal & (1 << 15)) == enable)
		return;

	BK4819_WriteRegister(BK4819_REG_7E, (regVal & ~(1 << 15) & ~(0b111 << 12)) 
		| (!enable << 15)   // 0  AGC fix mode
		| (0b100 << 12)       // 3  AGC fix index -> changed to min as experiment
	);

	// if(enable) {
	// 	BK4819_WriteRegister(BK4819_REG_7B, 0x8420);
	// }
	// else {
	// 	BK4819_WriteRegister(BK4819_REG_7B, 0x318C);
	
	// 	BK4819_WriteRegister(BK4819_REG_7C, 0x595E);
	// 	BK4819_WriteRegister(BK4819_REG_20, 0x8DEF);

	// 	for (uint8_t i = 0; i < 8; i++) {
	// 		//BK4819_WriteRegister(BK4819_REG_06, ((i << 13) | 0x2500u) + 0x036u);
	// 		BK4819_WriteRegister(BK4819_REG_06, (i & 7) << 13 | 0x4A << 7 | 0x36);
	// 	}
	// }
}

// REG_10, REG_11, REG_12 REG_13, REG_14
	//
	// Rx AGC Gain Table[]. (Index Max->Min is 3,2,1,0,-1)
	//
	// <15:10> ???
	//
	// <9:8>   LNA Gain Short 
	//         3 =   0dB  <<<		1o11				read from spectrum			reference manual
	//         2 = 					-24dB  				-19     					 -11
	//         1 = 					-30dB  				-24     					 -16
	//         0 = 					-33dB  				-28     					 -19
	//
	// <7:5>   LNA Gain
	//         7 =   0dB
	//         6 =  -2dB
	//         5 =  -4dB
	//         4 =  -6dB
	//         3 =  -9dB
	//         2 = -14dB <<<
	//         1 = -19dB
	//         0 = -24dB
	//
	// <4:3>   MIXER Gain
	//         3 =   0dB <<<
	//         2 =  -3dB
	//         1 =  -6dB
	//         0 =  -8dB
	//
	// <2:0>   PGA Gain
	//         7 =   0dB
	//         6 =  -3dB <<<
	//         5 =  -6dB
	//         4 =  -9dB
	//         3 = -15dB
	//         2 = -21dB
	//         1 = -27dB
	//         0 = -33dB
	//

void BK4819_InitAGC(const uint8_t agcType, ModulationMode_t modulation)
{
	
	
	uint16_t uVar1 = BK4819_ReadRegister(0x31);// HACK FIXME AM
	if(modulation==MODULATION_AM)
	{
		//AM modulation
		switch(agcType)
		{	
			case RX_AGC_SLOW:
				BK4819_WriteRegister(BK4819_REG_49, (0 << 14) | (50 << 7) | (15 << 0));
				break;
			case RX_AGC_FAST:
				BK4819_WriteRegister(BK4819_REG_49, (0 << 14) | (50 << 7) | (25 << 0));
				break;
			default:
				return;
		}
		BK4819_WriteRegister(0x31,uVar1 | 1);		// HACK FIXME AM
		BK4819_WriteRegister(0x42,0x6F5C);		// HACK FIXME AM
		BK4819_WriteRegister(0x2A,0x7434);		// HACK FIXME AM
		BK4819_WriteRegister(0x2B,0x0600);		// HACK FIXME AM
		BK4819_WriteRegister(0x2F,0x9990);		// HACK FIXME AM
	}
	else
	{
		//FM, USB modulation
				switch(agcType)
		{	
			case RX_AGC_SLOW:
				BK4819_WriteRegister(BK4819_REG_49, (0 << 14) | (84 << 7) | (56 << 0));
				break;
			case RX_AGC_FAST:
				BK4819_WriteRegister(BK4819_REG_49, (0 << 14) | (84 << 7) | (66 << 0));
				break;
			default:
				return;
		}
		BK4819_WriteRegister(0x31,uVar1 & 0xFFFFFFFE); // HACK FIXME AM
		BK4819_WriteRegister(0x42,0x6B5A);			   // HACK FIXME AM
		BK4819_WriteRegister(0x2A,0x7400);			   // HACK FIXME AM
		BK4819_WriteRegister(0x2B,0);				   // HACK FIXME AM
		BK4819_WriteRegister(0x2F,0x9890);			   // HACK FIXME AM
	
	}
		
	// switched values to ones from 1o11 am_fix:
	//BK4819_WriteRegister(BK4819_REG_7B, 0x8420); //Test 4.15
	BK4819_WriteRegister(BK4819_REG_12, 0x0393);  // 0x037B / 000000 11 011 11 011 / -24dB
	BK4819_WriteRegister(BK4819_REG_11, 0x01B5);  // 0x027B / 000000 10 011 11 011 / -43dB
	BK4819_WriteRegister(BK4819_REG_10, 0x0145);  // 0x007A / 000000 00 011 11 010 / -58dB
	BK4819_WriteRegister(BK4819_REG_14, 0x0019);  // 0x0019 / 000000 00 000 11 001 / -84dB
	
	//30, 10 - doesn't overload but sound low
	//50, 10 - best so far
	//50, 15, - SOFT - signal doesn't fall too low - works best for now
	//45, 25 - AGRESSIVE - lower histeresis, but volume jumps heavily, not good for music, might be good for aviation
	//1 << 14 - way better, seems to open squelch and match squelch as opposed to 0
	// what is this for? turned off, seems like rssi is increased?
	// BK4819_WriteRegister(BK4819_REG_7B, 0x8420); 

}



void BK4819_ToggleGpioOut(BK4819_GPIO_PIN_t Pin, bool bSet)
{
	if (bSet)
		gBK4819_GpioOutState |=  (0x40u >> Pin);
	else
		gBK4819_GpioOutState &= ~(0x40u >> Pin);

	BK4819_WriteRegister(BK4819_REG_33, gBK4819_GpioOutState);
}

void BK4819_SetCDCSSCodeWord(uint32_t CodeWord)
{
	// REG_51
	//
	// <15>  0
	//       1 = Enable TxCTCSS/CDCSS
	//       0 = Disable
	//
	// <14>  0
	//       1 = GPIO0Input for CDCSS
	//       0 = Normal Mode (for BK4819 v3)
	//
	// <13>  0
	//       1 = Transmit negative CDCSS code
	//       0 = Transmit positive CDCSS code
	//
	// <12>  0 CTCSS/CDCSS mode selection
	//       1 = CTCSS
	//       0 = CDCSS
	//
	// <11>  0 CDCSS 24/23bit selection
	//       1 = 24bit
	//       0 = 23bit
	//
	// <10>  0 1050HzDetectionMode
	//       1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	//
	// <9>   0 Auto CDCSS Bw Mode
	//       1 = Disable
	//       0 = Enable
	//
	// <8>   0 Auto CTCSS Bw Mode
	//       0 = Enable
	//       1 = Disable
	//
	// <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//       0   = min
	//       127 = max

	// Enable CDCSS
	// Transmit positive CDCSS code
	// CDCSS Mode
	// CDCSS 23bit
	// Enable Auto CDCSS Bw Mode
	// Enable Auto CTCSS Bw Mode
	// CTCSS/CDCSS Tx Gain1 Tuning = 51
	//
	BK4819_WriteRegister(BK4819_REG_51,
		BK4819_REG_51_ENABLE_CxCSS         |
		BK4819_REG_51_GPIO6_PIN2_NORMAL    |
		BK4819_REG_51_TX_CDCSS_POSITIVE    |
		BK4819_REG_51_MODE_CDCSS           |
		BK4819_REG_51_CDCSS_23_BIT         |
		BK4819_REG_51_1050HZ_NO_DETECTION  |
		BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
		BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
		(51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail 55Hz Rx detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | 2775u);

	// REG_08 <15:0> <15> = 1 for CDCSS high 12bit
	//               <15> = 0 for CDCSS low  12bit
	// <11:0> = CDCSShigh/low 12bit code
	//
	BK4819_WriteRegister(BK4819_REG_08, (0u << 15) | ((CodeWord >>  0) & 0x0FFF)); // LS 12-bits
	BK4819_WriteRegister(BK4819_REG_08, (1u << 15) | ((CodeWord >> 12) & 0x0FFF)); // MS 12-bits
}

void BK4819_SetCTCSSFrequency(uint32_t FreqControlWord)
{
	// REG_51 <15>  0                                 1 = Enable TxCTCSS/CDCSS           0 = Disable
	// REG_51 <14>  0                                 1 = GPIO0Input for CDCSS           0 = Normal Mode.(for BK4819v3)
	// REG_51 <13>  0                                 1 = Transmit negative CDCSS code   0 = Transmit positive CDCSScode
	// REG_51 <12>  0 CTCSS/CDCSS mode selection      1 = CTCSS                          0 = CDCSS
	// REG_51 <11>  0 CDCSS 24/23bit selection        1 = 24bit                          0 = 23bit
	// REG_51 <10>  0 1050HzDetectionMode             1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	// REG_51 <9>   0 Auto CDCSS Bw Mode              1 = Disable                        0 = Enable.
	// REG_51 <8>   0 Auto CTCSS Bw Mode              0 = Enable                         1 = Disable
	// REG_51 <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning     0 = min                            127 = max

	uint16_t Config;
	if (FreqControlWord == 2625)
	{	// Enables 1050Hz detection mode
		// Enable TxCTCSS
		// CTCSS Mode
		// 1050/4 Detect Enable
		// Enable Auto CDCSS Bw Mode
		// Enable Auto CTCSS Bw Mode
		// CTCSS/CDCSS Tx Gain1 Tuning = 74
		//
		Config = 0x944A;   // 1 0 0 1 0 1 0 0 0 1001010
	}
	else
	{	// Enable TxCTCSS
		// CTCSS Mode
		// Enable Auto CDCSS Bw Mode
		// Enable Auto CTCSS Bw Mode
		// CTCSS/CDCSS Tx Gain1 Tuning = 74
		//
		Config = 0x904A;   // 1 0 0 1 0 0 0 0 0 1001010
	}
	BK4819_WriteRegister(BK4819_REG_51, Config);

	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail RX detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | (((FreqControlWord * 206488u) + 50000u) / 100000u));   // with rounding
}

// freq_10Hz is CTCSS Hz * 10
void BK4819_SetTailDetection(const uint32_t freq_10Hz)
{
	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail RX detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC2 | ((253910 + (freq_10Hz / 2)) / freq_10Hz));  // with rounding
}

void BK4819_EnableVox(uint16_t VoxEnableThreshold, uint16_t VoxDisableThreshold, uint8_t VoxDelay)
{
	//VOX Algorithm
	//if (voxamp>VoxEnableThreshold)                VOX = 1;
	//else
	//if (voxamp<VoxDisableThreshold) (After Delay) VOX = 0;

	const uint16_t REG_31_Value = BK4819_ReadRegister(BK4819_REG_31);

	// 0xA000 is undocumented?
	BK4819_WriteRegister(BK4819_REG_46, 0xA000 | (VoxEnableThreshold & 0x07FF));

	// 0x1800 is undocumented?
	BK4819_WriteRegister(BK4819_REG_79, 0x1800 | (VoxDisableThreshold & 0x07FF));

	// Set VOX delay
	// Bottom 12 bits are undocumented, 15:12 vox disable delay *128ms
	// BK4819_WriteRegister(BK4819_REG_7A, 0x289A); // vox disable delay = 128*5 = 640ms
	// max delay = F89A = 1920ms
	// min delay = 089A = 0ms
	BK4819_WriteRegister(BK4819_REG_7A, (0x289A & ~(0xF << 12))|(VoxDelay<<12));
	
    // Enable VOX
	BK4819_WriteRegister(BK4819_REG_31, REG_31_Value | (1u << 2));    // VOX Enable
}

//1o11
// // filter bandwidth lowers when signal is low
// const uint16_t listenBWRegDynamicValues[5] = {
// 	0x45a8, // 25
// 	0x4408, // 12.5
// 	0x1148,	// 8.33
// 	0x4458, // 6.25
// 	0x0058  // 5
// };

// // filter bandwidth stays the same when signal is low
// const uint16_t listenBWRegValues[5] = {
// 	0x49a8, // 25
// 	0x4808, // 12.5
// 	0x1348,	// 8.33
// 	0x4858, // 6.25
// 	0x0058  // 5
// };
//fagci (narrower 25, 12.5)
// filter bandwidth lowers when signal is low
const uint16_t listenBWRegDynamicValues[5] = {
	0x3428, // 25
	0x3448, // 12.5
	0x4458,	// 8.33
	0x1148, // 6.25
	0x0058  // 5
};

// filter bandwidth stays the same when signal is low
const uint16_t listenBWRegValues[5] = {
	0x3628, // 25
	0x3648, // 12.5
	0x4858,	// 8.33
	0x1348, // 6.25
	0x0058  // 5
};

/* 	
	Sets filter bandwidth
    dynamic:  if set to true, it will use dynamic filters that lower bandwidth when signal is low
*/
void BK4819_SetFilterBandwidth(const BK4819_FilterBandwidth_t Bandwidth, const bool dynamic)
{
	BK4819_WriteRegister(BK4819_REG_43, dynamic==true ? listenBWRegDynamicValues[Bandwidth] : listenBWRegValues[Bandwidth]);
}

void BK4819_SetupPowerAmplifier(uint8_t Bias, uint32_t Frequency) {
  uint8_t Gain;

  if (Frequency < 28000000) {
    // Gain 1 = 1
    // Gain 2 = 0
    Gain = 0x08U;
  } else {
    // Gain 1 = 4
    // Gain 2 = 2
    Gain = 0x22U;
  }
  // Enable PACTLoutput
  BK4819_WriteRegister(BK4819_REG_36, (Bias << 8) | 0x80U | Gain);
}

void BK4819_SetFrequency(uint32_t Frequency)
{
	
	BK4819_WriteRegister(BK4819_REG_38, (Frequency >>  0) & 0xFFFF);
	BK4819_WriteRegister(BK4819_REG_39, (Frequency >> 16) & 0xFFFF);
}

void BK4819_SetupSquelch(
		uint8_t SquelchOpenRSSIThresh,
		uint8_t SquelchCloseRSSIThresh,
		uint8_t SquelchOpenNoiseThresh,
		uint8_t SquelchCloseNoiseThresh,
		uint8_t SquelchCloseGlitchThresh,
		uint8_t SquelchOpenGlitchThresh)
{
	// REG_70
	//
	// <15>   0 Enable TONE1
	//        1 = Enable
	//        0 = Disable
	//
	// <14:8> 0 TONE1 tuning gain
	//        0 ~ 127
	//
	// <7>    0 Enable TONE2
	//        1 = Enable
	//        0 = Disable
	//
	// <6:0>  0 TONE2/FSK tuning gain
	//        0 ~ 127
	//
	BK4819_WriteRegister(BK4819_REG_70, 0);

	// Glitch threshold for Squelch = close
	//
	// 0 ~ 255
	//
	BK4819_WriteRegister(BK4819_REG_4D, 0xA000 | SquelchCloseGlitchThresh);

	// REG_4E
	//
	// <15:14> 1 ???
	//
	// <13:11> 5 Squelch = open  Delay Setting
	//         0 ~ 7
	//
	// <10:9>  7 Squelch = close Delay Setting
	//         0 ~ 3
	//
	// <8>     0 ???
	//
	// <7:0>   8 Glitch threshold for Squelch = open
	//         0 ~ 255
	//
	BK4819_WriteRegister(BK4819_REG_4E,  // 01 101 11 1 00000000

		// original (*)
	(1u << 14) |                  //  1 ???
	(5u << 11) |                  // *5  squelch = open  delay .. 0 ~ 7
	(3u <<  9) |                  // *3  squelch = close delay .. 0 ~ 3
	SquelchOpenGlitchThresh);     //  0 ~ 255


	// REG_4F
	//
	// <14:8> 47 Ex-noise threshold for Squelch = close
	//        0 ~ 127
	//
	// <7>    ???
	//
	// <6:0>  46 Ex-noise threshold for Squelch = open
	//        0 ~ 127
	//
	BK4819_WriteRegister(BK4819_REG_4F, ((uint16_t)SquelchCloseNoiseThresh << 8) | SquelchOpenNoiseThresh);

	// REG_78
	//
	// <15:8> 72 RSSI threshold for Squelch = open    0.5dB/step
	//
	// <7:0>  70 RSSI threshold for Squelch = close   0.5dB/step
	//
	BK4819_WriteRegister(BK4819_REG_78, ((uint16_t)SquelchOpenRSSIThresh   << 8) | SquelchCloseRSSIThresh);

	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_RX_TurnOn();
}

// Set RF RX front end gain original QS front end register settings
// 0x03BE   00000 011 101 11 110
void BK4819_SetDefaultAmplifierSettings()
{
	BK4819_WriteRegister(BK4819_REG_13, 0x03BE);
}


void BK4819_SetAF(BK4819_AF_Type_t AF)
{
	// AF Output Inverse Mode = Inverse
	// Undocumented bits 0x2040
	//
//	BK4819_WriteRegister(BK4819_REG_47, 0x6040 | (AF << 8));
	BK4819_WriteRegister(BK4819_REG_47, (6u << 12) | (AF << 8) | (1u << 6));
}

void BK4819_SetRegValue(RegisterSpec s, uint16_t v) {
  uint16_t reg = BK4819_ReadRegister(s.num);
  reg &= ~(s.mask << s.offset);
  BK4819_WriteRegister(s.num, reg | (v << s.offset));
}

void BK4819_RX_TurnOn(void)
{
	// DSP Voltage Setting = 1
	// ANA LDO = 2.7v
	// VCO LDO = 2.7v
	// RF LDO  = 2.7v
	// PLL LDO = 2.7v
	// ANA LDO bypass
	// VCO LDO bypass
	// RF LDO  bypass
	// PLL LDO bypass
	// Reserved bit is 1 instead of 0
	// Enable  DSP
	// Enable  XTAL
	// Enable  Band Gap
	//
	BK4819_WriteRegister(BK4819_REG_37, 0x1F0F);  // 0001111100001111

	// Turn off everything
	BK4819_WriteRegister(BK4819_REG_30, 0);


	BK4819_WriteRegister(BK4819_REG_30, 
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_DISABLE_UNKNOWN |
		BK4819_REG_30_ENABLE_RX_LINK |
		BK4819_REG_30_ENABLE_AF_DAC |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO |
		BK4819_REG_30_DISABLE_PA_GAIN |
		BK4819_REG_30_DISABLE_MIC_ADC |
		BK4819_REG_30_DISABLE_TX_DSP |
		BK4819_REG_30_ENABLE_RX_DSP );
}

void BK4819_PickRXFilterPathBasedOnFrequency(uint32_t Frequency)
{
	if (Frequency < 28000000)
	{	// VHF
		BK4819_ToggleGpioOut(BK4819_GPIO4_PIN32_VHF_LNA, true);
		BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31_UHF_LNA, false);
	}
	else
	if (Frequency == 0xFFFFFFFF)
	{	// OFF
		BK4819_ToggleGpioOut(BK4819_GPIO4_PIN32_VHF_LNA, false);
		BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31_UHF_LNA, false);
	}
	else
	{	// UHF
		BK4819_ToggleGpioOut(BK4819_GPIO4_PIN32_VHF_LNA, false);
		BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31_UHF_LNA, true);
	}
}

void BK4819_DisableScramble(void)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value & ~(1u << 1));
}

void BK4819_EnableScramble(uint8_t Type)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value | (1u << 1));

	BK4819_WriteRegister(BK4819_REG_71, 0x68DC + (Type * 1032));   // 0110 1000 1101 1100
}

bool BK4819_CompanderEnabled(void)
{
	return (BK4819_ReadRegister(BK4819_REG_31) & (1u << 3)) ? true : false;
}

void BK4819_SetCompander(const unsigned int mode)
{
	// mode 0 .. OFF
	// mode 1 .. TX
	// mode 2 .. RX
	// mode 3 .. TX and RX

	const uint16_t r31 = BK4819_ReadRegister(BK4819_REG_31);

	if (mode == 0)
	{	// disable
		BK4819_WriteRegister(BK4819_REG_31, r31 & ~(1u << 3));
		return;
	}

	// REG_29
	//
	// <15:14> 10 Compress (AF Tx) Ratio
	//         00 = Disable
	//         01 = 1.333:1
	//         10 = 2:1
	//         11 = 4:1
	//
	// <13:7>  86 Compress (AF Tx) 0 dB point (dB)
	//
	// <6:0>   64 Compress (AF Tx) noise point (dB)
	//
	const uint16_t compress_ratio    = (mode == 1 || mode >= 3) ? 2 : 0;  // 2:1
	const uint16_t compress_0dB      = 86;
	const uint16_t compress_noise_dB = 64;
//	AB40  10 1010110 1000000
	BK4819_WriteRegister(BK4819_REG_29, // (BK4819_ReadRegister(BK4819_REG_29) & ~(3u << 14)) | (compress_ratio << 14));
		(compress_ratio    << 14) |
		(compress_0dB      <<  7) |
		(compress_noise_dB <<  0));

	// REG_28
	//
	// <15:14> 01 Expander (AF Rx) Ratio
	//         00 = Disable
	//         01 = 1:2
	//         10 = 1:3
	//         11 = 1:4
	//
	// <13:7>  86 Expander (AF Rx) 0 dB point (dB)
	//
	// <6:0>   56 Expander (AF Rx) noise point (dB)
	//
	const uint16_t expand_ratio    = (mode >= 2) ? 1 : 0;   // 1:2
	const uint16_t expand_0dB      = 86;
	const uint16_t expand_noise_dB = 56;
//	6B38  01 1010110 0111000
	BK4819_WriteRegister(BK4819_REG_28, // (BK4819_ReadRegister(BK4819_REG_28) & ~(3u << 14)) | (expand_ratio << 14));
		(expand_ratio    << 14) |
		(expand_0dB      <<  7) |
		(expand_noise_dB <<  0));

	// enable
	BK4819_WriteRegister(BK4819_REG_31, r31 | (1u << 3));
}



void BK4819_PlayTone(uint16_t Frequency, bool bTuningGainSwitch)
{
	uint16_t ToneConfig = BK4819_REG_70_ENABLE_TONE1;

	BK4819_EnterTxMute();
	BK4819_SetAF(BK4819_AF_BEEP);

	if (bTuningGainSwitch == 0)
		ToneConfig |=  96u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN;
	else
		ToneConfig |= 28u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN;
	BK4819_WriteRegister(BK4819_REG_70, ToneConfig);

	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_30, BK4819_REG_30_ENABLE_AF_DAC | BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_TX_DSP);

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));
}

// level 0 ~ 127
void BK4819_PlaySingleTone(const unsigned int tone_Hz, const unsigned int delay, const unsigned int level, const bool play_speaker)
{
	BK4819_EnterTxMute();
	
	if (play_speaker)
	{
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		BK4819_SetAF(BK4819_AF_BEEP);
	}
	else
		BK4819_SetAF(BK4819_AF_MUTE);

	
	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | ((level & 0x7f) << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_EnableTXLink();
	SYSTEM_DelayMs(50);

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(tone_Hz));

	BK4819_ExitTxMute();
	SYSTEM_DelayMs(delay);
	BK4819_EnterTxMute();

	if (play_speaker)
	{
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		BK4819_SetAF(BK4819_AF_MUTE);
	}
	
	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
	BK4819_ExitTxMute();
}

void BK4819_EnterTxMute(void)
{
	BK4819_WriteRegister(BK4819_REG_50, 0xBB20);
}

void BK4819_ExitTxMute(void)
{
	BK4819_WriteRegister(BK4819_REG_50, 0x3B20);
}

void BK4819_Sleep(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_37, 0x1D00);
}

void BK4819_TurnsOffTones_TurnsOnRX(void)
{
	BK4819_WriteRegister(BK4819_REG_70, 0);
	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_ExitTxMute();

	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_ENABLE_RX_LINK   |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
		BK4819_REG_30_ENABLE_RX_DSP);
}

void BK4819_ResetFSK(void)
{
	BK4819_WriteRegister(BK4819_REG_3F, 0x0000);        // Disable interrupts
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);        // Sync length 4 bytes, 7 byte preamble

	SYSTEM_DelayMs(30);

	BK4819_Idle();
}

void BK4819_FskClearFifo(void){
	const uint16_t fsk_reg59 = BK4819_ReadRegister(BK4819_REG_59);
	BK4819_WriteRegister(BK4819_REG_59, (1u << 15) | (1u << 14) | fsk_reg59);
}

void BK4819_FskEnableRx(void){
	const uint16_t fsk_reg59 = BK4819_ReadRegister(BK4819_REG_59);
	BK4819_WriteRegister(BK4819_REG_59, (1u << 12) | fsk_reg59);
}

void BK4819_FskEnableTx(void){
	const uint16_t fsk_reg59 = BK4819_ReadRegister(BK4819_REG_59);
	BK4819_WriteRegister(BK4819_REG_59, (1u << 11) | fsk_reg59);
}

void BK4819_Idle(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
}

void BK4819_ExitBypass(void)
{
	BK4819_SetAF(BK4819_AF_MUTE);

	// REG_7E
	//
	// <15>    0 AGC fix mode
	//         1 = fix
	//         0 = auto
	//
	// <14:12> 3 AGC fix index
	//         3 ( 3) = max
	//         2 ( 2)
	//         1 ( 1)
	//         0 ( 0)
	//         7 (-1)
	//         6 (-2)
	//         5 (-3)
	//         4 (-4) = min
	//
	// <11:6>  0 ???
	//
	// <5:3>   5 DC filter band width for Tx (MIC In)
	//         0 ~ 7
	//         0 = bypass DC filter
	//
	// <2:0>   6 DC filter band width for Rx (I.F In)
	//         0 ~ 7
	//         0 = bypass DC filter
	//

	uint16_t regVal = BK4819_ReadRegister(BK4819_REG_7E);

	// 0x302E / 0 011 000000 101 110
	BK4819_WriteRegister(BK4819_REG_7E, (regVal & ~(0b111 << 3)) 

		| (5u <<  3)       // 5  DC Filter band width for Tx (MIC In)

	);
}

void BK4819_PrepareTransmit(bool muteMic)
{
	BK4819_ExitBypass();
	BK4819_ExitTxMute();
	BK4819_TxOn_Beep();

	if(muteMic)
	{
		BK4819_MuteMic();
	}
}

void BK4819_MuteMic(void)
{
	const uint16_t reg30 = BK4819_ReadRegister(BK4819_REG_30);
	BK4819_WriteRegister(BK4819_REG_30, reg30 & ~(1u << 2));
}

void BK4819_TxOn_Beep(void)
{
	BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
	BK4819_WriteRegister(BK4819_REG_52, 0x028F);
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
}

void BK4819_ExitSubAu(void)
{
	// REG_51
	//
	// <15>  0
	//       1 = Enable TxCTCSS/CDCSS
	//       0 = Disable
	//
	// <14>  0
	//       1 = GPIO0Input for CDCSS
	//       0 = Normal Mode (for BK4819 v3)
	//
	// <13>  0
	//       1 = Transmit negative CDCSS code
	//       0 = Transmit positive CDCSS code
	//
	// <12>  0 CTCSS/CDCSS mode selection
	//       1 = CTCSS
	//       0 = CDCSS
	//
	// <11>  0 CDCSS 24/23bit selection
	//       1 = 24bit
	//       0 = 23bit
	//
	// <10>  0 1050HzDetectionMode
	//       1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	//
	// <9>   0 Auto CDCSS Bw Mode
	//       1 = Disable
	//       0 = Enable
	//
	// <8>   0 Auto CTCSS Bw Mode
	//       0 = Enable
	//       1 = Disable
	//
	// <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//       0   = min
	//       127 = max
	//
	BK4819_WriteRegister(BK4819_REG_51, 0x0000);
}

void BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable(void)
{
	if (gRxIdleMode)
	{
		BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
		BK4819_RX_TurnOn();
	}
}

void BK4819_EnableTXLink(void)
{
	BK4819_WriteRegister(BK4819_REG_30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_ENABLE_UNKNOWN   |
		BK4819_REG_30_DISABLE_RX_LINK  |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
		BK4819_REG_30_ENABLE_PA_GAIN   |
		BK4819_REG_30_DISABLE_MIC_ADC  |
		BK4819_REG_30_ENABLE_TX_DSP    |
		BK4819_REG_30_DISABLE_RX_DSP);
}

void BK4819_TransmitTone(bool bLocalLoopback, uint32_t Frequency)
{
	BK4819_EnterTxMute();

	// REG_70
	//
	// <15>   0 Enable TONE1
	//        1 = Enable
	//        0 = Disable
	//
	// <14:8> 0 TONE1 tuning gain
	//        0 ~ 127
	//
	// <7>    0 Enable TONE2
	//        1 = Enable
	//        0 = Disable
	//
	// <6:0>  0 TONE2/FSK amplitude
	//        0 ~ 127
	//
	// set the tone amplitude
	//
	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_MASK_ENABLE_TONE1 | (66u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));

	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

	BK4819_EnableTXLink();

	SYSTEM_DelayMs(50);

	BK4819_ExitTxMute();
}

void BK4819_GenTail(uint8_t Tail)
{
	// REG_52
	//
	// <15>    0 Enable 120/180/240 degree shift CTCSS or 134.4Hz Tail when CDCSS mode
	//         0 = Normal
	//         1 = Enable
	//
	// <14:13> 0 CTCSS tail mode selection (only valid when REG_52 <15> = 1)
	//         00 = for 134.4Hz CTCSS Tail when CDCSS mode
	//         01 = CTCSS0 120° phase shift
	//         10 = CTCSS0 180° phase shift
	//         11 = CTCSS0 240° phase shift
	//
	// <12>    0 CTCSSDetectionThreshold Mode
	//         1 = ~0.1%
	//         0 =  0.1 Hz
	//
	// <11:6>  0x0A CTCSS found detect threshold
	//
	// <5:0>   0x0F CTCSS lost  detect threshold

	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail 55Hz Rx detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz)*20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	
	switch (Tail)
	{
		case 0: // 134.4Hz CTCSS Tail
			BK4819_WriteRegister(BK4819_REG_52, 0x828F);   // 1 00 0 001010 001111
			break;
		case 1: // 120° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xA28F);   // 1 01 0 001010 001111
			break;
		case 2: // 180° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xC28F);   // 1 10 0 001010 001111
			break;
		case 3: // 240° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xE28F);   // 1 11 0 001010 001111
			break;
		case 4: // SQL_TONE freq
			BK4819_SetCTCSSFrequency(CTCSS_Options[gEeprom.SQL_TONE]);
			BK4819_SetTailDetection(CTCSS_Options[gEeprom.SQL_TONE]);
			break;
	}
}

void BK4819_EnableCDCSS(void)
{
	BK4819_GenTail(0);     // CTC134
	BK4819_WriteRegister(BK4819_REG_51, 0x804A);
}

void BK4819_EnableCTCSS(void)
{
		//BK4819_GenTail(1);     // 120° phase shift
		BK4819_GenTail(2);       // 180° phase shift
		//BK4819_GenTail(3);     // 240° phase shift
		// BK4819_GenTail(4);    // SQL_TONE tone freq

	// REG_51
	//
	// <15>  0
	//       1 = Enable TxCTCSS/CDCSS
	//       0 = Disable
	//
	// <14>  0
	//       1 = GPIO0Input for CDCSS
	//       0 = Normal Mode (for BK4819 v3)
	//
	// <13>  0
	//       1 = Transmit negative CDCSS code
	//       0 = Transmit positive CDCSS code
	//
	// <12>  0 CTCSS/CDCSS mode selection
	//       1 = CTCSS
	//       0 = CDCSS
	//
	// <11>  0 CDCSS 24/23bit selection
	//       1 = 24bit
	//       0 = 23bit
	//
	// <10>  0 1050HzDetectionMode
	//       1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	//
	// <9>   0 Auto CDCSS Bw Mode
	//       1 = Disable
	//       0 = Enable
	//
	// <8>   0 Auto CTCSS Bw Mode
	//       0 = Enable
	//       1 = Disable
	//
	// <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//       0   = min
	//       127 = max

	BK4819_WriteRegister(BK4819_REG_51, 0x904A); // 1 0 0 1 0 0 0 0 0 1001010
}

uint16_t BK4819_GetRSSI(void)
{
	return BK4819_ReadRegister(BK4819_REG_67) & 0x01FF;
}

uint8_t  BK4819_GetGlitchIndicator(void)
{
	return BK4819_ReadRegister(BK4819_REG_63) & 0x00FF;
}

uint8_t  BK4819_GetExNoiseIndicator(void)
{
	return BK4819_ReadRegister(BK4819_REG_65) & 0x007F;
}

uint16_t BK4819_GetVoiceAmplitudeOut(void)
{
	return BK4819_ReadRegister(BK4819_REG_64);
}

uint8_t BK4819_GetAfTxRx(void)
{
	return BK4819_ReadRegister(BK4819_REG_6F) & 0x003F;
}

bool BK4819_GetFrequencyScanResult(uint32_t *pFrequency)
{
	const uint16_t High     = BK4819_ReadRegister(BK4819_REG_0D);
	const bool     Finished = (High & 0x8000) == 0;
	if (Finished)
	{
		const uint16_t Low = BK4819_ReadRegister(BK4819_REG_0E);
		*pFrequency = (uint32_t)((High & 0x7FF) << 16) | Low;
	}
	return Finished;
}

BK4819_CssScanResult_t BK4819_GetCxCSSScanResult(uint32_t *pCdcssFreq, uint16_t *pCtcssFreq)
{
	uint16_t Low;
	uint16_t High = BK4819_ReadRegister(BK4819_REG_69); //Read CDCSS REG

	if ((High & 0x8000) == 0) //CDCSS Scan Indicator 1=Busy; 0=Found.
	{
		Low         = BK4819_ReadRegister(BK4819_REG_6A);
		*pCdcssFreq = ((High & 0xFFF) << 12) | (Low & 0xFFF);
		return BK4819_CSS_RESULT_CDCSS;
	}

	Low = BK4819_ReadRegister(BK4819_REG_68); //Read CTCSS reg

	if ((Low & 0x8000) == 0) //CTCSS Scan Indicator 1=Busy; 0=Found.
	{
		*pCtcssFreq = ((Low & 0x1FFF) * 4843) / 10000;
		return BK4819_CSS_RESULT_CTCSS;
	}

	return BK4819_CSS_RESULT_NOT_FOUND;
}

void BK4819_DisableFrequencyScan(void)
{
	// REG_32
	//
	// <15:14> 0 frequency scan time
	//         0 = 0.2 sec
	//         1 = 0.4 sec
	//         2 = 0.8 sec
	//         3 = 1.6 sec
	//
	// <13:1>  ???
	//
	// <0>     0 frequency scan enable
	//         1 = enable
	//         0 = disable
	//
	BK4819_WriteRegister(BK4819_REG_32, // 0x0244);    // 00 0000100100010 0
		(  0u << 14) |          // 0 frequency scan Time
		(290u <<  1) |          // ???
		(  0u <<  0));          // 0 frequency scan enable
}
#ifdef ENABLE_SCANNER
void BK4819_EnableFrequencyScan(void)
{
	// REG_32
	//
	// <15:14> 0 frequency scan time
	//         0 = 0.2 sec
	//         1 = 0.4 sec
	//         2 = 0.8 sec
	//         3 = 1.6 sec
	//
	// <13:1>  ???
	//
	// <0>     0 frequency scan enable
	//         1 = enable
	//         0 = disable
	//
	BK4819_WriteRegister(BK4819_REG_32, 0x0245);   // 00 0000100100010 1
	/*	(  0u << 14) |          // 0 frequency scan time
		REG_32<15:14> 0b00
			FrequencyScan Time.
			00=0.2 Sec; 01=0.4 Sec; 10=0.8 Sec; 11=1.6 Sec
		(290u <<  1) |          // ???
		(  1u <<  0));          // 1 frequency scan enable

		Frequency Scan

    RF Frequency Scanning:
    Use RF_FreqScan() to obtain the RF frequency at the LNAIN pin (requires a relatively strong signal, amplitude > -40 dBm).
        Returns 1 to indicate failure
        Returns 0 to indicate success
        The detected frequency is written to the global variables FRQ_HI16 and FRQ_LO16.

    After detecting the frequency, set the receive frequency to that frequency point.
    Use RF_CtcDcsScan() to detect the CTCSS frequency or CDCSS code.

        Returns 0 if no signal is detected
        Returns 1 if CTCSS is detected, and the frequency is stored in the global variable CtC_FREQ
        Returns 2 if 23-bit CDCSS is detected
        Returns 3 if 24-bit CDCSS is detected

    For both 23-bit and 24-bit CDCSS, the code is written to the global variables DCS_HI12 and DCS_LO12.*/
}
#endif
void BK4819_SetScanFrequency(uint32_t Frequency)
{
	BK4819_SetFrequency(Frequency);

	// REG_51
	//
	// <15>  0
	//       1 = Enable TxCTCSS/CDCSS
	//       0 = Disable
	//
	// <14>  0
	//       1 = GPIO0Input for CDCSS
	//       0 = Normal Mode (for BK4819 v3)
	//
	// <13>  0
	//       1 = Transmit negative CDCSS code
	//       0 = Transmit positive CDCSS code
	//
	// <12>  0 CTCSS/CDCSS mode selection
	//       1 = CTCSS
	//       0 = CDCSS
	//
	// <11>  0 CDCSS 24/23bit selection
	//       1 = 24bit
	//       0 = 23bit
	//
	// <10>  0 1050HzDetectionMode
	//       1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	//
	// <9>   0 Auto CDCSS Bw Mode
	//       1 = Disable
	//       0 = Enable
	//
	// <8>   0 Auto CTCSS Bw Mode
	//       0 = Enable
	//       1 = Disable
	//
	// <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//       0   = min
	//       127 = max
	//
	BK4819_WriteRegister(BK4819_REG_51,
		BK4819_REG_51_DISABLE_CxCSS         |
		BK4819_REG_51_GPIO6_PIN2_NORMAL     |
		BK4819_REG_51_TX_CDCSS_POSITIVE     |
		BK4819_REG_51_MODE_CDCSS            |
		BK4819_REG_51_CDCSS_23_BIT          |
		BK4819_REG_51_1050HZ_NO_DETECTION   |
		BK4819_REG_51_AUTO_CDCSS_BW_DISABLE |
		BK4819_REG_51_AUTO_CTCSS_BW_DISABLE);

	BK4819_RX_TurnOn();
}

void BK4819_Disable(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0);
}

void BK4819_StopScan(void)
{
	BK4819_DisableFrequencyScan();
	BK4819_Disable();
}

uint8_t BK4819_GetCDCSSCodeType(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0C) >> 14) & 3u;
}

uint8_t BK4819_GetCTCShift(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0C) >> 12) & 3u;
}

uint8_t BK4819_GetCTCType(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0C) >> 10) & 3u;
}

void BK4819_SendFSKData(uint16_t *pData)
{
	unsigned int i;
	uint8_t Timeout = 200;

	SYSTEM_DelayMs(20);

	BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_TX_FINISHED);
	BK4819_WriteRegister(BK4819_REG_59, 0x8068);
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);

	for (i = 0; i < 36; i++)
		BK4819_WriteRegister(BK4819_REG_5F, pData[i]);

	SYSTEM_DelayMs(20);

	BK4819_WriteRegister(BK4819_REG_59, 0x2868);

	while (Timeout-- && (BK4819_ReadRegister(BK4819_REG_0C) & 1u) == 0)
		SYSTEM_DelayMs(5);

	BK4819_WriteRegister(BK4819_REG_02, 0);

	SYSTEM_DelayMs(20);

	BK4819_ResetFSK();
}

void BK4819_PrepareFSKReceive(void)
{
	BK4819_ResetFSK();
	BK4819_WriteRegister(BK4819_REG_02, 0);
	BK4819_WriteRegister(BK4819_REG_3F, 0);
	BK4819_RX_TurnOn();
	BK4819_WriteRegister(BK4819_REG_3F, 0 | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);

	// Clear RX FIFO
	// FSK Preamble Length 7 bytes
	// FSK SyncLength Selection
	BK4819_WriteRegister(BK4819_REG_59, 0x4068);

	// Enable FSK Scramble
	// Enable FSK RX
	// FSK Preamble Length 7 bytes
	// FSK SyncLength Selection
	BK4819_WriteRegister(BK4819_REG_59, 0x3068);
}

//###########################################################################################

/* void play_morse_element(uint32_t freq, uint32_t duration_ms) {
    BK4819_WriteRegister(BK4819_REG_71, scale_freq(freq));
    BK4819_ExitTxMute();
    SYSTEM_DelayMs(duration_ms);
    BK4819_EnterTxMute();
    SYSTEM_DelayMs(50); // Small gap after each element
}

void play_morse_letter(const char *pattern) {
    while (*pattern) {
        if (*pattern == '.') {
            play_morse_element(400, 50);
        } else if (*pattern == '-') {
            play_morse_element(400, 150);
        }
        pattern++;
    }
    SYSTEM_DelayMs(100); // Adjust for letter gap
}

void send_robzyl_morse() {
    play_morse_letter(".-."); 	//R
    play_morse_letter("---"); 	//O
    play_morse_letter("-...");  //B
    play_morse_letter("--..");	//Z
    play_morse_letter("-.--");	//Y
    play_morse_letter(".-..");	//L
}  */

//###########################################################################################


void play_note(uint32_t freq, uint32_t duration) {
    BK4819_WriteRegister(BK4819_REG_71, scale_freq(freq));
    BK4819_ExitTxMute();
    SYSTEM_DelayMs(duration);
    BK4819_EnterTxMute();
    //SYSTEM_DelayMs(10);
}

void play_mario_intro() {
    play_note(660, 100);
    play_note(660, 100);
    play_note(0, 100);
    play_note(660, 100);
    play_note(0, 100);
    play_note(523, 100);
    play_note(660, 100);
    play_note(0, 100);
    play_note(784, 100);
    play_note(0, 300);
    play_note(392, 100);
}


//###########################################################################################
 void roger_beep_r2d2(void) {
	play_note(1318, 80);
    play_note(1568, 60);
    play_note(2093, 50);
    play_note(0,    20);
    play_note(1760, 70);
    play_note(1174, 40);
    play_note(2349, 60);
    play_note(0,    30);
    play_note(2637, 90);
	play_note(1760, 80);
    play_note(2093, 60);
    play_note(2637, 90);
    play_note(2349, 70);
    play_note(3136, 100);
    play_note(0,    40);
	play_note(2489, 60);   
    play_note(2637, 80);   
    play_note(2093, 70);   
    play_note(1760, 100);
} 
//###########################################################################################

 void roger_beep_r2d2_rnd(void){
// R2-D2 Style Acknowledgment Beep
play_note(1046, 50);  // C6
play_note(1318, 50);  // E6
play_note(1568, 70);  // G6
play_note(0, 30);     // Micro-pause for chirp effect
play_note(1760, 40);  // A6
play_note(1568, 40);  // G6
play_note(1318, 100); // E6 (ending the phrase)
} 
//###########################################################################################

void roger_beep_3(void) {
    for (uint16_t i=2000;i>500;i-=50) play_note(i, 5); 
	for (uint16_t i=2000;i>500;i-=50) play_note(i, 5); 
	for (uint16_t i=500;i<2550;i+=50) play_note(i, 7);
}
//###########################################################################################




//###########################################################################################

typedef struct {
    int freq;
    int dur;
} Note;

void send_pacman(void) {
    static const Note pacmanTheme[] = {
        // --- Mesure 1 ---
        {494,150}, {988,150}, {740,150}, {622,150}, {988,75}, {740,225}, {622,300},
        {523,150}, {1047,150}, {880,150}, {659,150}, {1047,75}, {880,225}, {659,300},

        // --- Mesure 2 ---
        {494,150}, {988,150}, {740,150}, {622,150}, {988,75}, {740,225}, {622,300},
        {622,75}, {659,75}, {698,75}, {698,75}, {740,75}, {784,75}, {784,75}, {831,75},
        {880,150}, {988,300}
    };

    for (size_t i = 0; i < sizeof(pacmanTheme)/sizeof(pacmanTheme[0]); i++) {
        play_note(pacmanTheme[i].freq, pacmanTheme[i].dur);
    }
}




void BK4819_PlayRoger(uint8_t song)
{
	BK4819_EnterTxMute();
	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | (66u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_EnableTXLink();
	SYSTEM_DelayMs(50);
switch (song)
{
	case 1:
		play_mario_intro();
	break;

	case 2:
		send_pacman();
	break;

	case 3:
		roger_beep_r2d2();
	break;

	case 4:
		roger_beep_3();	
	break;
	
	case 5:
		roger_beep_r2d2_rnd();	
	break;

default:
	break;
}
	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);   // 1 1 0000 0 1 1111 1 1 1 0
}






void BK4819_Enable_AfDac_DiscMode_TxDsp(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0x0302);
}

void BK4819_GetVoxAmp(uint16_t *pResult)
{
	*pResult = BK4819_ReadRegister(BK4819_REG_64) & 0x7FFF;
}

void BK4819_SetScrambleFrequencyControlWord(uint32_t Frequency)
{
	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));
}
