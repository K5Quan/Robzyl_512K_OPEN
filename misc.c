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

#include "misc.h"
#include "settings.h"

const uint8_t     fm_radio_countdown_500ms         =  2000 / 500;  // 2 seconds
const uint16_t    fm_play_countdown_scan_10ms      =   100 / 10;   // 100ms
const uint16_t    fm_play_countdown_noscan_10ms    =  1200 / 10;   // 1.2 seconds
const uint16_t    fm_restore_countdown_10ms        =  5000 / 10;   // 5 seconds

const uint8_t     vfo_state_resume_countdown_500ms =  2500 / 500;  // 2.5 seconds

const uint8_t     menu_timeout_500ms               =  10000 / 500;  // 10 s
const uint16_t    menu_timeout_long_500ms          =  30000 / 500;  // 30 s

const uint8_t     key_input_timeout_500ms          =  4;  // 2 seconds

const uint16_t    key_repeat_delay_10ms            =   400 / 10;   // 400ms
const uint16_t    key_repeat_10ms                  =    80 / 10;   // 80ms .. MUST be less than 'key_repeat_delay'
const uint16_t    key_debounce_10ms                =    20 / 10;   // 20ms
const uint8_t     scan_delay_10ms                  =   210 / 10;   // 210ms

const uint16_t    battery_save_count_10ms          = 10000 / 10;   // 10 seconds

const uint16_t    power_save1_10ms                 =   100 / 10;   // 100ms
const uint16_t    power_save2_10ms                 =   200 / 10;   // 200ms


const uint32_t    gDefaultAesKey[4]                = {0x4AA5CC60, 0x0312CC5F, 0xFFD2DABB, 0x6BBA7F92};

const uint8_t     gMicGain_dB2[5]                  = {31,24,16,8};

uint8_t           gSetting_F_LOCK;
bool              gSetting_ScrambleEnable;

uint8_t           gSetting_backlight_on_tx_rx;


uint8_t           gSetting_battery_text;

bool 	          gMonitor = 0;

uint32_t          gCustomAesKey[4];
bool              bHasCustomAesKey;
uint32_t          gChallenge[4];
uint8_t           gTryCount;

uint16_t          gEEPROM_RSSI_CALIB[7][4];

uint16_t          gEEPROM_1F8A;
uint16_t          gEEPROM_1F8C;

ChannelAttributes_t gMR_ChannelAttributes[FREQ_CHANNEL_LAST + 1];
ChannelFrequencyAttributes gMR_ChannelFrequencyAttributes[MR_CHANNEL_LAST +1];
volatile uint16_t gBatterySaveCountdown_10ms = battery_save_count_10ms;

volatile bool     gPowerSaveCountdownExpired;
volatile bool     gSchedulePowerSave;

volatile bool     gScheduleDualWatch = true;

volatile uint16_t gDualWatchCountdown_10ms;
bool              gDualWatchActive           = false;

volatile uint8_t  gSerialConfigCountDown_500ms;

volatile bool     gNextTimeslice_500ms;
volatile bool     gNextTimeslice_1s;
volatile bool     gNextTimeslice_vfo_nums;
volatile bool     gNextTimeslice_display;

volatile uint16_t gTxTimerCountdown_500ms;
volatile bool     gTxTimeoutReached;

volatile uint16_t gTailNoteEliminationCountdown_10ms;

volatile uint8_t    gVFOStateResumeCountdown_500ms;

bool              gEnableSpeaker;
uint8_t           gKeyInputCountdown = 0;
bool              bIsInLockScreen;
uint8_t           gFoundCTCSS;
uint8_t           gFoundCDCSS;
bool              gEndOfRxDetectedMaybe;

int16_t           gVFO_RSSI[2];
uint8_t           gVFO_RSSI_bar_level[2];

uint8_t           gReducedService;
uint8_t           gBatteryVoltageIndex;
bool     		  gCssBackgroundScan;

volatile bool     gScheduleScanListen = true;
volatile uint16_t gScanPauseDelayIn_10ms;

bool              gUpdateRSSI;
#if defined(ENABLE_TX1750)
	AlarmState_t  gAlarmState;
#endif

#ifdef ENABLE_SCREENSHOT
    volatile uint8_t  gUART_LockScreenshot = 0; // lock screenshot if Chirp is used
#endif
uint16_t          gMenuCountdown;
bool              gPttWasReleased;
bool              gPttWasPressed;
uint8_t           gKeypadLocked;
bool              gFlagReconfigureVfos;
uint8_t           gVfoConfigureMode;
bool              gFlagResetVfos;
bool              gRequestSaveVFO;
uint16_t           gRequestSaveChannel;
bool              gRequestSaveSettings;
bool              gFlagPrepareTX;

bool              gFlagAcceptSetting;
bool              gFlagRefreshSetting;

bool              gFlagSaveVfo;
bool              gFlagSaveSettings;
bool              gFlagSaveChannel;
bool              g_CDCSS_Lost;
uint8_t           gCDCSSCodeType;
bool              g_CTCSS_Lost;
bool              g_CxCSS_TAIL_Found;

bool              g_SquelchLost;
uint8_t           gFlashLightState;
bool			  Ptt_Toggle_Mode = false;
volatile uint16_t gFlashLightBlinkCounter;
bool              gFlagEndTransmission;
uint16_t           gNextMrChannel;
ReceptionMode_t   gRxReceptionMode;

bool              gTxVfoIsActive;
bool              gKeyBeingHeld;
bool              gPttIsPressed;
uint8_t           gPttDebounceCounter;
uint8_t           gMenuListCount;
uint8_t           gFSKWriteIndex;
bool              gF_LOCK = false;
uint8_t           gShowChPrefix;
volatile bool     gNextTimeslice;
volatile uint8_t  gFoundCDCSSCountdown_10ms;
volatile uint8_t  gFoundCTCSSCountdown_10ms;

volatile bool     gNextTimeslice40ms;
volatile bool	  gNextTimeslice_10ms;

volatile bool     gFlagTailNoteEliminationComplete;
volatile bool gScheduleFM;


volatile uint8_t  boot_counter_10ms;

int16_t           gCurrentRSSI = 0;  // now one per VFO

uint8_t           gIsLocked = 0xFF;



int32_t NUMBER_AddWithWraparound(int32_t Base, int32_t Add, int32_t LowerLimit, int32_t UpperLimit)
{
	Base += Add;

	if (Base == 0x7fffffff || Base < LowerLimit)
		return UpperLimit;

	if (Base > UpperLimit)
		return LowerLimit;

	return Base;
}

unsigned long StrToUL(const char * str)
{
	unsigned long ul = 0;
	for(uint8_t i = 0; i < strlen(str); i++){
		char c = str[i];
		if(c < '0' || c > '9')
			break;
		ul = ul * 10 + (uint8_t)(c-'0');
	}
	return ul;
}

// Checks if given val is in arr
bool IsValueInArray(int val, const int *arr, const int size) {
	int length = size / sizeof(int);
    for(int i = 0; i < length; i++) {
        if(arr[i] == val)
            return true;
    }
    return false;
}

sLevelAttributes GetSLevelAttributes(const int16_t rssi, const uint32_t frequency)
{
	sLevelAttributes att;
	// S0 .. base level
	int16_t      s0_dBm       = -130;

	// all S1 on max gain, no antenna
	const int8_t dBmCorrTable[7] = {
		-5, // band 1
		-38, // band 2
		-37, // band 3
		-20, // band 4
		-23, // band 5
		-23, // band 6
		-16  // band 7
	};

	// use UHF/VHF S-table for bands above HF
	if(frequency > HF_FREQUENCY)
		s0_dBm-=20;

	att.dBmRssi = Rssi2DBm(rssi)+dBmCorrTable[FREQUENCY_GetBand(frequency)];
	att.sLevel  = MIN(MAX((att.dBmRssi - s0_dBm) / 6, 0), 9);
	att.over    = MIN(MAX(att.dBmRssi - (s0_dBm + 9*6), 0), 99);
	//TODO: calculate based on the current squelch setting
	att.overSquelch = att.sLevel > 5;

	return att;
}

int Rssi2DBm(const uint16_t rssi)
{
	return (rssi >> 1) - 160;
}