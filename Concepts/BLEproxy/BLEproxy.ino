#include "credentials.h" 
#include "deviceconfig.h"
#include "AprsWebClient.h"
#include "BleAdvertisementParser.h"
#include "WiFiManager.h"
#include "NtpRtc.h"
#include <ArduinoBLE.h>


AprsWebClient aprsClient;
BleAdvertisementParser bleParser;


void setup()
{
	pinMode(USER_LED_PIN, OUTPUT);
	
	Serial.begin(DEBUG_BAUDRATE);
	Serial.setTimeout(1000);	
#ifdef _DEBUG_TRACE_
	uint8_t serialStartupDelay = 50;
	while (!Serial && --serialStartupDelay)
	{
		delay(100);
	}
#endif
	DebugPRINT("\n");
	DebugTRACE("Hello!\n");
	
	UserLED(ON);
	NtpRtc::instance()->PrintTime();
	WiFiManager::Disconnect();
	UserLED(OFF);

	if (!BLE.begin())
	{
		DebugTRACE("Starting BLE failed!\n");
		error();
	}	
	BLE.scanForAddress(SHELLY_BLUHT_ADDRESS);
}


void loop()
{
	BLEDevice peripheral = BLE.available();
	if (peripheral && peripheral.hasAdvertisementData())
	{
		if (bleParser.FetchBleData(peripheral))
		{
			BLE.stopScan();
			BLE.end();

			aprsClient.SendWeatherReportPacket(
				bleParser.GetHumidity(),
				bleParser.GetTemperature(),
				bleParser.GetBatteryLevel()
			);

			if (!BLE.begin())
			{
				DebugTRACE("Starting BLE failed!\n");
				error();
			}	
			BLE.scanForAddress(SHELLY_BLUHT_ADDRESS);
		}
	}
}


void error()
{
	while (1)
	{
		UserLED(OFF);
		delay(500);
		UserLED(ON);
		delay(500);
	}
}
