#pragma once

class WiFiManager
{
	/// @brief Number of connection retries
	static constexpr auto maxRetries =		30;
	/// @brief Delay in [ms] between connection retries
	static constexpr auto retryDelayMs =	1000;
	
public:
	static bool Connect();
	static void Disconnect();
};