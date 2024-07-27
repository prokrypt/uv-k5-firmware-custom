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

#include "app/chFrScanner.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/scanner.h"
#include "bitmaps.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"

static void convertTime(uint8_t *line, uint8_t type) 
{
	char str[8] = "";

    uint8_t m, s; // Declare variables for seconds, hours, minutes, and seconds
    uint16_t t;

    if(type == 0) // Tx timer
		t = ((gTxTimerCountdown_500ms+1) / 2);
		//t = ((gEeprom.TX_TIMEOUT_TIMER + 1) * 5) - (gTxTimerCountdown_500ms / 2);
    else          // Rx timer
		t = (7199 - gRxTimerCountdown_500ms)/2;

    m = t / 60;
    s = t - (m * 60);

	gStatusLine[14] = 0x00; // Quick fix on display (on scanning I, II, etc.)

    sprintf(str, "%02d:%02d", m, s);
    UI_PrintStringSmallBufferNormal(str, line + 0);

    gUpdateStatus = true;
}

void UI_DisplayStatus()
{
	char str[8] = "";

	gUpdateStatus = false;
	memset(gStatusLine, 0, sizeof(gStatusLine));

	uint8_t     *line = gStatusLine;
	unsigned int x    = 0;
	// **************

	// POWER-SAVE indicator
	if (gCurrentFunction == FUNCTION_POWER_SAVE) {
		memcpy(line + x, gFontPowerSave, sizeof(gFontPowerSave));
	}
	x += 8;
	unsigned int x1 = x;

#ifdef ENABLE_NOAA
	if (gIsNoaaMode) { // NOASS SCAN indicator
		memcpy(line + x, BITMAP_NOAA, sizeof(BITMAP_NOAA));
		x1 = x + sizeof(BITMAP_NOAA);
	}
	x += sizeof(BITMAP_NOAA);
#endif

#ifdef ENABLE_DTMF_CALLING
	if (gSetting_KILLED) {
		memset(line + x, 0xFF, 10);
		x1 = x + 10;
	}
	else
#endif
	{ // SCAN indicator
		if (gScanStateDir != SCAN_OFF || SCANNER_IsScanning()) {
			if (IS_MR_CHANNEL(gNextMrChannel) && !SCANNER_IsScanning()) { // channel mode
				switch(gEeprom.SCAN_LIST_DEFAULT) {
					case 0: 
					case 1:
					case 2:
					case 3:
						memcpy(line + x + 2, &BITMAP_ScanList1230[gEeprom.SCAN_LIST_DEFAULT*7], sizeof(BITMAP_ScanList1230)/4);
						break;
					case 4:
						memcpy(line + x + 0, BITMAP_ScanList5, sizeof(BITMAP_ScanList5));
						break;
					case 5:
						memcpy(line + x + 1, BITMAP_ScanList6, sizeof(BITMAP_ScanList6));
						break;
				}
			}
			else {	// frequency mode
				memcpy(line + x + 1, gFontS, sizeof(gFontS));
				//UI_PrintStringSmallBufferNormal("S", line + x + 1);
			}
			x1 += 10;
		}
	}
	x += 10;  // font character width

	// Only for debug
	// Only for debug
	// Only for debug
	
	bool debug = false;
	if(debug)
	{
		sprintf(str, "%d", gDebug);
		UI_PrintStringSmallBufferNormal(str, line + x + 1);
		x += 16;
	}
	else
	{

	#ifdef ENABLE_VOICE
		// VOICE indicator
		if (gEeprom.VOICE_PROMPT != VOICE_PROMPT_OFF){
			memcpy(line + x, BITMAP_VoicePrompt, sizeof(BITMAP_VoicePrompt));
			x1 = x + sizeof(BITMAP_VoicePrompt);
		}
		x += sizeof(BITMAP_VoicePrompt);
	#endif

		if(!SCANNER_IsScanning()) {
			if(gCurrentFunction == FUNCTION_TRANSMIT)
			{
				convertTime(line, 0);
			}
			else if(FUNCTION_IsRx())
			{
				convertTime(line, 1);
			}
			else
			{
				uint8_t dw = (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) + (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) * 2;
				if(dw == 1 || dw == 3) { // DWR - dual watch + respond
					if(gDualWatchActive)
						memcpy(line + x + (dw==1?0:2), gFontDWR, sizeof(gFontDWR) - (dw==1?0:5));
					else
						memcpy(line + x + 3, gFontHold, sizeof(gFontHold));
				}
				else if(dw == 2) { // XB - crossband
					memcpy(line + x + 2, gFontXB, sizeof(gFontXB));
				}
				else
				{
					memcpy(line + x + 2, gFontMO, sizeof(gFontMO));
				}
			}
		}
		x += sizeof(gFontDWR) + 3;
	}

#ifdef ENABLE_VOX
	// VOX indicator
	if (gEeprom.VOX_SWITCH) {
		memcpy(line + x, gFontVox, sizeof(gFontVox));
		x1 = x + sizeof(gFontVox) + 1;
	}
	x += sizeof(gFontVox) + 3;
#endif

#ifdef ENABLE_FEAT_F4HWN
	// PTT indicator
	memcpy(line + x, &gFontPtt[gSetting_set_ptt_session*2], sizeof(gFontPtt)/2);
	x1 = x + sizeof(gFontPtt)/2 + 1;
	x += sizeof(gFontPtt)/2 + 3;
#endif

	x = MAX(x1, 70u);

	// KEY-LOCK indicator
	if (gEeprom.KEY_LOCK) {
		memcpy(line + x + 1, gFontKeyLock, sizeof(gFontKeyLock));
	}
	else if (gWasFKeyPressed) {
		memcpy(line + x + 1, gFontF, sizeof(gFontF));
		/*
		UI_PrintStringSmallBufferNormal("F", line + x + 1);
		
		for (uint8_t i = 71; i < 79; i++)
		{
			gStatusLine[i] ^= 0x7F;
		}
		*/
	}
	else if (gBackLight)
	{
		memcpy(line + x + 1, gFontLight, sizeof(gFontLight));
	}
	/*
	else if (gChargingWithTypeC)
	{
		memcpy(line + x + 1, BITMAP_USB_C, sizeof(BITMAP_USB_C));
	}
	*/
	
	// Battery
	unsigned int x2 = LCD_WIDTH - sizeof(BITMAP_BatteryLevel1);

	UI_DrawBattery(line + x2, gBatteryDisplayLevel, gLowBatteryBlink);

	switch (gSetting_battery_text) {
		default:
		case 0:
			break;

		case 1:	{	// voltage
			const uint16_t voltage = (gBatteryVoltageAverage <= 9999) ? gBatteryVoltageAverage/10 : 999; // limit to 9.99V
#ifdef ENABLE_FEAT_F4HWN
			sprintf(str, "%u.%02u", voltage / 100, voltage % 100);
#else
			sprintf(str, "%u.%02uV", voltage / 100, voltage % 100);
#endif
			break;
		}

		case 2:		// percentage
			sprintf(str, "%u%%", gBatteryPercent);
			break;
	}

	x2 -= (7 * strlen(str))+1;
	UI_PrintStringSmallBufferNormal(str, line + x2);

	// **************

	ST7565_BlitStatusLine();
}
