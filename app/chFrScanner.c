
#include "app/app.h"
#include "app/chFrScanner.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"

int8_t            gScanStateDir;
bool              gScanKeepResult;
bool              gScanPauseMode;

#ifdef ENABLE_SCAN_RANGES
uint32_t          gScanRangeStart;
uint32_t          gScanRangeStop;
#endif

typedef enum {
	SCAN_NEXT_CHAN_SCANLIST1 = 0,
	SCAN_NEXT_CHAN_SCANLIST2,
	SCAN_NEXT_CHAN_SCANLIST3,
	SCAN_NEXT_CHAN_DUAL_WATCH,
	SCAN_NEXT_CHAN_MR,
	SCAN_NEXT_NUM
} scan_next_chan_t;

scan_next_chan_t	currentScanList;
uint32_t            initialFrqOrChan;
uint8_t           	initialCROSS_BAND_RX_TX;

#ifndef ENABLE_FEAT_F4HWN
	uint32_t lastFoundFrqOrChan;
#else
	uint32_t lastFoundFrqOrChan;
	uint32_t lastFoundFrqOrChanOld;
#endif

uint8_t dualscan = 1;
uint8_t dwchan;

static void NextFreqChannel(void);
static void NextMemChannel(void);

void CHFRSCANNER_Start(const bool storeBackupSettings, const int8_t scan_direction)
{
	if (storeBackupSettings) {
		initialCROSS_BAND_RX_TX = gEeprom.CROSS_BAND_RX_TX;
		gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
		gScanKeepResult = false;
	}
	
	RADIO_SelectVfos();

	gNextMrChannel   = gRxVfo->CHANNEL_SAVE;
	currentScanList = SCAN_NEXT_CHAN_SCANLIST1;
	gScanStateDir    = scan_direction;

	if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) {
		dwchan = (gEeprom.RX_VFO + 1) & 1u;
		dwchan = gEeprom.ScreenChannel[dwchan]+1;
		if (!IS_MR_CHANNEL(dwchan))
			dwchan = 0;
	}

	if (IS_MR_CHANNEL(gNextMrChannel))
	{	// channel mode
		if (storeBackupSettings) {
			initialFrqOrChan = gRxVfo->CHANNEL_SAVE;
			lastFoundFrqOrChan = initialFrqOrChan;
		}
		NextMemChannel();
	}
	else
	{	// frequency mode
		if (storeBackupSettings) {
			initialFrqOrChan = gRxVfo->freq_config_RX.Frequency;
			lastFoundFrqOrChan = initialFrqOrChan;
		}
		NextFreqChannel();
	}

#ifdef ENABLE_FEAT_F4HWN
	lastFoundFrqOrChanOld = lastFoundFrqOrChan;
#endif

	gScanPauseDelayIn_10ms = scan_pause_delay_in_2_10ms;
	gScheduleScanListen    = false;
	gRxReceptionMode       = RX_MODE_NONE;
	gScanPauseMode         = false;
}

void CHFRSCANNER_ContinueScanning(void)
{
	if (IS_FREQ_CHANNEL(gNextMrChannel))
	{
		if (gCurrentFunction == FUNCTION_INCOMING)
			APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
		else
			NextFreqChannel();  // switch to next frequency
	}
	else
	{
		if (gCurrentCodeType == CODE_TYPE_OFF && gCurrentFunction == FUNCTION_INCOMING)
			APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
		else
			NextMemChannel();    // switch to next channel
	}
	
	gScanPauseMode      = false;
	gRxReceptionMode    = RX_MODE_NONE;
	gScheduleScanListen = false;
}

void CHFRSCANNER_Found(void)
{
	switch (gEeprom.SCAN_RESUME_MODE)
	{
		case SCAN_RESUME_TO:
			if (!gScanPauseMode)
			{
				gScanPauseDelayIn_10ms = scan_pause_delay_in_1_10ms;
				gScheduleScanListen    = false;
				gScanPauseMode         = true;
			}
			break;

		case SCAN_RESUME_CO:
		case SCAN_RESUME_SE:
			gScanPauseDelayIn_10ms = 0;
			gScheduleScanListen    = false;
			break;
	}

#ifdef ENABLE_FEAT_F4HWN
	lastFoundFrqOrChanOld = lastFoundFrqOrChan;
#endif

	if (IS_MR_CHANNEL(gRxVfo->CHANNEL_SAVE)) { //memory scan
		lastFoundFrqOrChan = gRxVfo->CHANNEL_SAVE;
	}
	else { // frequency scan
		lastFoundFrqOrChan = gRxVfo->freq_config_RX.Frequency;
	}


	gScanKeepResult = true;
}

void CHFRSCANNER_Stop(void)
{
	dwchan = 0;
	if(initialCROSS_BAND_RX_TX != CROSS_BAND_OFF) {
		gEeprom.CROSS_BAND_RX_TX = initialCROSS_BAND_RX_TX;
		initialCROSS_BAND_RX_TX = CROSS_BAND_OFF;
	}
	
	gScanStateDir = SCAN_OFF;

	const uint32_t chFr = gScanKeepResult ? lastFoundFrqOrChan : initialFrqOrChan;
	const bool channelChanged = chFr != initialFrqOrChan;
	if (IS_MR_CHANNEL(gNextMrChannel)) {
		gEeprom.MrChannel[gEeprom.RX_VFO]     = chFr;
		gEeprom.ScreenChannel[gEeprom.RX_VFO] = chFr;
		RADIO_ConfigureChannel(gEeprom.RX_VFO, VFO_CONFIGURE_RELOAD);

		if(channelChanged) {
			SETTINGS_SaveVfoIndices();
			gUpdateStatus = true;
		}
	}
	else {
		gRxVfo->freq_config_RX.Frequency = chFr;
		RADIO_ApplyOffset(gRxVfo);
		RADIO_ConfigureSquelchAndOutputPower(gRxVfo);
		if(channelChanged) {
			SETTINGS_SaveChannel(gRxVfo->CHANNEL_SAVE, gEeprom.RX_VFO, gRxVfo, 1);
		}
	}

	dualscan=1;
	
	RADIO_SetupRegisters(true);
	gUpdateDisplay = true;
}

static void NextFreqChannel(void)
{
#ifdef ENABLE_SCAN_RANGES
	if(gScanRangeStart) {
		gRxVfo->freq_config_RX.Frequency = APP_SetFreqByStepAndLimits(gRxVfo, gScanStateDir, gScanRangeStart, gScanRangeStop);
	}
	else
#endif
		gRxVfo->freq_config_RX.Frequency = APP_SetFrequencyByStep(gRxVfo, gScanStateDir);

	RADIO_ApplyOffset(gRxVfo);
	RADIO_ConfigureSquelchAndOutputPower(gRxVfo);
	RADIO_SetupRegisters(true);

#ifdef ENABLE_FASTER_CHANNEL_SCAN
	gScanPauseDelayIn_10ms = 9;   // 90ms
#else
	gScanPauseDelayIn_10ms = scan_pause_delay_in_6_10ms;
#endif

	gUpdateDisplay     = true;
}

static void NextMemChannel(void)
{
	static unsigned int prev_mr_chan = 0;
	// 0 = 1, 1 = 2, 2 = 3, 3 = 0, 4 = all lists, 5 = all
	// const bool          enabled      = (gEeprom.SCAN_LIST_DEFAULT < 6) ? gEeprom.SCAN_LIST_ENABLED[gEeprom.SCAN_LIST_DEFAULT] : true;
	const bool enabled = true;
	const int chan1 = -1;
	const int chan2 = -2;
	//const int           chan1        = (gEeprom.SCAN_LIST_DEFAULT < 3) ? gEeprom.SCANLIST_PRIORITY_CH1[gEeprom.SCAN_LIST_DEFAULT] : -1;
	//const int           chan2        = (gEeprom.SCAN_LIST_DEFAULT < 3) ? gEeprom.SCANLIST_PRIORITY_CH2[gEeprom.SCAN_LIST_DEFAULT] : -1;
	const unsigned int  prev_chan    = gNextMrChannel;
	unsigned int        chan         = 0;

	if (enabled)
	{
		switch (currentScanList)
		{
			case SCAN_NEXT_CHAN_SCANLIST1:
				prev_mr_chan = gNextMrChannel;
	
				if (chan1 >= 0)
				{
					if (RADIO_CheckValidChannel(chan1, false, gEeprom.SCAN_LIST_DEFAULT))
					{
						currentScanList = SCAN_NEXT_CHAN_SCANLIST1;
						gNextMrChannel   = chan1;
						break;
					}
				}
				[[fallthrough]];
			case SCAN_NEXT_CHAN_SCANLIST2:
				if (chan2 >= 0)
				{
					if (RADIO_CheckValidChannel(chan2, false, gEeprom.SCAN_LIST_DEFAULT))
					{
						currentScanList = SCAN_NEXT_CHAN_SCANLIST2;
						gNextMrChannel   = chan2;
						break;
					}
				}
				[[fallthrough]];
/*			case SCAN_NEXT_CHAN_SCANLIST3:
				if (chan3 >= 0)
				{
					if (RADIO_CheckValidChannel(chan3, false, 0))
					{
						currentScanList = SCAN_NEXT_CHAN_SCANLIST3;
						gNextMrChannel   = chan3;
						break;
					}
				}
				[[fallthrough]];
*/			// this bit doesn't yet work if the other VFO is a frequency
			case SCAN_NEXT_CHAN_DUAL_WATCH:
				// dual watch is enabled - include the other VFO in the scan
				if (dwchan && !gMonitor){
					if (++dualscan%4==0) {
						dualscan=0;
						currentScanList = SCAN_NEXT_CHAN_DUAL_WATCH;
						if (!gMR_ChannelExclude[dwchan-1])
							gNextMrChannel   = dwchan-1;
						break;
					}
				}
				[[fallthrough]];
			default:
			case SCAN_NEXT_CHAN_MR:
				currentScanList = SCAN_NEXT_CHAN_MR;
				gNextMrChannel   = prev_mr_chan;
				chan             = 0xff;
				break;
		}
	}

	if (!enabled || chan == 0xff)
	{
		chan = RADIO_FindNextChannel(gNextMrChannel + gScanStateDir, gScanStateDir, (gEeprom.SCAN_LIST_DEFAULT < 5) ? true : false, gEeprom.SCAN_LIST_DEFAULT);
		if (chan == 0xFF)
		{	// no valid channel found
			chan = MR_CHANNEL_FIRST;
		}
		
		gNextMrChannel = chan;
	}

	if (gNextMrChannel != prev_chan)
	{
		gEeprom.MrChannel[    gEeprom.RX_VFO] = gNextMrChannel;
		gEeprom.ScreenChannel[gEeprom.RX_VFO] = gNextMrChannel;

		RADIO_ConfigureChannel(gEeprom.RX_VFO, VFO_CONFIGURE_RELOAD);
		RADIO_SetupRegisters(true);

//		if (dualscan!=0)
			gUpdateDisplay = true;
	}

#ifdef ENABLE_FASTER_CHANNEL_SCAN
	gScanPauseDelayIn_10ms = 9;  // 90ms .. <= ~60ms it misses signals (squelch response and/or PLL lock time) ?
#else
	gScanPauseDelayIn_10ms = scan_pause_delay_in_3_10ms;
#endif

	if (enabled)
		if (++currentScanList >= SCAN_NEXT_NUM)
			currentScanList = SCAN_NEXT_CHAN_SCANLIST1;  // back round we go
}
