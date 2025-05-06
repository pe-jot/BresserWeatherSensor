#include "WiFiManager.h"
#include "credentials.h"
#include "deviceconfig.h"
#include <WiFiNINA.h>

bool WiFiManager::Connect()
{
	wiFiDrv.wifiDriverInit();
	uint8_t retries = maxRetries;
	do
	{
		DebugTRACE("Establishing WiFi connection ...\n");
		if (WiFi.begin(WIFI_SSID, WIFI_PASS) == WL_CONNECTED)
		{
			DebugTRACE("Connected to '" WIFI_SSID "', IP: ");
			DebugPRINTLN(WiFi.localIP());			
			WiFi.setHostname(DEVICE_NAME);
			WiFi.lowPowerMode();
			return true;
		}
		
		DebugTRACE("Cannot connect - Reason code: ");
		DebugPRINTLN(WiFi.reasonCode());		
		delay(retryDelayMs);
	}
	while (WiFi.status() != WL_CONNECTED && --retries > 0);
	
	WiFi.end();
	wiFiDrv.wifiDriverDeinit();
	return false;
}

void WiFiManager::Disconnect()
{
	WiFi.disconnect();
	WiFi.end();
	wiFiDrv.wifiDriverDeinit();
}