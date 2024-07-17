/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
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

#include "driver/eeprom.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "settings.h"
#include "misc.h"
#include "ui/helper.h"
#include "ui/welcome.h"
#include "ui/status.h"
#include "version.h"
#include "bitmaps.h"

void UI_DisplayReleaseKeys(void)
{
	memset(gStatusLine,  0, sizeof(gStatusLine));
#ifdef ENABLE_FEAT_F4HWN
		ST7565_ContrastAndInv();
#endif
	UI_DisplayClear();

	UI_PrintString("RELEASE", 0, 127, 1, 10);
	UI_PrintString("ALL KEYS", 0, 127, 3, 10);

	ST7565_BlitStatusLine();  // blank status line
	ST7565_BlitFullScreen();
}

void UI_DisplayWelcome(void)
{
	char WelcomeString0[16];
	char WelcomeString1[16];

	memset(gStatusLine,  0, sizeof(gStatusLine));

#ifdef ENABLE_FEAT_F4HWN
		ST7565_ContrastAndInv();
#endif
	UI_DisplayClear();

#ifdef ENABLE_FEAT_F4HWN
	ST7565_BlitStatusLine();
	ST7565_BlitFullScreen();
	
	if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_SOUND) {
//		ST7565_FillScreen(0x00);
#else
	if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_FULL_SCREEN) {
//		ST7565_FillScreen(0xFF);
#endif
	} else {
		memset(WelcomeString0, 0, sizeof(WelcomeString0));
		memset(WelcomeString1, 0, sizeof(WelcomeString1));

		sprintf(WelcomeString1, "%u.%03uV %u%%",
				gBatteryVoltageAverage / 1000,
				gBatteryVoltageAverage % 1000,
				gBatteryPercent);

		if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_VOLTAGE)
		{
			strcpy(WelcomeString0, "VOLTAGE");
		}
		else if(gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_ALL)
		{
			EEPROM_ReadBuffer(0x0EB0, WelcomeString0, 16);
		}
		else if(gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_MESSAGE)
		{
			EEPROM_ReadBuffer(0x0EB0, WelcomeString0, 16);
			EEPROM_ReadBuffer(0x0EC0, WelcomeString1, 16);

			// Given the above, we should never have an empty WelcomString1
			//if(strlen(WelcomeString1) == 0)
			//{
			//	strcpy(WelcomeString1, "BIENVENUE");
			//}
		}

		UI_PrintString(WelcomeString0, 0, 127, 0, 10);
		UI_PrintString(WelcomeString1, 0, 127, 2, 10);

#ifdef ENABLE_FEAT_F4HWN
		UI_PrintStringSmallNormal(Version, 0, 128, 4);

		for (uint8_t i = 0; i < 128; i++)
		{
			gFrameBuffer[3][i] ^= 0x80;
		}

		for (uint8_t i = 8; i < 120; i++)
		{
			gFrameBuffer[4][i] ^= 0xFF;
		}

		#ifdef ENABLE_SPECTRUM
			#ifdef ENABLE_FMRADIO
					UI_PrintStringSmallNormal(Based, 0, 127, 5);
					UI_PrintStringSmallNormal(Credits, 0, 127, 6);
			#else
					UI_PrintStringSmallNormal("Bandscope  ", 0, 127, 5);
					memcpy(gFrameBuffer[5] + 95, BITMAP_Ready, sizeof(BITMAP_Ready));
					UI_PrintStringSmallNormal("Broadcast  ", 0, 127, 6);
			#endif
		#else
			UI_PrintStringSmallNormal("Bandscope  ", 0, 127, 5);
			UI_PrintStringSmallNormal("Broadcast  ", 0, 127, 6);
			memcpy(gFrameBuffer[6] + 95, BITMAP_Ready, sizeof(BITMAP_Ready));
		#endif
#else
		UI_PrintStringSmallNormal(Version, 0, 127, 6);
#endif

		//ST7565_BlitStatusLine();  // blank status line : I think it's useless
		ST7565_BlitFullScreen();
	}
}
