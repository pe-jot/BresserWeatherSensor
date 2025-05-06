#include "AprsWebClient.h"
#include "deviceconfig.h"
#include "credentials.h"
#include "WiFiManager.h"
#include "NtpRtc.h"
#include <ctime>
#include <cstring>

typedef enum {
	eReadingPrefix,
	eReadingStatusCode
} tHttpParsingState;

typedef enum {
	HTTP_OK = 200,
	HTTP_NO_CONTENT = 204
} tHttpStatusCode;

static constexpr const char* cAprsHeader =	"user " APRS_CALLSIGN " pass " STRINGIFY(APRS_PASSCODE) " vers " SOFTWARE_NAME "\n";
static constexpr const char* cHttpHeader =
	"POST / HTTP/1.1\r\n"
	"Host: " APRS_SERVER_NAME ":" STRINGIFY(APRS_SERVER_PORT) "\r\n"
	"Content-Type: application/octet-stream\r\n"
	"Accept-Type: text/plain\r\n"
	"Connection: Close\r\n"
	"User-Agent: ArduinoIoT/1.0\r\n";

void AprsWebClient::GenerateWeatherReportPacket(const time_t now, unsigned char humidity, const short temperature, unsigned char battery)
{
	if (humidity >= 100)
	{
		humidity = 0;
	}
	if (battery > 100)
	{
		battery = 100;
	}
	
	char timestamp[sizeof("ddHHMM")];
	auto utc = gmtime(&now);
	strftime(timestamp, sizeof(timestamp), "%d%H%M", utc);
	
	float fahrenheit = round(static_cast<float>(temperature) * static_cast<float>(0.18) + static_cast<float>(32.0));
	
	snprintf(mAprsPacket, cAprsPacketSize, APRS_CALLSIGN ">APRS,TCPIP*:@%6sz" APRS_LATITUDE "/" ARPS_LONGITUDE "_c...s...g...t%03.0fh%02u Bat: %u%%" USER_COMMENT "\n", timestamp, fahrenheit, humidity, battery);
}

void AprsWebClient::UpdateContentLengthHeader()
{	
	const auto contentLength = strlen(cAprsHeader) + strlen(mAprsPacket);
	snprintf(mHttpContentLengthHeader, cMaxContentHeaderSize, "Content-Length: %hu\r\n", contentLength);
}

bool AprsWebClient::Connect()
{
	if (!WiFiManager::Connect())
	{
		DebugTRACE("Error: not connected to WiFi!\n");
		return false;
	}

	DebugTRACE("Connecting to APRS server...\n");
	auto retries = maxRetries;
	while (!mClient.connect(APRS_SERVER_NAME, APRS_SERVER_PORT) && --retries > 0)
	{
		delay(retryDelayMs);
	}
	
	if (retries == 0)
	{
		DebugTRACE("Error connecting to APRS server!\n");
		WiFiManager::Disconnect();
		return false;
	}
	return true;
}

/// <param name="humidity">[%] valid range: 0..99</param>
/// <param name="temperature">[0.1°C]</param>
/// <param name="battery">[%] (sent as comment)</param>
bool AprsWebClient::SendWeatherReportPacket(unsigned char humidity, const short temperature, unsigned char battery)
{
	if (sizeof(APRS_LATITUDE) != sizeof("0000.00N") || sizeof(ARPS_LONGITUDE) != sizeof("00000.00E"))
	{
		DebugTRACE("APRS configuration error! Check latitude/longitude!\n");
		return false;
	}

	const auto now = NtpRtc::instance()->GetTime();
	if (difftime(now, mLastSendTime) < minAprsSendingInterval)
	{
		DebugTRACE("Do not send APRS packet due to time restriction!\n");
		return false;
	}
	mLastSendTime = now;

	DebugTRACE("Generating weather report packet...\n");
	GenerateWeatherReportPacket(now, humidity, temperature, battery);
	UpdateContentLengthHeader(); // Update mandatory length header before sending
	
	if (!Connect())
	{
		return false;
	}
	
	mClient.print(cHttpHeader);
	mClient.print(mHttpContentLengthHeader);
	mClient.print("\r\n");
	mClient.print(cAprsHeader);
	mClient.print(mAprsPacket);
	mClient.flush();

	DebugTRACE("APRS packet sent.\n");

	uint8_t retries = 100;
	while (!mClient.available() && --retries > 0)
	{
		delay(100);
	}
	
	while(mClient.available())
	{
		auto bytesRead = mClient.read(mReadBuffer, cReadBufferSize);
		auto responseStatus = ReadStatusCode(bytesRead); // #ToDo: Doesn't work :-(

		DebugTRACE("Server response: ");
		DebugPRINTLN(responseStatus);

		if (responseStatus == HTTP_OK || responseStatus == HTTP_NO_CONTENT)
		{
			mClient.stop();
			WiFiManager::Disconnect();
			return true;
		}
	}

	mClient.stop();
	WiFiManager::Disconnect();
	return false;
}

short AprsWebClient::ReadStatusCode(const unsigned short bytesRead)
{
	static constexpr auto statusPrefix = "HTTP/*.* ";

	if (bytesRead > cReadBufferSize)
	{
		return -1;
	}

	short statusCode = -1;
	unsigned short i = 0;
	char c = 0;
	const char* pPrefix = statusPrefix;
	tHttpParsingState state = eReadingPrefix;

	do
	{
		c = mReadBuffer[i];
		switch (state)
		{
		case eReadingPrefix:
			// We haven't reached the status code yet
			if ((*pPrefix == '*') || (*pPrefix == c))
			{
				// This character matches, just move along
				pPrefix++;
				if (*pPrefix == '\0')
				{
					// We've reached the end of the prefix
					state = eReadingStatusCode;
				}
			}
			break;

		case eReadingStatusCode:
			if (isdigit(c))
			{
				// This assumes we won't get more than the 3 digits we want
				statusCode = statusCode * 10 + (c - '0');
			}
			else
			{
				// We've reached the end of the status code
				// We could sanity check it here or double-check for ' ' rather than anything else, but let's be lenient
				return statusCode;
			}
			break;
		}
	} while (c != 0 && c != '\n' && ++i < (bytesRead - 1));

	return -1;
}