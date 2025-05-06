#pragma once

#include <WiFiNINA.h>

#define APRS_SERVER_NAME					"srvr.aprs-is.net"
#define APRS_SERVER_PORT					8080
#define APRS_LATITUDE						"4809.30N"
#define ARPS_LONGITUDE						"01400.30E"
#define USER_COMMENT						" test"

class AprsWebClient
{
	/// @brief Number of connection retries
	static constexpr auto maxRetries =		10;
	/// @brief Delay in [ms] between retries
	static constexpr auto retryDelayMs =	1000;
	/// @brief Time in [s] to suppress sending APRS weather packets
	static constexpr auto minAprsSendingInterval =	(5 * 60);
	
public:
	bool SendWeatherReportPacket(unsigned char humidity, const short temperature, unsigned char battery);

private:
	void GenerateWeatherReportPacket(const time_t now, unsigned char humidity, const short temperature, unsigned char battery);
	void UpdateContentLengthHeader();
	bool Connect();
	short ReadStatusCode(const unsigned short bytesRead);
	
	static constexpr auto cAprsPacketSize = sizeof("MYCALL-13>APRS,TCPIP*:@ddHHMMz0000.00N/00000.00E_c...s...g...t000h00 Bat: 000%\n") + sizeof(USER_COMMENT); // A typical weather report packet
	static constexpr auto cMaxContentHeaderSize = sizeof("Content-Length: 1234\r\n");
	static constexpr auto cReadBufferSize = 100; // Don't need a lot for only parsing the HTTP status
	
	char mAprsPacket[cAprsPacketSize];
	char mHttpContentLengthHeader[cMaxContentHeaderSize];
	uint8_t mReadBuffer[cReadBufferSize];
	time_t mLastSendTime = 0;
	WiFiClient mClient;
};
