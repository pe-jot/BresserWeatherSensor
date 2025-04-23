#pragma once

#include <stdint.h>

#define AHTX0_I2CADDR_DEFAULT		0x38	// AHT default i2c address
#define AHTX0_I2CADDR_ALTERNATE		0x39	// AHT alternate i2c address

class AHTX0
{
public:
	bool begin(const uint8_t i2c_address = AHTX0_I2CADDR_DEFAULT);
	bool read(float &humidity, float &temperature);
	void readData(float &humidity, float &temperature);
	void readData(uint32_t &humidity, int32_t &temperature);
	bool triggerRead();
	bool isBusy();
	
private:
	uint8_t getStatus();
	uint8_t mAddress;
};
