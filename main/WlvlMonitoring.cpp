/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 16.01.2024

  Copyright (C) 2024-now Authors and www.dsp-crowd.com

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "WlvlMonitoring.h"
#include "SystemCommanding.h"
#include "EspWifiConnecting.h"
#include "LibTime.h"

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StMain) \
		gen(StFancyStart) \
		gen(StFancyDoneWait) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 1
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

using namespace std;

#define LOG_LVL	0

bool WlvlMonitoring::fancyCreateReq = false;
bool WlvlMonitoring::fancyDrivenByPool = false;
uint32_t WlvlMonitoring::cntFancy = 1;

WlvlMonitoring::WlvlMonitoring()
	: Processing("WlvlMonitoring")
	, mStartMs(0)
	, mDiffLoopMs(0)
	, mLastTimeLoopMs(0)
	, mFancyDiffMs(0)
	, mpLed(NULL)
	, mpPool(NULL)
	, mOkWifiOld(false)
{
	mState = StStart;
}

/* member functions */

Success WlvlMonitoring::process()
{
	uint32_t curTimeMs = millis();
	uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
	list<FancyCalculating *>::iterator iter;
	TaskHandle_t pTask;
	UBaseType_t prio;
#if 0
	dStateTrace;
#endif
	mDiffLoopMs = curTimeMs - mLastTimeLoopMs;
	mLastTimeLoopMs = curTimeMs;

	switch (mState)
	{
	case StStart:

		procInfLog("Starting main process");

		pTask = xTaskGetCurrentTaskHandle();
		prio = uxTaskPriorityGet(pTask);

		procDbgLog(LOG_LVL, "Priority of main process is %u", prio);

		mpLed = EspLedPulsing::create();
		if (!mpLed)
			return procErrLog(-1, "could not create process");

		mpLed->pinSet(GPIO_NUM_2);
		mpLed->paramSet(50, 200, 1, 800);

		mpLed->procTreeDisplaySet(false);
		start(mpLed);

		cmdReg(
			"procAdd",
			&WlvlMonitoring::cmdProcAdd,
			"", "",
			"Add dummy process");

		mpPool = ThreadPooling::create();
		if (!mpPool)
		{
			procErrLog(-1, "could not create process");

			mState = StMain;
			break;
		}
#if 1
		mpPool->workerCntSet(2);
		mpPool->driverCreateFctSet(poolDriverCreate);
#else
		mpPool->workerCntSet(1);
#endif
		start(mpPool);

		mState = StMain;

		break;
	case StMain:

		wifiCheck();

		if (!fancyCreateReq)
			break;
		fancyCreateReq = false;

		mState = StFancyStart;

		break;
	case StFancyStart:

		procDbgLog(LOG_LVL, "creating fancy process");

		for (uint32_t i = 0; i < cntFancy; ++i)
		{
			FancyCalculating *pFancy;

			pFancy = FancyCalculating::create();
			if (!pFancy)
			{
				procErrLog(-1, "could not create process");
				break;
			}

			pFancy->paramSet(100, 40);
			mLstFancy.push_back(pFancy);

			if (!fancyDrivenByPool or !mpPool)
			{
				start(pFancy);
				continue;
			}

			start(pFancy, DrivenByExternalDriver);
			ThreadPooling::procAdd(pFancy);
		}

		mStartMs = millis();
		mState = StFancyDoneWait;

		break;
	case StFancyDoneWait:

		mFancyDiffMs = diffMs;

		iter = mLstFancy.begin();
		while (iter != mLstFancy.end())
		{
			FancyCalculating *pFancy = *iter;

			success = pFancy->success();
			if (success == Pending)
			{
				++iter;
				continue;
			}

			// Consume results ...

			repel(pFancy);
			pFancy = NULL;

			iter = mLstFancy.erase(iter);
		}

		if (mLstFancy.size())
			break;

		mState = StMain;

		break;
	default:
		break;
	}

	return Pending;
}

void WlvlMonitoring::wifiCheck()
{
	bool ok = EspWifiConnecting::ok();

	if (ok == mOkWifiOld)
		return;
	mOkWifiOld = ok;

	if (ok)
		mpLed->paramSet(50, 200, 1, 800);
	else
		mpLed->paramSet(50, 200, 2, 600);
}

/* static functions */

/*
 * Literature
 * - https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/system/freertos.html
 */
void WlvlMonitoring::poolDriverCreate(Processing *pDrv, uint16_t idDrv)
{
	xTaskCreatePinnedToCore(
		cpuBoundDrive,					// function
		!idDrv ? "Primary" : "Secondary",	// name
		4096,		// stack
		pDrv,		// parameter
		1,			// priority
		NULL,		// handle
		idDrv);		// core ID
}

void WlvlMonitoring::cpuBoundDrive(void *arg)
{
	Processing *pDrv = (Processing *)arg;

	dbgLog(LOG_LVL, "entered CPU bound process for driver %p", pDrv);

	while (1)
	{
		pDrv->treeTick();
		this_thread::sleep_for(chrono::milliseconds(2));
	}
}

void WlvlMonitoring::cmdProcAdd(char *pArgs, char *pBuf, char *pBufEnd)
{
	cntFancy = 1;
	fancyDrivenByPool = false;

	if (pArgs)
		cntFancy = strtol(pArgs, NULL, 10);

	if (cntFancy > 20)
	{
		infLog("max 20 tasks allowed");
		cntFancy = 20;
	}

	if (pArgs)
		pArgs = strchr(pArgs, ' ');

	if (pArgs)
		fancyDrivenByPool = strtol(pArgs, NULL, 10);

	dbgLog(LOG_LVL, "requesting fancy process");
	fancyCreateReq = true;

	dInfo("Count: %lu, Driven by %s\n", cntFancy, fancyDrivenByPool ? "thread pool" : "parent");
}

void WlvlMonitoring::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
#endif
	dInfo("Loop duration\t\t%lums\n", mDiffLoopMs);
	dInfo("Fancy duration\t\t%lums\n", mFancyDiffMs);
}

