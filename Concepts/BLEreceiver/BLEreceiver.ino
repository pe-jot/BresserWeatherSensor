#include <ArduinoBLE.h>
#include "BleAdvertisementParser.h"

#define SHELLY_BLUHT_ADDRESS	"7c:c6:b6:61:e3:a0"

BleAdvertisementParser parser;

// APRS part: see Git/Funk/AprsTestWebclient

void setup()
{
	Serial.begin(115200);
	while (!Serial);

	if (!BLE.begin())
	{
		Serial.println("Starting BLE failed!");
		while (1);
	}
	
	BLE.scanForAddress(SHELLY_BLUHT_ADDRESS);
}



void loop()
{
	// Check if a peripheral has been discovered
	BLEDevice peripheral = BLE.available();
	if (peripheral)
	{
		// Discovered a peripheral	
		Serial.print("Address: ");
		Serial.println(peripheral.address());

		if (peripheral.hasLocalName())
		{
			Serial.print("Local Name: ");
			Serial.println(peripheral.localName());
		}

		if (peripheral.hasAdvertisementData())
		{
			uint8_t advertisementDataLength = peripheral.advertisementDataLength();
			Serial.print("Advertisement data length: ");
			Serial.println(advertisementDataLength);

			uint8_t* pAdvertisementData = (uint8_t*)malloc(advertisementDataLength);
			if (!pAdvertisementData)
			{
				return;
			}
			peripheral.advertisementData(pAdvertisementData, advertisementDataLength);

			// Print raw data
			for (uint8_t i = 0; i < advertisementDataLength; i++)
			{
				Serial.print(pAdvertisementData[i] < 0x10 ? " 0" : " ");
				Serial.print(pAdvertisementData[i], HEX);
			}
			Serial.println();

			parser.ParseAdvertisementData(pAdvertisementData, advertisementDataLength);

			// Print parsed data
			Serial.println("Sensor data:");
			Serial.print(" Packet: ");
			Serial.println(parser.GetPacketId());
			Serial.print(" Battery: ");
			Serial.println(parser.GetBatteryLevel());
			Serial.print(" Humidity: ");
			Serial.println(parser.GetHumidity());
			Serial.print(" Temperature: ");
			Serial.println(parser.GetTemperature());
			Serial.print(" Button: ");
			Serial.println(parser.GetButtonPressed());
			Serial.print(" Error: ");
			Serial.println(parser.HasParseError());

			free(pAdvertisementData);
		}

		Serial.println();
	}
}
