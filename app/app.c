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

#include "app/action.h"
#include "app/app.h"
#include "app/fm.h"
#include "app/generic.h"
#include "app/main.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "app/uart.h"
#include "app/spectrum.h"
#include "ARMCM0.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/backlight.h"
#include "driver/bk1080.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "spectrum.h"
#if defined(ENABLE_OVERLAY)
	#include "sram-overlay.h"
#endif
#include "ui/battery.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/menu.h"
#include "ui/status.h"
#include "ui/ui.h"
#include "driver/systick.h"

#ifdef ENABLE_SCREENSHOT
    #include "screenshot.h"
#endif

#include "driver/eeprom.h"
extern bool gBacklightAlwaysOn;  // подсветка всегда включена (F+8)

bool gCurrentTxState = false;

static void ProcessKey(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
static void FlashlightTimeSlice();

static void UpdateRSSI()
{
    int16_t rssi = BK4819_GetRSSI();
    if (gCurrentRSSI == rssi) return;   // no change
    gCurrentRSSI = rssi;
}

static void CheckForIncoming(void)
{
	if (!g_SquelchLost)
		return;          // squelch is closed

	// squelch is open

	if (gCurrentFunction != FUNCTION_INCOMING)
			{
				FUNCTION_Select(FUNCTION_INCOMING);
				//

				UpdateRSSI();
				gUpdateRSSI = true;
			}
			return;

	gRxReceptionMode = RX_MODE_DETECTED;
	if (gCurrentFunction != FUNCTION_INCOMING)
	{
		FUNCTION_Select(FUNCTION_INCOMING);
		UpdateRSSI();
		gUpdateRSSI = true;
	}
}


static void HandleIncoming(void)
{
	bool bFlag;

	if (!g_SquelchLost) {	// squelch is closed

		if (gCurrentFunction != FUNCTION_FOREGROUND) {
			FUNCTION_Select(FUNCTION_FOREGROUND);
			
		}
		return;
	}

	bFlag = (gCurrentCodeType == CODE_TYPE_OFF);

	if (g_CTCSS_Lost && gCurrentCodeType == CODE_TYPE_CONTINUOUS_TONE) {
		bFlag       = true;
		gFoundCTCSS = false;
	}

	if (g_CDCSS_Lost && gCDCSSCodeType == CDCSS_POSITIVE_CODE && (gCurrentCodeType == CODE_TYPE_DIGITAL || gCurrentCodeType == CODE_TYPE_REVERSE_DIGITAL)) {
		gFoundCDCSS = false;
	}
	else if (!bFlag)
		return;

	APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
}

static void HandleReceive(void)
{
	#define END_OF_RX_MODE_SKIP 0
	#define END_OF_RX_MODE_END  1
	#define END_OF_RX_MODE_TTE  2

	uint8_t Mode = END_OF_RX_MODE_SKIP;

	if (gFlagTailNoteEliminationComplete)
	{
		Mode = END_OF_RX_MODE_END;
		goto Skip;
	}

	if (gCurrentCodeType != CODE_TYPE_OFF
		&& ((gFoundCTCSS && gFoundCTCSSCountdown_10ms == 0)
			|| (gFoundCDCSS && gFoundCDCSSCountdown_10ms == 0))
	){
		gFoundCTCSS = false;
		gFoundCDCSS = false;
		Mode        = END_OF_RX_MODE_END;
		goto Skip;
	}

	if (g_SquelchLost)
	{
		if (!gEndOfRxDetectedMaybe){
			switch (gCurrentCodeType)
			{
				case CODE_TYPE_OFF:
					if (gEeprom.SQUELCH_LEVEL)
					{
						if (g_CxCSS_TAIL_Found)
						{
							Mode               = END_OF_RX_MODE_TTE;
							g_CxCSS_TAIL_Found = false;
						}
					}
					break;

				case CODE_TYPE_CONTINUOUS_TONE:
					if (g_CTCSS_Lost)
					{
						gFoundCTCSS = false;
					}
					else
					if (!gFoundCTCSS)
					{
						gFoundCTCSS               = true;
						gFoundCTCSSCountdown_10ms = 100;   // 1 sec
					}

					if (g_CxCSS_TAIL_Found)
					{
						Mode               = END_OF_RX_MODE_TTE;
						g_CxCSS_TAIL_Found = false;
					}
					break;

				case CODE_TYPE_DIGITAL:
				case CODE_TYPE_REVERSE_DIGITAL:
					if (g_CDCSS_Lost && gCDCSSCodeType == CDCSS_POSITIVE_CODE)
					{
						gFoundCDCSS = false;
					}
					else
					if (!gFoundCDCSS)
					{
						gFoundCDCSS               = true;
						gFoundCDCSSCountdown_10ms = 100;   // 1 sec
					}

					if (g_CxCSS_TAIL_Found)
					{
						if (BK4819_GetCTCType() == 1)
							Mode = END_OF_RX_MODE_TTE;

						g_CxCSS_TAIL_Found = false;
					}

					break;
			}
		}
	}
	else
		Mode = END_OF_RX_MODE_END;

	if (!gEndOfRxDetectedMaybe         &&
	     Mode == END_OF_RX_MODE_SKIP   &&
	     gNextTimeslice40ms            &&
	    (gCurrentCodeType == CODE_TYPE_DIGITAL || gCurrentCodeType == CODE_TYPE_REVERSE_DIGITAL) &&
	     BK4819_GetCTCType() == 1)
		Mode = END_OF_RX_MODE_TTE;
	else
		gNextTimeslice40ms = false;

Skip:
	switch (Mode)
	{
		case END_OF_RX_MODE_SKIP:
			break;

		case END_OF_RX_MODE_END:
			RADIO_SetupRegisters(true);
			
			break;

		case END_OF_RX_MODE_TTE:
			AUDIO_AudioPathOff();

			gTailNoteEliminationCountdown_10ms = 20;
			gFlagTailNoteEliminationComplete   = false;
			gEndOfRxDetectedMaybe = true;
			gEnableSpeaker        = false;
			break;
	}
}

static void HandleFunction(void)
{
	switch (gCurrentFunction)
	{
		case FUNCTION_FOREGROUND:
			CheckForIncoming();
			break;

		case FUNCTION_TRANSMIT:
			break;

		case FUNCTION_MONITOR:
			break;

		case FUNCTION_INCOMING:
			HandleIncoming();
			break;

		case FUNCTION_RECEIVE:
			HandleReceive();
			break;

		case FUNCTION_POWER_SAVE:
			if (!gRxIdleMode)
				CheckForIncoming();
			break;

		case FUNCTION_BAND_SCOPE:
			break;
	}
}

void APP_StartListening(FUNCTION_Type_t Function)
{
	const unsigned int chan = 0;
	if (gFmRadioMode)
		BK1080_Init(0, false);


	// clear the other vfo's rssi level (to hide the antenna symbol)
	gVFO_RSSI_bar_level[(chan + 1) & 1u] = 0;

	AUDIO_AudioPathOn();
	//VFO sensitivity test KOLYAN ToggleAFDAC
  	uint32_t Reg = regs_cache[BK4819_REG_30];
  	Reg &= ~(1 << 9);
  	Reg |= (1 << 9);
  	BK4819_WriteRegister(BK4819_REG_30, Reg);
	
	//VFO sensitivity test KOLYAN ToggleAFBit
  	uint32_t reg = regs_cache[BK4819_REG_47];
  	reg &= ~(1 << 8);
  	reg |= 1 << 8;
  	BK4819_WriteRegister(BK4819_REG_47, reg);

 	
	gEnableSpeaker = true;

	if (gSetting_backlight_on_tx_rx >= BACKLIGHT_ON_TR_RX)
		BACKLIGHT_TurnOn();
	gTxVfoIsActive = true; 
	// AF gain - original QS values
	// if (gTxVfo->Modulation != MODULATION_FM){
	// 	BK4819_WriteRegister(BK4819_REG_48, 0xB3A8);
	// }
	// else 
	{
	BK4819_WriteRegister(BK4819_REG_48,
		(11u << 12)                |     // ??? .. 0 to 15, doesn't seem to make any difference
		( 0u << 10)                |     // AF Rx Gain-1
		(gEeprom.VOLUME_GAIN << 4) |     // AF Rx Gain-2
		(gEeprom.DAC_GAIN    << 0));     // AF DAC Gain (after Gain-1 and Gain-2)
	}

		RADIO_SetModulation(gTxVfo->Modulation);  // no need, set it now

	FUNCTION_Select(Function);


	if (Function == FUNCTION_MONITOR || gFmRadioMode)
	{	// squelch is disabled
		if (gScreenToDisplay != DISPLAY_MENU)     // 1of11 .. don't close the menu
			GUI_SelectNextDisplay(DISPLAY_MAIN);
	}
	else
		
	
	BK4819_InitAGC(gTxVfo->Modulation);
	
}

uint32_t APP_SetFrequencyByStep(VFO_Info_t *pInfo, int8_t direction)
{
	uint32_t Frequency = FREQUENCY_RoundToStep(pInfo->freq_config_RX.Frequency + (direction * pInfo->StepFrequency), pInfo->StepFrequency);
	return Frequency;
}

static void CheckRadioInterrupts(void)
{
	
	int safety = 100; // max 100 interruptions
	while ((BK4819_ReadRegister(BK4819_REG_0C) & 1u) && --safety > 0) 
	{	// BK chip interrupt request

		uint16_t interrupt_status_bits;

		// reset the interrupt ?
		BK4819_WriteRegister(BK4819_REG_02, 0);

		// fetch the interrupt status bits
		interrupt_status_bits = BK4819_ReadRegister(BK4819_REG_02);

		// 0 = no phase shift
		// 1 = 120deg phase shift
		// 2 = 180deg phase shift
		// 3 = 240deg phase shift
		const uint8_t ctcss_shift = BK4819_GetCTCShift();
		if (ctcss_shift > 0)
			g_CTCSS_Lost = true;

		if (interrupt_status_bits & BK4819_REG_02_CxCSS_TAIL)
			g_CxCSS_TAIL_Found = true;

		if (interrupt_status_bits & BK4819_REG_02_CDCSS_LOST)
		{
			g_CDCSS_Lost = true;
			gCDCSSCodeType = BK4819_GetCDCSSCodeType();
		}

		if (interrupt_status_bits & BK4819_REG_02_CDCSS_FOUND)
			g_CDCSS_Lost = false;

		if (interrupt_status_bits & BK4819_REG_02_CTCSS_LOST)
			g_CTCSS_Lost = true;

		if (interrupt_status_bits & BK4819_REG_02_CTCSS_FOUND)
			g_CTCSS_Lost = false;

		if (interrupt_status_bits & BK4819_REG_02_SQUELCH_LOST)
		{
			g_SquelchLost = true;
			BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
		}

		if (interrupt_status_bits & BK4819_REG_02_SQUELCH_FOUND)
		{
			g_SquelchLost = false;
			BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
		}

	}
}

void APP_EndTransmission(bool playRoger)
{	// back to RX mode
	RADIO_SendEndOfTransmission(playRoger);
	// send the CTCSS/DCS tail tone - allows the receivers to mute the usual FM squelch tail/crash
	RADIO_EnableCxCSS();
	RADIO_SetupRegisters(false);

	if (gMonitor)
		gFlagReconfigureVfos = true; //turn the monitor back on
}

void APP_Update(void)
{

	if (gCurrentFunction == FUNCTION_TRANSMIT && (gTxTimeoutReached || gSerialConfigCountDown_500ms > 0))
	{	// transmitter timed out or must de-key
		gTxTimeoutReached = false;

		gFlagEndTransmission = true;
		APP_EndTransmission(true);
		RADIO_SetVfoState(VFO_STATE_TIMEOUT);
		//GUI_DisplayScreen();
	}

	if (gReducedService)
		return;

	if (gCurrentFunction != FUNCTION_TRANSMIT)
		HandleFunction();

	if (gFmRadioMode && gFmRadioCountdown_500ms > 0)    // 1of11
		return;


//Robby69 auto start spectrum 
	uint8_t Spectrum_state = 0; //Spectrum Not Active
  	if (!gF_LOCK) EEPROM_ReadBuffer(0x1D00, &Spectrum_state, 1); //Do not autostart if FLOCK BOOT
	if (Spectrum_state >0 && Spectrum_state <5)
		APP_RunSpectrum(Spectrum_state);


	if (!gPttIsPressed && gCurrentFunction != FUNCTION_POWER_SAVE)
				{
					if (gTxVfoIsActive && gScreenToDisplay == DISPLAY_MAIN)
						GUI_SelectNextDisplay(DISPLAY_MAIN);

					gTxVfoIsActive     = false;
					gRxReceptionMode   = RX_MODE_NONE;
				}

		
	if (gSchedulePowerSave)
	{
		if (
			gFmRadioMode                  ||
			gPttIsPressed                     ||
		    gKeyBeingHeld                     ||
			gEeprom.BATTERY_SAVE == 0         ||
		    gCssBackgroundScan                      ||
		    gScreenToDisplay != DISPLAY_MAIN  

			)
		{
			gBatterySaveCountdown_10ms   = battery_save_count_10ms;
		}
		else 

		{
			//if (gCurrentFunction != FUNCTION_POWER_SAVE)
				FUNCTION_Select(FUNCTION_POWER_SAVE);
		}

		gSchedulePowerSave = false;

	}


	if (gPowerSaveCountdownExpired && gCurrentFunction == FUNCTION_POWER_SAVE)

	{	// wake up, enable RX then go back to sleep

		if (gRxIdleMode)
		{
			BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable();

			FUNCTION_Init();

			gPowerSave_10ms = power_save1_10ms; // come back here in a bit
			gRxIdleMode     = false;            // RX is awake
		}
		else
		if ( gCssBackgroundScan || gUpdateRSSI)
		{	// dual watch mode off or scanning or rssi update request

			UpdateRSSI();

			// go back to sleep

			gPowerSave_10ms = gEeprom.BATTERY_SAVE * 20; //TEST KOLYAN
			gRxIdleMode     = true;

			BK4819_Sleep();
			BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);

		}

		gPowerSaveCountdownExpired = false;

	}
}

static void gobacktospectrum(void){
	uint8_t Spectrum_state = 0; //Spectrum Not Active
	EEPROM_ReadBuffer(0x1D00, &Spectrum_state, 1);
	if (Spectrum_state >10 && Spectrum_state <15) //WAS SPECTRUM
		APP_RunSpectrum(Spectrum_state-10);
}

// called every 10ms
static void CheckKeys(void)
{

// -------------------- Toggle PTT ------------------------
//ROBBY69 TOGGLE PTT
// First check if we need to stop transmission due to timeout
if (gCurrentTxState && gTxTimerCountdown_500ms == 0) {
    ProcessKey(KEY_PTT, false, false);  // Turn off TX
    gCurrentTxState = false;
    gPttIsPressed = false;  // Reset PTT state as well
	
}

// Then handle PTT button logic
if (!GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT) && gSerialConfigCountDown_500ms == 0)
{   // PTT button is pressed
    if (!gPttIsPressed)
    {   // Only act on the initial press, not while holding
        if (++gPttDebounceCounter >= 3)    // 30ms debounce
        {   
            gPttIsPressed = true;
            gPttDebounceCounter = 0;
            boot_counter_10ms = 0;
		if (Ptt_Toggle_Mode) {
                // Toggle between KEY_PTT on/off
                if (gCurrentTxState) {
                    ProcessKey(KEY_PTT, false, false);  // Turn off TX
                    gCurrentTxState = false;
					
					
                } else {
                    ProcessKey(KEY_PTT, true, false);   // Turn on TX
                    gCurrentTxState = true;
                }
            } else {
                // Standard PTT behavior - transmit while pressed
                if (!gCurrentTxState) {
                    ProcessKey(KEY_PTT, true, false);   // Turn on TX
                    gCurrentTxState = true;
                }
            }
        }
    }
    else
    {
        gPttDebounceCounter = 0;  // Reset if already pressed
    }
}
else
{   // PTT button is released
    if (gPttIsPressed) {
        if (!Ptt_Toggle_Mode && gCurrentTxState) {
            // Only turn off TX if in normal PTT mode and we're transmitting
            ProcessKey(KEY_PTT, false, false);
            gCurrentTxState = false;
			gobacktospectrum();
        }
    }
    gPttIsPressed = false;
    gPttDebounceCounter = 0;
}

// --------------------- OTHER KEYS ----------------------------

	// scan the hardware keys
	KEY_Code_t Key = KEYBOARD_Poll();

	if (Key != KEY_INVALID) // any key pressed
		boot_counter_10ms = 0;   // cancel boot screen/beeps if any key pressed

	if (gKeyReading0 != Key) // new key pressed
	{	

		if (gKeyReading0 != KEY_INVALID && Key != KEY_INVALID)
			ProcessKey(gKeyReading1, false, gKeyBeingHeld);  // key pressed without releasing previous key

		gKeyReading0     = Key;
		gDebounceCounter = 0;
		return;
	}

	gDebounceCounter++;

	if (gDebounceCounter == key_debounce_10ms) // debounced new key pressed
	{	
		if (Key == KEY_INVALID) //all non PTT keys released
		{
			if (gKeyReading1 != KEY_INVALID) // some button was pressed before
			{
				ProcessKey(gKeyReading1, false, gKeyBeingHeld); // process last button released event
				gKeyReading1 = KEY_INVALID;
			}
		}
		else // process new key pressed
		{
			gKeyReading1 = Key;
			ProcessKey(Key, true, false);
		}

		gKeyBeingHeld = false;
		return;
	}

	if (gDebounceCounter < key_repeat_delay_10ms || Key == KEY_INVALID) // the button is not held long enough for repeat yet, or not really pressed
		return;

	if (gDebounceCounter == key_repeat_delay_10ms) //initial key repeat with longer delay
	{	
		if (Key != KEY_PTT)
		{
			gKeyBeingHeld = true;
			ProcessKey(Key, true, true); // key held event
		}
	}
	else //subsequent fast key repeats
	{	
		if (Key == KEY_UP || Key == KEY_DOWN) // fast key repeats for up/down buttons
		{
			gKeyBeingHeld = true;
			if ((gDebounceCounter % key_repeat_10ms) == 0)
				ProcessKey(Key, true, true); // key held event
		}

		if (gDebounceCounter < 0xFFFF)
			return;

		gDebounceCounter = key_repeat_delay_10ms+1;
	}
}

void APP_TimeSlice10ms(void)
{
	gFlashLightBlinkCounter++;
	#ifdef ENABLE_UART
	if (UART_IsCommandAvailable())
	{
		__disable_irq();
		UART_HandleCommand();
		__enable_irq();
	}
	#endif
	if (gReducedService)
		return;

	if (gCurrentFunction != FUNCTION_POWER_SAVE || !gRxIdleMode)
		CheckRadioInterrupts();

	if (gCurrentFunction == FUNCTION_TRANSMIT)
	{	// transmitting
			if ((gFlashLightBlinkCounter % (150 / 10)) == 0) // once every 150ms
				UI_DisplayAudioBar();
	}

	static uint8_t DisplayStatusCountdown = 15;
	if (!DisplayStatusCountdown--){
		UI_DisplayStatus();
		GUI_DisplayScreen();
		DisplayStatusCountdown = 15;
		ST7565_BlitFullScreen();
	}

	#ifdef ENABLE_SCREENSHOT
        getScreenShot(false);
    #endif
	
	// Skipping authentic device checks

		if (gFmRadioMode && gFmRadioCountdown_500ms > 0)   // 1of11
			return;


	FlashlightTimeSlice();

	if (gFmRadioMode && gFM_RestoreCountdown_10ms > 0)
	{
		if (--gFM_RestoreCountdown_10ms == 0)
		{	// switch back to FM radio mode
			FM_Start();
			GUI_SelectNextDisplay(DISPLAY_FM);
		}
	}
	SCANNER_TimeSlice10ms();
	CheckKeys();
}

void cancelUserInputModes(void)
{
	if (gWasFKeyPressed || gKeyInputCountdown > 0 || gInputBoxIndex > 0)
	{
		gWasFKeyPressed     = false;
		gInputBoxIndex      = 0;
		gKeyInputCountdown  = 0;
	}
}

// this is called once every 500ms
void APP_TimeSlice500ms(void)
{
	static bool exit_menu = false;

	if (gKeypadLocked > 0)
		if (--gKeypadLocked == 0)
			
	if (gKeyInputCountdown > 0)	--gKeyInputCountdown;

	if (gMenuCountdown > 0)
		{if (--gMenuCountdown == 0) exit_menu = (gScreenToDisplay == DISPLAY_MENU);} // exit menu mode

		if (gFmRadioCountdown_500ms > 0)
		{
			gFmRadioCountdown_500ms--;
			if (gFmRadioMode)           // 1of11
				return;
		}


if (gBacklightCountdown > 0 && !gBacklightAlwaysOn && 
		!gCssBackgroundScan &&
		!(gScreenToDisplay == DISPLAY_MENU && (UI_MENU_GetCurrentMenuId() == MENU_ABR || UI_MENU_GetCurrentMenuId() == MENU_ABR_MAX)) 
		) 
	{	if (--gBacklightCountdown == 0)
				if (gEeprom.BACKLIGHT_TIME < (ARRAY_SIZE(gSubMenu_BACKLIGHT) - 1))
					BACKLIGHT_TurnOff();
	}

	if (gSerialConfigCountDown_500ms > 0)
	{
	}

	if (gReducedService)
	{
		BOARD_ADC_GetBatteryInfo(&gBatteryCurrentVoltage);

		if (gBatteryCalibration[3] < gBatteryCurrentVoltage)
		{
			#ifdef ENABLE_OVERLAY
				overlay_FLASH_RebootToBootloader();
			#else
				NVIC_SystemReset();
			#endif
		}

		return;
	}

	gBatteryCheckCounter++;

	// Skipped authentic device check

	if (gCurrentFunction != FUNCTION_TRANSMIT)
	{

		if ((gBatteryCheckCounter & 1) == 0)
		{
			BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[gBatteryVoltageIndex++]);
			if (gBatteryVoltageIndex > 3)
				gBatteryVoltageIndex = 0;
			//BATTERY_GetReadings(true);
			BATTERY_GetReadings();
		}
	}
	
	// regular display updates (once every 2 sec) - if need be
	
	if (!gCssBackgroundScan)
	{
		{
			if (exit_menu)
			{	exit_menu = false;
				gMenuCountdown = 0;

				if (gEeprom.BACKLIGHT_TIME == 0) // backlight always off
				{
					BACKLIGHT_TurnOff();	// turn the backlight OFF
				}
				gWasFKeyPressed  = false;
				gInputBoxIndex   = 0;
				{
					GUI_DisplayType_t disp = DISPLAY_INVALID;

						if (gFmRadioMode &&
							gCurrentFunction != FUNCTION_RECEIVE &&
							gCurrentFunction != FUNCTION_MONITOR &&
							gCurrentFunction != FUNCTION_TRANSMIT)
						{
							disp = DISPLAY_FM;
						}


					if (disp == DISPLAY_INVALID)
					{
							disp = DISPLAY_MAIN;
					}

					if (disp != DISPLAY_INVALID)
						GUI_SelectNextDisplay(disp);
				}
			}
		}
	}

	if (gCurrentFunction != FUNCTION_POWER_SAVE && gCurrentFunction != FUNCTION_TRANSMIT)
		UpdateRSSI();

	if (!gPttIsPressed && gVFOStateResumeCountdown_500ms > 0)
	{
		if (--gVFOStateResumeCountdown_500ms == 0)
		{
			RADIO_SetVfoState(VFO_STATE_NORMAL);
			if (gCurrentFunction != FUNCTION_RECEIVE  &&
			    gCurrentFunction != FUNCTION_TRANSMIT &&
			    gCurrentFunction != FUNCTION_MONITOR  &&
				gFmRadioMode)
			{	// switch back to FM radio mode
				FM_Start();
				GUI_SelectNextDisplay(DISPLAY_FM);
			}
		}
	}

	BATTERY_TimeSlice500ms();
	SCANNER_TimeSlice500ms();

}

#if defined(ENABLE_TX1750)
	static void ALARM_Off(void)
	{
		gAlarmState = ALARM_STATE_OFF;

		AUDIO_AudioPathOff();
		gEnableSpeaker = false;

		if (gEeprom.ALARM_MODE == ALARM_MODE_TONE)
		{
			RADIO_SendEndOfTransmission(true);
			RADIO_EnableCxCSS();
		}

		SYSTEM_DelayMs(5);

		RADIO_SetupRegisters(true);

		if (gScreenToDisplay != DISPLAY_MENU)     // 1of11 .. don't close the menu
			gRequestDisplayScreen = DISPLAY_MAIN;
	}
#endif



static void ProcessKey(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (Key == KEY_EXIT && !BACKLIGHT_IsOn() && gEeprom.BACKLIGHT_TIME > 0)
	{	// just turn the light on for now so the user can see what's what
		BACKLIGHT_TurnOn();
		return;
	}

	if (gCurrentFunction == FUNCTION_POWER_SAVE)
		FUNCTION_Select(FUNCTION_FOREGROUND);

	gBatterySaveCountdown_10ms = battery_save_count_10ms;

	if (!bKeyPressed) // key released
	{
		if (gFlagSaveVfo)
		{
			SETTINGS_SaveVfoIndices();
			gFlagSaveVfo = false;
		}

		if (gFlagSaveSettings)
		{
			SETTINGS_SaveSettings();
			gFlagSaveSettings = false;
		}

		if (gFlagSaveChannel)
		{
			SETTINGS_SaveChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, gFlagSaveChannel);
			gFlagSaveChannel = false;

			if (gVfoConfigureMode == VFO_CONFIGURE_NONE)
				// gVfoConfigureMode is so as we don't wipe out previously setting this variable elsewhere
				gVfoConfigureMode = VFO_CONFIGURE;
		}
	}
	else // key pressed or held
	{
		const uint8_t s = gSetting_backlight_on_tx_rx;
		const int m = UI_MENU_GetCurrentMenuId();
		if 	(	//not when PTT and the backlight shouldn't turn on on TX
				!(Key == KEY_PTT && s != BACKLIGHT_ON_TR_TX && s != BACKLIGHT_ON_TR_TXRX) 
				// not in the backlight menu
				&& !(gScreenToDisplay == DISPLAY_MENU && ( m == MENU_ABR || m == MENU_ABR_MAX || m == MENU_ABR_MIN))
			) 
		{
			BACKLIGHT_TurnOn();
		}

		if (Key == KEY_EXIT && bKeyHeld)
		{	// exit key held pressed

			// cancel user input
			cancelUserInputModes();
			
			if (gMonitor)
				ACTION_Monitor(); //turn off the monitor
		}

		if (gScreenToDisplay == DISPLAY_MENU)       // 1of11
			gMenuCountdown = menu_timeout_500ms;

	}

	bool lowBatPopup = gLowBattery && !gLowBatteryConfirmed &&  gScreenToDisplay == DISPLAY_MAIN;

	if ((gEeprom.KEY_LOCK || lowBatPopup) && gCurrentFunction != FUNCTION_TRANSMIT && Key != KEY_PTT)
	{	// keyboard is locked or low battery popup

		// close low battery popup
		if(Key == KEY_EXIT && bKeyPressed && lowBatPopup) {
			gLowBatteryConfirmed = true;
			
			return;
		}		

		if (Key == KEY_F)
		{	// function/key-lock key

			if (!bKeyPressed)
				return;

			if (!bKeyHeld)
			{	// keypad is locked, tell the user
				gKeypadLocked  = 4;      // 2 seconds
				
				return;
			}
		}
		else if (Key != KEY_SIDE1 && Key != KEY_SIDE2)
		{
			if (!bKeyPressed || bKeyHeld) return;

			// keypad is locked, tell the user
			gKeypadLocked  = 4;          // 2 seconds
			
			return;
		}
	}

	if (Key <= KEY_9 || Key == KEY_F)
	{
		if (gCssBackgroundScan)
		{	// FREQ/CTCSS/DCS scanning
			if (bKeyPressed && !bKeyHeld)
			return;
		}
	}

	bool bFlag = false;
	if (Key == KEY_PTT)
	{
		if (gPttWasPressed)
		{
			bFlag = bKeyHeld;
			if (!bKeyPressed)
			{
				bFlag          = true;
				gPttWasPressed = false;
			}
		}
	}
	else if (gPttWasReleased)
	{
		//if (bKeyHeld) bFlag = true;
		if (!bKeyPressed)
		{
			bFlag           = true;
			gPttWasReleased = false;
		}
	}

	if (gWasFKeyPressed && (Key == KEY_PTT || Key == KEY_EXIT || Key == KEY_SIDE1 || Key == KEY_SIDE2))
	{	// cancel the F-key
		gWasFKeyPressed = false;
		   
	}

	if (!bFlag)
	{
		if (gCurrentFunction == FUNCTION_TRANSMIT)
		{	// transmitting

#if defined(ENABLE_TX1750)
			if (gAlarmState == ALARM_STATE_OFF)
#endif
			{
				if (Key == KEY_PTT)
				{
					GENERIC_Key_PTT(bKeyPressed);
					goto Skip;
				}

				if (!bKeyPressed || bKeyHeld)
				{
					if (!bKeyPressed)
					{
						AUDIO_AudioPathOff();

						gEnableSpeaker = false;


						if (gCurrentVfo->SCRAMBLING_TYPE == 0 || !gSetting_ScrambleEnable)
							BK4819_DisableScramble();
						else
							BK4819_EnableScramble(gCurrentVfo->SCRAMBLING_TYPE - 1);
					}
				}
				else
				{BK4819_DisableScramble();}
			}
#if defined(ENABLE_TX1750)
				else
				if ((!bKeyHeld && bKeyPressed) || (gAlarmState == ALARM_STATE_TX1750 && bKeyHeld && !bKeyPressed))
				{
					ALARM_Off();

					FUNCTION_Select(FUNCTION_FOREGROUND);

					if (Key == KEY_PTT)
						gPttWasPressed  = true;
					else
					if (!bKeyHeld)
						gPttWasReleased = true;
				}
#endif
		}
		else
		{
			switch (gScreenToDisplay)
			{
				case DISPLAY_MAIN:
					if ((Key == KEY_SIDE1 || Key == KEY_SIDE2) )
						{
							ACTION_Handle(Key, bKeyPressed, bKeyHeld);
						}
					else
						MAIN_ProcessKeys(Key, bKeyPressed, bKeyHeld);

					break;

					case DISPLAY_FM:
						FM_ProcessKeys(Key, bKeyPressed, bKeyHeld);
						break;

				case DISPLAY_MENU:
					MENU_ProcessKeys(Key, bKeyPressed, bKeyHeld);
					break;
				case DISPLAY_SCANNER:
					SCANNER_ProcessKeys(Key, bKeyPressed, bKeyHeld);
					break;

				case DISPLAY_INVALID:
				default:
					
					break;
			}
		}
	}

Skip:
	
	if (gFlagAcceptSetting)
	{
		gMenuCountdown = menu_timeout_500ms;

		MENU_AcceptSetting();

		gFlagRefreshSetting = true;
		gFlagAcceptSetting  = false;
	}

	if (gRequestSaveSettings)
	{
		if (!bKeyHeld)
			SETTINGS_SaveSettings();
		else
			gFlagSaveSettings = 1;

		gRequestSaveSettings = false;
		        
	}

	if (gRequestSaveVFO)
	{
		if (!bKeyHeld)
			SETTINGS_SaveVfoIndices();
		else
			gFlagSaveVfo = true;
		gRequestSaveVFO = false;
	}

	if (gRequestSaveChannel > 0)
	{
		if (!bKeyHeld)
		{
			SETTINGS_SaveChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, gRequestSaveChannel);

				if (gVfoConfigureMode == VFO_CONFIGURE_NONE)  // 'if' is so as we don't wipe out previously setting this variable elsewhere
					gVfoConfigureMode = VFO_CONFIGURE;
		}
		else
		{
			gFlagSaveChannel = gRequestSaveChannel;

			if (gRequestDisplayScreen == DISPLAY_INVALID)
				gRequestDisplayScreen = DISPLAY_MAIN;
		}

		gRequestSaveChannel = 0;
	}

	if (gVfoConfigureMode != VFO_CONFIGURE_NONE)
	{
		RADIO_ConfigureChannel(gVfoConfigureMode);
		
		if (gRequestDisplayScreen == DISPLAY_INVALID)
			gRequestDisplayScreen = DISPLAY_MAIN;

		gFlagReconfigureVfos = true;
		gVfoConfigureMode    = VFO_CONFIGURE_NONE;
		gFlagResetVfos       = false;
	}

	if (gFlagReconfigureVfos)
	{
		RADIO_SelectVfos();

		RADIO_SetupRegisters(true);

		gVFO_RSSI_bar_level[0]      = 0;
		gVFO_RSSI_bar_level[1]      = 0;

		gFlagReconfigureVfos        = false;

		if (gMonitor)
			ACTION_Monitor();   // 1of11
	}

	if (gFlagRefreshSetting)
	{
		gFlagRefreshSetting = false;
		gMenuCountdown      = menu_timeout_500ms;

		MENU_ShowCurrentSetting();
	}

	if (gFlagPrepareTX)
	{
		RADIO_PrepareTX();
		gFlagPrepareTX = false;
	}

	GUI_SelectNextDisplay(gRequestDisplayScreen);
	gRequestDisplayScreen = DISPLAY_INVALID;

	
}

static void FlashlightTimeSlice()
{
		if (gFlashLightState == FLASHLIGHT_BLINK && (gFlashLightBlinkCounter & 15u) == 0)
		GPIO_FlipBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
	else if(gFlashLightState == FLASHLIGHT_SOS) {
		const uint16_t u = 15;
		static uint8_t c;
		static uint16_t next;

		if(gFlashLightBlinkCounter - next > 7*u) {
			c = 0;
			next = gFlashLightBlinkCounter + 1;
		}
		else if(gFlashLightBlinkCounter == next) {
			if(c==0) {
				GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
			}
			else
				GPIO_FlipBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);

			if(c >= 18) {
				next = gFlashLightBlinkCounter + 7*u;
				c = 0;
			}
			else if(c==7 || c==9 || c==11)
			 	next = gFlashLightBlinkCounter + 3*u;
			else
				next = gFlashLightBlinkCounter + u;

			c++;
		}
	}
}