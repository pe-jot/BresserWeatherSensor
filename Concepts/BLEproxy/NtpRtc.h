#pragma once

#include <RTCZero.h>
#include <ctime>

class NtpRtc
{
	/// @brief Number of fetching retries
	static constexpr auto maxRetries =		10;
	/// @brief Delay in [ms] between retries
	static constexpr auto retryDelayMs =	1000;
	/// @brief Time in [s] to suppress fetching NTP time (Time which we expect RTC to run acceptably stable)
	static constexpr auto minNtpSyncPeriod =	(24 * 60 * 60);
	
public:
	static NtpRtc* instance();
	bool SyncWithNtp(const bool force = false);
	time_t GetTime();
	void PrintTime();

protected:
	RTCZero rtc;
	
private:
	NtpRtc();
	~NtpRtc() = default;
	NtpRtc(const NtpRtc&) = delete;
	NtpRtc& operator=(const NtpRtc&) = delete;
	time_t GetNtpTime();
	time_t mLastNtpSync;
};