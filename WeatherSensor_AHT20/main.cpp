
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
#include <util/delay.h>

#define ENABLE_DEBUG
#include "Debug.h"
#include "AHTX0.h"						// Original source: https://github.com/adafruit/Adafruit_AHTX0


#define DATA_BITS						41
#define PREAMBLE_BITS					8
#define BITS_PER_BYTE					8
#define PACKET_LENGTH_BITS				(DATA_BITS + PREAMBLE_BITS)
#define PACKET_LENGTH_BYTES				(PACKET_LENGTH_BITS / BITS_PER_BYTE)
#define PACKET_COUNT					15


AHTX0 sensor;
SerialDebugging debug;

typedef void (*FPinterruptHandler)(void);
volatile FPinterruptHandler fpInterruptHandler;

volatile enum OperationStates opState;
volatile uint8_t txBuffer[PACKET_LENGTH_BYTES];
volatile uint8_t currentByte;
volatile uint8_t currentBit;


void configureFullSpeed(void)
{
	// Configure clock to 1 MHz
	UNLOCK_PROTECTED_REGISTERS();
	CLKCTRL.MCLKCTRLA = CLKCTRL_CLKSEL_OSC20M_gc;
	UNLOCK_PROTECTED_REGISTERS();
	CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_16X_gc | CLKCTRL_PEN_bm; // Clk/16 = 1 MHz
	// Wait for clock being ready & stable
	while ((CLKCTRL.MCLKSTATUS & (CLKCTRL_OSC20MS_bm | CLKCTRL_SOSC_bm)) != CLKCTRL_OSC20MS_bm);
}


void configureLowSpeed(void)
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
		opState = TRIGGER_SENSOR_READ;
		pitCycleCount = 15;
	}

	RTC.PITINTFLAGS = RTC_PI_bm;
	SREG = sreg;
}


ISR(TCB0_INT_vect)
{
	uint8_t sreg = SREG;
	
	if (fpInterruptHandler)
	{
		fpInterruptHandler();
	}
	
	TCB0.INTFLAGS = TCB_CAPT_bm;
	SREG = sreg;
}


void transmit_handler()
{
	static uint8_t temp = 0;
	static uint8_t cycle = 0;
	static int8_t currentBitValue = -1;
	
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
				// Packet sending complete
				STOP_TCB0();
				TX_PIN_LOW();
			}
		}
	}
	
	++cycle;
	if (cycle > 2)
	{
		cycle = 0;
	}
}


void assemblePacket(const uint8_t id, const uint8_t battLow, const uint8_t test, const uint8_t channel, const int16_t temperature, const uint8_t humidity)
{
	DEBUG_BYTE('#');
	DEBUG_VALUE(id);
	DEBUG_BYTE('t');
	DEBUG_VALUE(temperature);
	DEBUG_BYTE('h');
	DEBUG_VALUE(humidity);
	
	if ((channel == 0) || (humidity > 100) || (temperature < -677) || (temperature > 1590))
	{
		return;
	}
	
	int16_t temperatureFahrenheit = (temperature * 18 / 10) + 320;
	uint16_t temperatureRaw = temperatureFahrenheit + 900;
	
	txBuffer[0] = id;
	txBuffer[1] = ((battLow & 0x1) << 7) | ((test & 0x1) << 6) | ((channel & 0x3) << 4) | ((temperatureRaw >> 8) & 0xF);
	txBuffer[2] = (temperatureRaw & 0x00FF);
	txBuffer[3] = humidity;
	txBuffer[4] = (txBuffer[0] + txBuffer[1] + txBuffer[2] + txBuffer[3]) % 256;
	txBuffer[5] = 0;
}


void setup(void)
{
	configureFullSpeed();

	opState = PREPARE_POWERDOWN;
	
	// Configure voltage monitoring
	// Loaded from fuse: BOD.CTRLA = BOD_SAMPFREQ_125Hz_gc | BOD_ACTIVE_SAMPLED_gc | BOD_SLEEP_DIS_gc;
	// Loaded from fuse: BOD.CTRLB = BOD_LVL_BODLEVEL0_gc; // BOD 1.8V
	BOD.VLMCTRLA = BOD_VLMLVL_25ABOVE_gc; // VLM threshold: BOD+25%

	// Configure IO Ports
	PORTA.DIRSET = TXPWR_BIT | TXPWR_GND_BIT | TX_PIN_BIT;
	PORTA.OUTCLR = TXPWR_BIT | TXPWR_GND_BIT | TX_PIN_BIT;
	PORTC.DIRSET = LED_BIT;
	PORTC.OUTSET = LED_BIT;
	
	// Configure RTC to 4s PIT
	while (RTC.STATUS != 0);
	RTC.CLKSEL = RTC_CLKSEL_INT1K_gc;
	RTC.PITINTCTRL = RTC_PI_bm;
	while ((RTC.PITSTATUS & RTC_CTRLBUSY_bm) != 0);
	RTC.PITCTRLA = RTC_PERIOD_CYC4096_gc | RTC_PITEN_bm;
	
	// Only the simpler TCBn is capable of running in Idle & Standby sleep modes.
	// Unfortunately, ATtiny816 has only one TCB, so we need to switch between the two different functions in software.
	// Common TCB0 configuration
	TCB0.CTRLA = TCB_RUNSTDBY_bm;
	TCB0.CTRLB = TCB_CNTMODE_INT_gc;
	TCB0.INTCTRL = TCB_CAPT_bm;
	
#ifdef ENABLE_DEBUG
	debug.begin();
#endif	

	sei();
	
	if (!sensor.begin())
	{
		opState = ERROR;
	}
}


void loop(void)
{
	static uint8_t packetCount = 0;
	static uint8_t id = rand() % 255;
	static uint8_t testButtonPressed = 1;
	
	switch (opState)
	{
		case PREPARE_POWERDOWN:
			configureLowSpeed();
			SLPCTRL.CTRLA = SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;
			opState = WAIT_FOR_READ;
			
		case WAIT_FOR_READ:
			sleep_cpu();
			break;
			
		case TRIGGER_SENSOR_READ:
			configureFullSpeed();
			if (!sensor.triggerRead())
			{
				opState = ERROR;
			}
			// Prepare for standby sleep mode
			SLPCTRL.CTRLA = SLPCTRL_SMODE_STDBY_gc | SLPCTRL_SEN_bm;
			// Configure TCB0 to 50ms interrupt (@ 1 MHz). 
			fpInterruptHandler = 0;
			TCB0.CCMP = 50000;
			START_TCB0();
			opState = WAIT_FOR_SENSOR;
			
		case WAIT_FOR_SENSOR:
			sleep_cpu();
			if (!sensor.isBusy())
			{
				STOP_TCB0();
				opState = READ_SENSOR;
			}
			break;
			
		case READ_SENSOR:
			{
				int32_t temperature;
				uint32_t humidity;
				sensor.readData(humidity, temperature); // Returns 1/10 centigrade
				
				const uint8_t batteryLow = BOD.STATUS & BOD_VLMS_bm;
				const uint8_t channel = 2;
				assemblePacket(id, batteryLow, testButtonPressed, channel, (int16_t)temperature, (uint8_t)humidity);
				
				testButtonPressed = 0; // Only set the first time
			}
			// Initiate transmission
			TXPWR_ON();
			packetCount = PACKET_COUNT;
			// Prepare for standby sleep mode
			SLPCTRL.CTRLA = SLPCTRL_SMODE_STDBY_gc | SLPCTRL_SEN_bm;
			// Configure TCB0 to 250 µs periodic interrupt (@ 1 MHz).
			fpInterruptHandler = transmit_handler;
			TCB0.CCMP = 250;
			opState = INIT_NEXT_TX_PACKET;
			
		case INIT_NEXT_TX_PACKET:
			currentBit = 0;
			currentByte = 0;
			TX_PIN_LOW();	
			START_TCB0();
			--packetCount;
			opState = WAIT_FOR_PACKET_TRANSMITTED;
			
		case WAIT_FOR_PACKET_TRANSMITTED:
			if (TCB0_RUNNING)
			{
				// Let interrupt handler do its job
				sleep_cpu();
			}
			else
			{
				if (packetCount > 0)
				{
					opState = INIT_NEXT_TX_PACKET;
				}
				else
				{
					// Entire transmission finished
					TXPWR_OFF();
					opState = PREPARE_POWERDOWN;
				}
			}
			break;
			
		case ERROR:
			LED_ON();
			configureLowSpeed();
			while(1);
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
