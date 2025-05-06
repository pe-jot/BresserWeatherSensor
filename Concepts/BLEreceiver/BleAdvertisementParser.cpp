#include "BleAdvertisementParser.h"
#include <string.h>

BleAdvertisementParser::BleAdvertisementParser()
    : mPacketId(0)
    , mBatteryLevel(0)
    , mHumidity(0)
    , mTemperature(0)
    , mButtonPressed(false)
    , mParseError(false)
{
}

void BleAdvertisementParser::ParseAdvertisementData(const unsigned char* pData, unsigned char dataLen)
{
    while (dataLen)
    {
        const unsigned char elementLen = *pData;
        pData += 1;
        dataLen -= 1;

        if (dataLen < elementLen || elementLen == 0)
        {
            mParseError = true;
            return; // Packet incomplete or corrupt
        }

        ParseAdvertisingDataElement(pData, elementLen);
        pData += elementLen;
        dataLen -= elementLen;
    }
}

void BleAdvertisementParser::ParseAdvertisingDataElement(const unsigned char* const data, const unsigned char elementLen)
{
    const unsigned char msgtype = data[0];
    unsigned char* pData = (unsigned char*)&data[1];
    switch (msgtype)
    {
    case 0x16:  // Service Data
        ParseServiceDataElement(pData, elementLen - 1);
        break;

    default:    // Unknown (or at least not of our interest)
        return;
    }
}

void BleAdvertisementParser::ParseServiceDataElement(const unsigned char* pData, unsigned char dataLen)
{
    if (dataLen < 5 || !IsBtHomeUuid(&pData[0], dataLen) || !IsUnencryptedBtHomeVersion2(pData[2]))
    {
        return;
    }
    pData += (2 + 1);
    dataLen -= (2 + 1);

    mPacketId = 0;
    mBatteryLevel = 0;
    mHumidity = 0;
    mTemperature = 0;
    mButtonPressed = false;
    mParseError = false;

    while (dataLen)
    {
        char consumedDataLen = ParseServiceItem(pData, dataLen);
        if (consumedDataLen == 0)
        {
            return;
        }
        pData += consumedDataLen;
        dataLen -= consumedDataLen;
    }
}

bool BleAdvertisementParser::IsBtHomeUuid(const unsigned char* const data, const unsigned char dataLen) const
{
    const unsigned char btHomeUuid[] = { 0xD2, 0xFC };
    return (dataLen >= 2) && (memcmp(data, btHomeUuid, sizeof(btHomeUuid)) == 0);
}

bool BleAdvertisementParser::IsUnencryptedBtHomeVersion2(const unsigned char flags) const
{
    const unsigned char encryptionFlagBit = (1 << 0);
    const unsigned char btHomeVersionBits = (7 << 5);
    const unsigned char btHomeVersion2 = (2 << 5);
    return ((flags & (btHomeVersionBits | encryptionFlagBit)) == btHomeVersion2);
}

unsigned char BleAdvertisementParser::ParseServiceItem(const unsigned char* const data, const unsigned char availableDataLen)
{
    unsigned char consumedDataLen = 0;
    const unsigned char itemtype = data[0];
    switch (itemtype)
    {
    case 0x00:  // Packet ID
        if (availableDataLen >= 2)
        {
            mPacketId = data[1];
            consumedDataLen = 2;
        }
        break;

    case 0x01:  // Battery level
        if (availableDataLen >= 2)
        {
            mBatteryLevel = data[1];
            consumedDataLen = 2;
        }
        break;

    case 0x2E:  // Humidity
        if (availableDataLen >= 2)
        {
            mHumidity = data[1];
            consumedDataLen = 2;
        }
        break;

    case 0x45:  // Temperature
        if (availableDataLen >= 3)
        {
            memcpy((void*)&mTemperature, &data[1], 2);
            consumedDataLen = 3;
        }
        break;

    case 0x3A:  // Button event
        if (availableDataLen >= 2)
        {
            mButtonPressed = data[1] == 0x01;
            consumedDataLen = 2;
        }
        break;

    default:    // Unknown
        mParseError = true;    // Error at this point since we cannot continue parsing the remaining data!
        return 0;
    }

    return consumedDataLen;
}
