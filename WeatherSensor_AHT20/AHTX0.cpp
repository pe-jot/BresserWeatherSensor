/*
 * Software License Agreement (BSD License)
 * 
 * Copyright (c) 2019 Limor Fried (Adafruit Industries)
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the
 * names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

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
	_delay_ms(200); // 200 ms to power up

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


void AHTX0::readData(uint8_t *pData)
{
	TWI_MasterRead(mAddress, pData, 6, TWIM_SEND_STOP);
}


void AHTX0::readData(uint32_t &humidity, int32_t &temperature)
{
	uint8_t data[6];
	TWI_MasterRead(mAddress, data, 6, TWIM_SEND_STOP);
	
	uint32_t srh = ((uint32_t)data[1] * 0x1000) + ((uint32_t)data[2] * 0x10) + (data[3] / 0x10);
	humidity = (uint32_t)srh * (uint32_t)100 / (uint32_t)0x100000;
	
	uint32_t st = ((uint32_t)(data[3] & 0x0F) * 0x10000) + ((uint32_t)data[4] * 0x100) + data[5];
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
