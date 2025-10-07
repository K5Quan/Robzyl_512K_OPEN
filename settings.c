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

#include "driver/eeprom.h"
#include "driver/bk4819.h"
#include "misc.h"
#include "settings.h"
#include "board.h"


EEPROM_Config_t gEeprom;

void SETTINGS_SaveVfoIndices(void)
{
	uint8_t State[8];

	EEPROM_ReadBuffer(0x0E80, State, sizeof(State));

	State[0] = gEeprom.ScreenChannel;
	State[1] = gEeprom.MrChannel;
	State[2] = gEeprom.FreqChannel;
	EEPROM_WriteBuffer(0x0E80, State);
}

void SETTINGS_SaveSettings(void)
{
	uint8_t  State[8];

	State[1] = gEeprom.SQUELCH_LEVEL;
	State[2] = gEeprom.TX_TIMEOUT_TIMER;
	State[3] = false;
	State[4] = gEeprom.KEY_LOCK;
	State[5] = false;
	State[6] = 0;

	State[7] = gEeprom.MIC_SENSITIVITY;
	EEPROM_WriteBuffer(0x0E70, State);

	State[0] = (gEeprom.BACKLIGHT_MIN << 4) + gEeprom.BACKLIGHT_MAX;
	State[1] = gEeprom.CHANNEL_DISPLAY_MODE;
	State[3] = gEeprom.BATTERY_SAVE;
	State[5] = gEeprom.BACKLIGHT_TIME;
	State[7] = gEeprom.VFO_OPEN;
	EEPROM_WriteBuffer(0x0E78, State);

	State[0] |= gEeprom.KEY_M_LONG_PRESS_ACTION << 1;
	State[1] = gEeprom.KEY_1_SHORT_PRESS_ACTION;
	State[2] = gEeprom.KEY_1_LONG_PRESS_ACTION;
	State[3] = gEeprom.KEY_2_SHORT_PRESS_ACTION;
	State[4] = gEeprom.KEY_2_LONG_PRESS_ACTION;
	State[5] = gEeprom.SCAN_RESUME_MODE;
	State[6] = gEeprom.AUTO_KEYPAD_LOCK;
	State[7] = gEeprom.POWER_ON_DISPLAY_MODE;
	EEPROM_WriteBuffer(0x0E90, State);

	// 0x0E98..0x0E9F
	memset(State, 0xFF, sizeof(State));
	EEPROM_ReadBuffer(0x0E98, State, 8);
	#ifdef ENABLE_PWRON_PASSWORD
		memcpy(&State[0], &gEeprom.POWER_ON_PASSWORD, 4);
			#endif
	memcpy(&State[4], &gEeprom.RX_OFFSET, 4);
	EEPROM_WriteBuffer(0x0E98, State);

	memset(State, 0xFF, sizeof(State));
	#ifdef ENABLE_PWRON_PASSWORD
		State[2] = gEeprom.PASSWORD_WRONG_ATTEMPTS;
	#endif
	EEPROM_WriteBuffer(0x0EA0, State);

	memset(State, 0xFF, sizeof(State));
	#if defined(ENABLE_TX1750)
		State[0] = gEeprom.ALARM_MODE;
	#else
		State[0] = false;
	#endif
	State[1] = gEeprom.ROGER;
	State[4] = gEeprom.BATTERY_TYPE;
	State[5] = gEeprom.SQL_TONE;
	EEPROM_WriteBuffer(0x0EA8, State);
	memset(State, 0xFF, sizeof(State));
	State[0]  = gSetting_F_LOCK;
	State[6]  = gSetting_ScrambleEnable;
	State[7] = (State[7] & ~(3u << 2)) | ((gSetting_battery_text & 3u) << 2);
	State[7] = (State[7] & ~(3u << 6)) | ((gSetting_backlight_on_tx_rx & 3u) << 6);

	EEPROM_WriteBuffer(0x0F40, State);

		//0x0E88..0x0E8F
		memset(State, 0xFF, sizeof(State));
		memcpy(&State[0], &gEeprom.FM_FrequencyPlaying, 2);
		EEPROM_WriteBuffer(0x0E88, State);


}

void SETTINGS_SaveChannel(uint16_t Channel, const VFO_Info_t *pVFO, uint8_t Mode)
{

	{
		uint16_t OffsetVFO = 0x2000 + Channel * 16;

		if (!IS_MR_CHANNEL(Channel))
		{	// it's a VFO, not a Channel
			OffsetVFO  = 0x0C80;
			OffsetVFO += (Channel - FREQ_CHANNEL_FIRST) * 32;
		}

		if (Mode >= 2 || !IS_MR_CHANNEL(Channel))
		{	// copy VFO to a Channel

			uint8_t State[8];

			((uint32_t *)State)[0] = pVFO->freq_config_RX.Frequency;
			((uint32_t *)State)[1] = pVFO->TX_OFFSET_FREQUENCY;
			EEPROM_WriteBuffer(OffsetVFO + 0, State);

			State[0] =  pVFO->freq_config_RX.Code;
			State[1] =  pVFO->freq_config_TX.Code;
			State[2] = (pVFO->freq_config_TX.CodeType << 4) | pVFO->freq_config_RX.CodeType;
			State[3] = (pVFO->Modulation << 4) | pVFO->TX_OFFSET_FREQUENCY_DIRECTION;
			State[4] = 0
				| (pVFO->BUSY_CHANNEL_LOCK << 4)
				| (pVFO->OUTPUT_POWER      << 2)
				| ((pVFO->CHANNEL_BANDWIDTH != BK4819_FILTER_BW_WIDE) << 1)
				| (pVFO->FrequencyReverse  << 0);
			if(pVFO->CHANNEL_BANDWIDTH != BK4819_FILTER_BW_WIDE)
				State[4] |= ((pVFO->CHANNEL_BANDWIDTH - 1) << 5);

			State[6] =  pVFO->STEP_SETTING;
			State[7] =  pVFO->SCRAMBLING_TYPE;
			EEPROM_WriteBuffer(OffsetVFO + 8, State);

			SETTINGS_UpdateChannel(Channel, pVFO, true);

			if (IS_MR_CHANNEL(Channel))
			{	// it's a memory Channel

				#ifndef ENABLE_KEEP_MEM_NAME
					// clear/reset the Channel name
					SETTINGS_SaveChannelName(Channel, "");
				#else
					if (Mode >= 3) {
						SETTINGS_SaveChannelName(Channel, pVFO->Name);
					}
				#endif
			}
		}
	}
}


void SETTINGS_SaveBatteryCalibration(const uint16_t * batteryCalibration)
{
	uint16_t buf[4];
	EEPROM_WriteBuffer(0x1F40, batteryCalibration);
	EEPROM_ReadBuffer( 0x1F48, buf, sizeof(buf));
	buf[0] = batteryCalibration[4];
	buf[1] = batteryCalibration[5];
	EEPROM_WriteBuffer(0x1F48, buf);
}

void SETTINGS_SaveChannelName(uint16_t Channel, const char * name)
{
	uint16_t offset = Channel * 16;
	uint8_t  buf[16];
	memset(&buf, 0x00, sizeof(buf));
	memcpy(buf, name, MIN(strlen(name),10u));
	EEPROM_WriteBuffer(0x5E80 + offset, buf); //1000 Channels
	EEPROM_WriteBuffer(0x5E88 + offset, buf + 8);
}


void SETTINGS_FetchChannelName(char *s, const uint16_t Channel)
{
	int i;

	if (s == NULL)
		return;

	memset(s, 0, 11);  // 's' had better be large enough !

	if (Channel == 0xFFFF)
		return;

	if (!RADIO_CheckValidChannel(Channel, false, 0))
		return;


	EEPROM_ReadBuffer(0x5E80 + (Channel * 16), s + 0, 8);
	EEPROM_ReadBuffer(0x5E88 + (Channel * 16), s + 8, 2);

	for (i = 0; i < 10; i++)
		if (s[i] < 32 || s[i] > 127)
			break;                // invalid char

	s[i--] = 0;                   // null term

	while (i >= 0 && s[i] == 32)  // trim trailing spaces
		s[i--] = 0;               // null term
}

void SETTINGS_UpdateChannel(uint16_t Channel, const VFO_Info_t *pVFO, bool keep)
{
	{
		uint8_t  state[8];
		ChannelAttributes_t  att = {
			.band = 0xf,
			.scanlist = 0,
			};        // default attributes

		uint16_t offset = 0x2000 + (Channel & ~7u);
		EEPROM_ReadBuffer(offset, state, sizeof(state));

		if (keep) {
			att.band = pVFO->Band;
			att.scanlist = pVFO->SCANLIST;
			if (state[Channel & 7u] == att.__val)
				return; // no change in the attributes
		}

		state[Channel & 7u] = att.__val;
		EEPROM_WriteBuffer(offset, state);

		gMR_ChannelAttributes[Channel] = att;

		if (IS_MR_CHANNEL(Channel)) {	// it's a memory Channel
			if (!keep) {
				// clear/reset the Channel name
				SETTINGS_SaveChannelName(Channel, "");
			}
		}
	}
}

void SETTINGS_SetVfoFrequency(uint32_t frequency) {
	const uint8_t Vfo = 0;
	// clamp the frequency entered to some valid value
	if (frequency < RX_freq_min())
	{
		frequency = RX_freq_min();
	}
	else
	if (frequency >= BX4819_band1.upper && frequency < BX4819_band2.lower)
	{
		const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
		frequency = (frequency < center) ? BX4819_band1.upper : BX4819_band2.lower;
	}
	else
	if (frequency > frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper)
	{
		frequency = frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper;
	}

	{
		const FREQUENCY_Band_t band = FREQUENCY_GetBand(frequency);

		if (gTxVfo->Band != band)
		{
			gTxVfo->Band               = band;
			gEeprom.ScreenChannel = band + FREQ_CHANNEL_FIRST;
			gEeprom.FreqChannel   = band + FREQ_CHANNEL_FIRST;

			SETTINGS_SaveVfoIndices();

			RADIO_ConfigureChannel(Vfo, VFO_CONFIGURE_RELOAD);
		}

		// Autoset stepFrequency based on step setting
		gTxVfo->StepFrequency = gStepFrequencyTable[gTxVfo->STEP_SETTING];

		frequency = FREQUENCY_RoundToStep(frequency, gTxVfo->StepFrequency);

		if (frequency >= BX4819_band1.upper && frequency < BX4819_band2.lower)
		{	// clamp the frequency to the limit
			const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
			frequency = (frequency < center) ? BX4819_band1.upper - gTxVfo->StepFrequency : BX4819_band2.lower;
		}

		gTxVfo->freq_config_RX.Frequency = frequency;
	}
}