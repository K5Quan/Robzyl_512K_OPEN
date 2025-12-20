/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Modified work 2024 kamilsss655
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

#include "backlight.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/pwmplus.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "settings.h"

extern bool gBacklightAlwaysOn;  // из main.c

// this is decremented once every 500ms
uint16_t gBacklightCountdown = 0;  // ← uint16_t, не int16_t
bool backlightOn;

void BACKLIGHT_InitHardware()
{
	const uint32_t PWM_FREQUENCY_HZ =  6000;
	PWM_PLUS0_CLKSRC |= ((48000000 / 1024 / PWM_FREQUENCY_HZ) << 16);
	PWM_PLUS0_PERIOD = 1023;

	PORTCON_PORTB_SEL0 &= ~(PORTCON_PORTB_SEL0_B6_MASK);
	PORTCON_PORTB_SEL0 |= PORTCON_PORTB_SEL0_B6_BITS_PWMP0_CH0;

	PWM_PLUS0_GEN = PWMPLUS_GEN_CH0_OE_BITS_ENABLE | PWMPLUS_GEN_CH0_OUTINV_BITS_ENABLE;

	PWM_PLUS0_CFG = PWMPLUS_CFG_CNT_REP_BITS_ENABLE | PWMPLUS_CFG_COUNTER_EN_BITS_ENABLE;
}

void BACKLIGHT_TurnOn(void)  // ← исправлена опечатка vvoid → void
{
    backlightOn = true;

    if (gBacklightAlwaysOn) {
        gBacklightCountdown = 0;
        BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MAX);
        return;
    }

    if (gEeprom.BACKLIGHT_TIME == 0) {
        BACKLIGHT_TurnOff();
        return;
    }

    BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MAX);

    switch (gEeprom.BACKLIGHT_TIME)
    {
        default:
        case 1: gBacklightCountdown = 5;   break;
        case 2: gBacklightCountdown = 10;  break;
        case 3: gBacklightCountdown = 20;  break;
        case 4: gBacklightCountdown = 60;  break;
        case 5: gBacklightCountdown = 120; break;
        case 6: gBacklightCountdown = 240; break;
        case 7: gBacklightCountdown = 0;   break;
    }

    gBacklightCountdown *= 2;
}

void BACKLIGHT_TurnOff(void)
{
    if (gBacklightAlwaysOn)
        return;

#ifdef ENABLE_BLMIN_TMP_OFF
    register uint8_t tmp;

    if (gEeprom.BACKLIGHT_MIN_STAT == BLMIN_STAT_ON)
        tmp = gEeprom.BACKLIGHT_MIN;
    else
        tmp = 0;

    BACKLIGHT_SetBrightness(tmp);
#else
    BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MIN);
#endif
    gBacklightCountdown = 0;
    backlightOn = false;
}

bool BACKLIGHT_IsOn()
{
    return backlightOn || gBacklightAlwaysOn;
}

void BACKLIGHT_SetBrightness(uint8_t brightness)
{
	PWM_PLUS0_CH0_COMP = (1 << brightness) - 1;
}