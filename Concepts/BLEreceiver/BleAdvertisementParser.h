#pragma once

class BleAdvertisementParser
{
public:
    BleAdvertisementParser();

    void ParseAdvertisementData(const unsigned char* pData, unsigned char dataLen);

    unsigned char GetPacketId() const { return mPacketId; }
    unsigned char GetBatteryLevel() const { return mBatteryLevel; }
    unsigned char GetHumidity() const { return mHumidity; }
    short GetTemperature() const { return mTemperature; }
    bool GetButtonPressed() const { return mButtonPressed; }
    bool HasParseError() const { return mParseError; }

private:
    void ParseAdvertisingDataElement(const unsigned char* const data, const unsigned char elementLen);
    void ParseServiceDataElement(const unsigned char* pData, unsigned char dataLen);
    bool IsBtHomeUuid(const unsigned char* const data, const unsigned char dataLen) const;
    bool IsUnencryptedBtHomeVersion2(const unsigned char flags) const;
    unsigned char ParseServiceItem(const unsigned char* const data, const unsigned char availableDataLen);

    unsigned char mPacketId;     // [ 0..255 ]
    unsigned char mBatteryLevel; // [ % ]
    unsigned char mHumidity;     // [ % ]
    short mTemperature;          // [ 0.1 °C ]
    bool mButtonPressed;
    bool mParseError;
};

