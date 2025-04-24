#include "AHTX0.h"

extern "C"
{
#include "twi.h"
#include <util/delay.h>
}

#define AHTX0_CMD_CALIBRATE			0xE1	// Calibration command
#define AHTX0_CMD_TRIGGER			0xAC	// Trigger reading command
#define AHTX0_CMD_SOFTRESET			0xBA	// Soft reset command
#define AHTX0_STATUS_BUSY			0x80	// Status bit for busy
#define AHTX0_STATUS_CALIBRATED		0x08	// Status bit for calibrated


bool AHTX0::begin(const uint8_t i2c_address)
{
	mAddress = i2c_address;
	_delay_ms(20); // 20 ms to power up

	TWI_MasterInit();

	uint8_t cmd[3];

	cmd[0] = AHTX0_CMD_SOFTRESET;
	if (TWI_MasterWrite(mAddress, cmd, 1, TWIM_SEND_STOP) != 0)
	{
		return false;
	}
	_delay_ms(20);

	while (getStatus() & AHTX0_STATUS_BUSY)
	{
		_delay_ms(10);
	}

	cmd[0] = AHTX0_CMD_CALIBRATE;
	cmd[1] = 0x08;
	cmd[2] = 0x00;
	TWI_MasterWrite(mAddress, cmd, 3, TWIM_SEND_STOP); // may not 'succeed' on newer AHT20s

	while (getStatus() & AHTX0_STATUS_BUSY)
	{
		_delay_ms(10);
	}
	if (!(getStatus() & AHTX0_STATUS_CALIBRATED))
	{
		return false;
	}
	
	return true;
}


uint8_t AHTX0::getStatus()
{
	uint8_t ret = 0xFF;
	TWI_MasterRead(mAddress, &ret, 1, TWIM_SEND_STOP);
	return ret;
}


bool AHTX0::isBusy()
{
	return (getStatus() & AHTX0_STATUS_BUSY) == AHTX0_STATUS_BUSY;
}


bool AHTX0::triggerRead()
{
	uint8_t cmd[3] = { AHTX0_CMD_TRIGGER, 0x33, 0 };
	if (TWI_MasterWrite(mAddress, cmd, 3, TWIM_SEND_STOP) != 0)
	{
		return false;
	}
	return true;
}


void AHTX0::readData(uint32_t &humidity, int32_t &temperature)
{
	uint8_t data[6];
	TWI_MasterRead(mAddress, data, 6, TWIM_SEND_STOP);
	
	uint32_t srh = (data[1] * 0x1000) + (data[2] * 0x10) + (data[3] / 0x10);	
	humidity = (uint32_t)srh * (uint32_t)100 / (uint32_t)0x100000;
	
	uint32_t st = ((data[3] & 0x0F) * 0x10000) + (data[4] * 0x100) + data[5];
	temperature = (int32_t)st * (int32_t)2000 / (int32_t)0x100000 - (int32_t)500;
}


void AHTX0::readData(float &humidity, float &temperature)
{
	uint8_t data[6];
	TWI_MasterRead(mAddress, data, 6, TWIM_SEND_STOP);
	
	uint32_t h = data[1];
	h <<= 8;
	h |= data[2];
	h <<= 4;
	h |= data[3] >> 4;
	humidity = ((float)h * 100) / 0x100000;

	uint32_t tdata = data[3] & 0x0F;
	tdata <<= 8;
	tdata |= data[4];
	tdata <<= 8;
	tdata |= data[5];
	temperature = ((float)tdata * 200 / 0x100000) - 50;
}


bool AHTX0::read(float &humidity, float &temperature)
{
	if (!triggerRead())
	{
		return false;
	}
	while (isBusy())
	{
		_delay_ms(10);
	}
	readData(humidity, temperature);
	return true;
}
