
/*
 * Operation states:
 * Init			=> Full / Run
 * ReadEnv		=> Full / Run
 * Send/Calc	=> Mid / Run
 * Send/Wait	=> Mid-Low / Stby
 * Idle			=> X / PowerDown
 */

#include "config.h"

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <math.h>

#define ENABLE_DEBUG
#include "Debug.h"
#include "AHTX0.h"						// Original source: https://github.com/adafruit/Adafruit_AHTX0


#define DATA_BITS						41
#define PREAMBLE_BITS					8
#define BITS_PER_BYTE					8
#define PACKET_LENGTH_BITS				(DATA_BITS + PREAMBLE_BITS)
#define PACKET_LENGTH_BYTES				(PACKET_LENGTH_BITS / BITS_PER_BYTE)
#define PACKET_COUNT					15
#define TX_IN_PROGRESS					(TCB0.STATUS & TCB_RUN_bm)


AHTX0 sensor;
SerialDebugging debug;

volatile uint8_t cmdEnterPowersave;
volatile uint8_t cmdEnterStandby;
volatile uint8_t cmdSleep;
volatile uint8_t cmdReadEnvironmentData;
volatile uint8_t txBuffer[PACKET_LENGTH_BYTES];
volatile uint8_t currentByte;
volatile uint8_t currentBit;

const uint8_t testButtonPressed = 0;
const uint8_t batteryLow = 0;
const uint8_t channel = 2;

uint8_t packetCount;
uint8_t id;
float temperature;
float humidity;


inline void configureFullSpeed(void)
{
	// Configure clock to 5 MHz
	UNLOCK_PROTECTED_REGISTERS();
	CLKCTRL.MCLKCTRLA = CLKCTRL_CLKSEL_OSC20M_gc;
	UNLOCK_PROTECTED_REGISTERS();
	CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_4X_gc | CLKCTRL_PEN_bm; // Clk/4 = 5 MHz
	// Wait for clock being ready & stable
	while ((CLKCTRL.MCLKSTATUS & (CLKCTRL_OSC20MS_bm | CLKCTRL_SOSC_bm)) != CLKCTRL_OSC20MS_bm);
}


inline void configureMediumSpeed(void)
{
	// Configure clock to 1 MHz
	UNLOCK_PROTECTED_REGISTERS();
	CLKCTRL.MCLKCTRLA = CLKCTRL_CLKSEL_OSC20M_gc;
	UNLOCK_PROTECTED_REGISTERS();
	CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_16X_gc | CLKCTRL_PEN_bm; // Clk/16 = 1 MHz
	// Wait for clock being ready & stable
	while ((CLKCTRL.MCLKSTATUS & (CLKCTRL_OSC20MS_bm | CLKCTRL_SOSC_bm)) != CLKCTRL_OSC20MS_bm);
}


inline void configureLowSpeed(void)
{
	// Configure clock to 32 kHz
	UNLOCK_PROTECTED_REGISTERS();
	CLKCTRL.MCLKCTRLA = CLKCTRL_CLKSEL_OSCULP32K_gc;
	UNLOCK_PROTECTED_REGISTERS();
	CLKCTRL.MCLKCTRLB = 0; // Clk/1 resulting in 32768 Hz
	// Wait for clock being ready & stable
	while ((CLKCTRL.MCLKSTATUS & (CLKCTRL_OSC32KS_bm | CLKCTRL_SOSC_bm)) != CLKCTRL_OSC32KS_bm);
}


ISR(RTC_PIT_vect) // Every 4s - first interval is undefined (anything between 0..4s)
{
	static uint8_t pitCycleCount = 1;
	uint8_t sreg = SREG;
	
	if (--pitCycleCount == 0)
	{
		configureFullSpeed();
		cmdReadEnvironmentData = 1;
		pitCycleCount = 15;
	}
	else
	{
		// Don't touch anything, just go back to sleep again
		cmdSleep = 1;
	}

	RTC.PITINTFLAGS = RTC_PI_bm;
	SREG = sreg;
}


ISR(TCB0_INT_vect) // Every 250µs @ 1 MHz - could be named TCB0_CAPT_vect as well :-/
{
	static uint8_t temp = 0;
	static uint8_t cycle = 0;
	static int8_t currentBitValue = -1;
	
	uint8_t sreg = SREG;
	
	if (cycle == 0)
	{
		if (currentBit < PREAMBLE_BITS)
		{
			currentBitValue = -1;
		}
		else
		{
			// Byte finished - load next one
			if ((currentBit % BITS_PER_BYTE) == 0)
			{
				temp = txBuffer[currentByte];
				++currentByte;
			}
			
			currentBitValue = ((temp & 0x80) == 0x80);
			temp = temp << 1;
		}
		
		++currentBit;
		
		// In case of preamble sequence this actually generates an alternating signal.
		// In case of the data bits this generates the negative edge.
		TX_PIN_TOGGLE();
	}
	else
	{
		// 0 bit ... short pulse of 250 us followed by a 500 us gap
		// 1 bit ... long pulse of 500 us followed by a 250 us gap
		if ((currentBitValue == 0 && cycle == 1) || (currentBitValue == 1 && cycle == 2))
		{
			TX_PIN_LOW();
			if (currentBit >= PACKET_LENGTH_BITS)
			{
				currentBit = 0;
				// Stop TX
				TCB0.CTRLA &= ~TCB_ENABLE_bm; // Stop Timer
				TX_PIN_LOW();
			}
		}
	}
	
	++cycle;
	if (cycle > 2)
	{
		cycle = 0;
	}
	
	if (TX_IN_PROGRESS)
	{
		// Only enter standby while we have to wait for the next bit to send
		cmdEnterStandby = 1;
	}
	
	TCB0.INTFLAGS = TCB_CAPT_bm;
	SREG = sreg;
}


int16_t centigradeToFahrenheit(const int16_t in)
{
	return (in * 18 / 10) + 320;
}


uint16_t fahrenheitToRaw(const int16_t in)
{
	return in + 900;
}


void assemblePacket(const uint8_t id, const uint8_t battLow, const uint8_t test, const uint8_t channel, const float temperature, const float humidity)
{
	uint8_t intHumidity = (uint8_t)round(humidity);
	int16_t intTemperature = (int16_t)round(temperature * 10.0);
	
	DEBUG_BYTE('#');
	DEBUG_VALUE(id);
	DEBUG_BYTE('t');
	DEBUG_VALUE(intTemperature);
	DEBUG_BYTE('h');
	DEBUG_VALUE(intHumidity);
	
	if ((channel == 0) || (intHumidity > 100) || (intTemperature < -677) || (intTemperature > 1590))
	{
		return;
	}
	
	uint16_t temperatureRaw = fahrenheitToRaw(centigradeToFahrenheit(intTemperature));
	
	txBuffer[0] = id;
	txBuffer[1] = ((battLow & 0x1) << 7) | ((test & 0x1) << 6) | ((channel & 0x3) << 4) | ((temperatureRaw >> 8) & 0xF);
	txBuffer[2] = (temperatureRaw & 0x00FF);
	txBuffer[3] = intHumidity;
	txBuffer[4] = (txBuffer[0] + txBuffer[1] + txBuffer[2] + txBuffer[3]) % 256;
	txBuffer[5] = 0;
}


void setup(void)
{
	configureFullSpeed();

	cmdEnterStandby = 0;
	cmdEnterPowersave = 0;
	cmdSleep = 0;
	cmdReadEnvironmentData = 0;
	
	packetCount = 0;
	id = rand() % 255;
	
	temperature = NAN;
	humidity = NAN;

	CONFIGURE_IOPORTS();
	
	// Configure RTC to 4s PIT
	while (RTC.STATUS != 0);
	RTC.CLKSEL = RTC_CLKSEL_INT1K_gc;
	RTC.PITINTCTRL = RTC_PI_bm;
	while ((RTC.PITSTATUS & RTC_CTRLBUSY_bm) != 0);
	RTC.PITCTRLA = RTC_PERIOD_CYC4096_gc | RTC_PITEN_bm;
	
	// Configure TCB0 to 250 µs interrupt (@ 1 MHz). Only the simpler TCBn is capable of running in Idle & Standby Sleep Modes
	TCB0.CTRLA = TCB_RUNSTDBY_bm;
	TCB0.INTCTRL = TCB_CAPT_bm;
	TCB0.CCMP = 250;
	
#ifdef ENABLE_DEBUG
	debug.begin();
#endif	

	sei();
	
	if (!sensor.begin())
	{
		LED_ON();
		while(1);
	}
}


void loop(void)
{
	if (cmdEnterStandby)
	{
		SLPCTRL.CTRLA = SLPCTRL_SMODE_STDBY_gc | SLPCTRL_SEN_bm;
		cmdSleep = 1;
		cmdEnterStandby = 0;
	}
	else if (cmdEnterPowersave)
	{
		configureLowSpeed();
		SLPCTRL.CTRLA = SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;
		cmdSleep = 1;
		cmdEnterPowersave = 0;
	}

	if (cmdSleep)
	{
		cmdSleep = 0;
		sleep_cpu();
		return;
	}
	
	if (packetCount > 0 && !TX_IN_PROGRESS)
	{
		// Start TX
		currentBit = 0;
		currentByte = 0;
		TX_PIN_LOW();
		TCB0.CTRLA |= TCB_ENABLE_bm; // Start Timer
		--packetCount;
	}

	if (cmdReadEnvironmentData && packetCount == 0)
	{
		cmdReadEnvironmentData = 0;
		
		// CPU is configured to full speed at interrupt level already.
		temperature = NAN;
		humidity = NAN;
		if (!sensor.read(humidity, temperature)) // Returns centigrade
		{
			LED_ON();
			while(1);
		}
		assemblePacket(id, batteryLow, testButtonPressed, channel, temperature, humidity);
		
		// Initiate transmission
		TXPWR_ON();
		configureMediumSpeed();
		packetCount = PACKET_COUNT;
	}
	
	if (!cmdReadEnvironmentData && !TX_IN_PROGRESS && packetCount == 0)
	{
		TXPWR_OFF();
		cmdEnterPowersave = 1;
	}
}


int main(void)
{
	setup();
	while(1)
	{
		loop();
	}
}
