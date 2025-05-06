/*
BME280I2CI2C.cpp
This code records data from the BME280I2C sensor and provides an API.
This file is part of the Arduino BME280I2C library.
Copyright (C) 2016  Tyler Glenn

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Written: Dec 30 2015.
Last Updated: Jan 1 2016. - Happy New year!

This header must be included in any derived code or copies of the code.

Based on the data sheet provided by Bosch for the BME280I2C environmental sensor,
calibration code based on algorithms providedBosch, some unit conversations courtesy
of www.endmemo.com, altitude equation courtesy of NOAA, and dew point equation
courtesy of Brian McNoldy at http://andrew.rsmas.miami.edu.
 */

#include "BME280I2C.h"

extern "C" {
  #include "twi.h"
}

/****************************************************************/
BME280I2C::BME280I2C
(
  const Settings& settings
):BME280(settings),
  m_settings(settings)
{
}


/****************************************************************/
void BME280I2C::setSettings
(
   const Settings& settings
)
{
   m_settings = settings;
   BME280::setSettings(settings);
}


/****************************************************************/
const BME280I2C::Settings& BME280I2C::getSettings() const
{
   return m_settings;
}


/****************************************************************/
bool BME280I2C::begin()
{
	twi_init();
	return BME280::begin();
}


/****************************************************************/
bool BME280I2C::WriteRegister
(
  uint8_t addr,
  uint8_t data
)
{  
  uint8_t txBuffer[2] = { addr, data };
  twi_writeTo(m_settings.bme280Addr, txBuffer, sizeof(txBuffer), 1, 1);

  return true; // TODO: Check return values from wire calls.
}


/****************************************************************/
bool BME280I2C::ReadRegister
(
  uint8_t addr,
  uint8_t data[],
  uint8_t length
)
{  
  uint8_t txBuffer[1] = { addr };
  twi_writeTo(m_settings.bme280Addr, txBuffer, sizeof(txBuffer), 1, 1);
  
  uint8_t rxLength = twi_readFrom(m_settings.bme280Addr, data, length, 1);

  return rxLength == length;
}
