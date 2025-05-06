#include "NtpRtc.h"
#include "deviceconfig.h"
#include "WiFiManager.h"
#include <WiFiNINA.h>

NtpRtc* NtpRtc::instance()
{
	// Stack-based Singleton pattern
	static NtpRtc instance;
	return &instance;
}

NtpRtc::NtpRtc()
	: mLastNtpSync(0)
{
	rtc.begin();
	SyncWithNtp(true);
}

bool NtpRtc::SyncWithNtp(const bool force)
{
	const auto dT = difftime(rtc.getEpoch(), mLastNtpSync);
	if (!force && dT < minNtpSyncPeriod)
	{
		return true;
	}
	const auto ntpTime = GetNtpTime();
	if (ntpTime == 0)
	{
		return false;
	}
	mLastNtpSync = ntpTime;
	rtc.setEpoch(ntpTime);
	return true;
}

time_t NtpRtc::GetNtpTime()
{
	if (!WiFiManager::Connect())
	{
		DebugTRACE("Error: not connected to WiFi!\n");
		return 0;
	}
	
	auto retries = maxRetries;
	time_t newTime = 0;	
	do
	{
		newTime = WiFi.getTime();
		if (newTime != 0)
		{
			WiFiManager::Disconnect();
			return newTime;
		}
		delay(retryDelayMs);
	}
	while (newTime == 0 && --retries > 0);
	
	WiFiManager::Disconnect();
	return 0;
}

time_t NtpRtc::GetTime()
{
	return rtc.getEpoch();
}

void NtpRtc::PrintTime()
{
	DebugTRACE("Time: ");
	const time_t t = GetTime();
	char* cTime = ctime(&t);
	DebugPRINT(cTime); // ctime() seems to add some line break
}
