/*
 * CPU = ATmega328PB
 * CLK = 8 MHz
 */

#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "BME280/BME280I2C.h"			// Original source: https://www.github.com/finitespace/BME280

#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#include "Debug.h"
#endif

#define PRELOAD_250US			0x06
#define PRELOAD_1000MS			0x85EE
#define DATA_BITS				41
#define PREAMBLE_BITS			8
#define BITS_PER_BYTE			8
#define PACKET_LENGTH_BITS		(DATA_BITS + PREAMBLE_BITS)
#define PACKET_LENGTH_BYTES		(PACKET_LENGTH_BITS / BITS_PER_BYTE)
#define PACKET_COUNT			15
#define TX_IN_PROGRESS			(TCCR0B & 0x7)


BME280I2C::Settings settings(
	BME280::OSR_X1,
	BME280::OSR_X1,
	BME280::OSR_X1,
	BME280::Mode_Forced,
	BME280::StandbyTime_1000ms,
	BME280::Filter_Off,
	BME280::SpiEnable_False,
	BME280I2C::I2CAddr_0x76
);

BME280I2C bme(settings);
SerialDebugging debug;

volatile uint8_t txBuffer[PACKET_LENGTH_BYTES];
volatile uint8_t currentByte;
volatile uint8_t currentBit;
volatile uint8_t cmdReadEnvironmentData;


ISR(TIMER0_OVF_vect)
{
	static uint8_t temp = 0;
	static uint8_t cycle = 0;
	static int8_t currentBitValue = -1;
	uint8_t sreg = SREG;
	
	TCNT0 = PRELOAD_250US;
	
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
			
			currentBitValue = ((temp & 0x80) == 0x80);;
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
				TCCR0B = 0;
				TX_PIN_LOW();
			}
		}
	}
	
	++cycle;
	if (cycle > 2)
	{
		cycle = 0;
	}

	SREG = sreg;
}


ISR(TIMER3_OVF_vect)
{
	static unsigned char postscaler = 60;
	uint8_t sreg = SREG;
	TCNT3 = PRELOAD_1000MS;
	
	if (--postscaler == 0)
	{
		/* Trigger measurement every minute */
		postscaler = 60;
		cmdReadEnvironmentData = 1;
	}
	
	SREG = sreg;
}


int16_t centigradeToFahrenheit(const int16_t& in)
{
	return (in * 18 / 10) + 320;
}


uint16_t fahrenheitToRaw(const int16_t& in)
{
	return in + 900;
}


void assemblePacket(const uint8_t& id, const uint8_t& batteryLow, const uint8_t& test, const uint8_t& channel, const float& temperature, const float& humidity)
{
	uint8_t intHumidity = (uint8_t)round(humidity);
	int16_t intTemperature = (int16_t)round(temperature * 10.0);
	
#ifdef ENABLE_DEBUG
	debug.sendText("\tID ");
	debug.sendValue(id);
	debug.sendText("\tt = ");
	debug.sendValue(intTemperature);
	debug.sendText("\th = ");
	debug.sendValue(intHumidity);
	debug.sendText("\n");
#endif
	
	if ((channel == 0) || (intHumidity > 100) || (intTemperature < -677) || (intTemperature > 1590))
	{
		return;
	}
	
	uint16_t temperatureRaw = fahrenheitToRaw(centigradeToFahrenheit(intTemperature));
	
	txBuffer[0] = id;
	txBuffer[1] = ((batteryLow & 0x1) << 7) | ((test & 0x1) << 6) | ((channel & 0x3) << 4) | ((temperatureRaw >> 8) & 0xF);	
	txBuffer[2] = (temperatureRaw & 0x00FF);
	txBuffer[3] = intHumidity;
	txBuffer[4] = (txBuffer[0] + txBuffer[1] + txBuffer[2] + txBuffer[3]) % 256;
	txBuffer[5] = 0;
}


int main(void)
{
	uint8_t packetCount = 0;
	uint8_t id = rand() % 255;
	uint8_t batteryLow = 0;
	uint8_t testButtonPressed = 0;
	uint8_t channel = 2;
	float temperature = NAN;
	float humidity = NAN;
	float pressure = NAN;
	
	cmdReadEnvironmentData = 1;
	
	// Power reduction
	ACSR = (1 << ACD);	// Analog Comparator off
	PRR0 |= (1 << PRTWI0) | (1 << PRTIM2) | (1 << PRUSART1) | (1 << PRTIM1) | (1 << PRSPI0) | (1 << PRADC);
	PRR1 |= (1 << PRPTC) | (1 << PRTIM4) | (1 << PRSPI1);
		
	DDRB |= LED0_PIN;
	DDRC |= TX_PIN | LED1_PIN;
	
	// Prepare Timer0
	TIMSK0 = (1 << TOIE0);
	
	// Configure Timer3
	TCNT3 = PRELOAD_1000MS;
	TCCR3A = 0;
	TCCR3B = (4 << CS30); // clk/256
	TCCR3C = 0;
	TIFR3 |= (1 << TOV3);
	TIMSK3 = (1 << TOIE3);
	
#ifdef ENABLE_DEBUG
	debug.begin();
	debug.sendText("Hello\n");
#endif
	
	sei();
	bme.begin();
	
	while(1) 
	{
		if (packetCount > 0 && !TX_IN_PROGRESS)
		{
			// Start TX
			currentBit = 0;
			currentByte = 0;
			TX_PIN_LOW();
			TCCR0B = (2 << CS00); // Clk/8
			TCNT0 = PRELOAD_250US;
			TIFR0 = (1 << TOV0);
			
			--packetCount;
		}
		
		if (packetCount == 0 && (SW0_PRESSED || cmdReadEnvironmentData))
		{
#ifdef ENABLE_DEBUG
			debug.sendText("Starting measurement\n");
#endif			
			bme.read(pressure, temperature, humidity); // Returns hPa and Celsius
			testButtonPressed = SW0_PRESSED;
			
			assemblePacket(id, batteryLow, testButtonPressed, channel, temperature, humidity);
			
			packetCount = PACKET_COUNT;
			cmdReadEnvironmentData = 0;
			_delay_ms(100);
		}
	}
	
	return 0;
}

extern "C" void __cxa_pure_virtual(void) __attribute__ ((__noreturn__));
void __cxa_pure_virtual(void) {
	while(1);
}

extern "C" void __cxa_deleted_virtual(void) __attribute__ ((__noreturn__));
void __cxa_deleted_virtual(void) {
	while(1);
}
