#pragma once

#include "NtpRtc.h"
#include <ArduinoBLE.h>

class BleAdvertisementParser
{
	/// @brief Time in [s] of which the last BLE packet is considered valid
	static constexpr auto maxDataAge =	(5 * 60);

public:
	BleAdvertisementParser();

	bool FetchBleData(BLEDevice& device);
	bool IsDataValid() const;

	unsigned char GetPacketId() const;
	unsigned char GetBatteryLevel() const;
	unsigned char GetHumidity() const;
	short GetTemperature() const;
	bool GetButtonPressed() const;
	bool HasParseError() const;

private:
	void ParseAdvertisementData(const unsigned char* pData, unsigned char dataLen);
	void ParseAdvertisingDataElement(const unsigned char* const data, const unsigned char elementLen);
	void ParseServiceDataElement(const unsigned char* pData, unsigned char dataLen);
	bool IsBtHomeUuid(const unsigned char* const data, const unsigned char dataLen) const;
	bool IsUnencryptedBtHomeVersion2(const unsigned char flags) const;
	unsigned char ParseServiceItem(const unsigned char* const data, const unsigned char availableDataLen);

	unsigned char mPacketId;		// [ 0..255 ]
	unsigned char mBatteryLevel;	// [ % ]
	unsigned char mHumidity;		// [ % ]
	short mTemperature;				// [ 0.1 °C ]
	bool mButtonPressed;
	bool mParseError;
	time_t mLastSuccess;
	unsigned char mLastPacketId;
};

