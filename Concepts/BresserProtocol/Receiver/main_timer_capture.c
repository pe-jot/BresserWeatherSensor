#ifdef _USE_TIMER_CAPTURE_

/*
 * CPU = ATmega328PB
 * CLK = 8 MHz
 */ 

#define F_CPU					8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "SerialDebug.h"

#define SW0_PIN					(1 << PB7)
#define SW0_PRESSED				((PINB & SW0_PIN) == 0)

#define LED_ON(_PIN, _PORT)		_PORT |= _PIN
#define LED_OFF(_PIN, _PORT)	_PORT &= ~_PIN
#define LED_TOGGLE(_PIN, _PORT)	_PORT ^= _PIN

#define LED0_PIN				(1 << PB5)
#define LED0_PORT				PORTB
#define LED1_PIN				(1 << PC5)
#define LED1_PORT				PORTC

#define TX_PIN					(1 << PC0)
#define TX_PIN_LOW()			PORTC &= ~TX_PIN
#define TX_PIN_HIGH()			PORTC |=  TX_PIN
#define TX_PIN_TOGGLE()			PORTC ^=  TX_PIN

#define PRELOAD_250US			0x06	/* = 250 ticks */

#define DATA_BITS				41
#define PREAMBLE_BITS			8
#define BITS_PER_BYTE			8
#define PACKET_LENGTH_BITS		(DATA_BITS + PREAMBLE_BITS)
#define PACKET_LENGTH_BYTES		(PACKET_LENGTH_BITS / BITS_PER_BYTE)
#define PACKET_COUNT			15

#define TX_IN_PROGRESS			(TCCR0B & 0x7)

#define START_TX() { \
	currentBit = 0; \
	currentByte = 0; \
	TX_PIN_LOW(); \
	TCCR0B = (2 << CS00); /* Clk/8 */ \
	TCNT0 = PRELOAD_250US; \
	TIFR0 = (1 << TOV0); \
}

#define STOP_TX() { \
	TCCR0B = 0; \
	TX_PIN_LOW(); \
}

#define PRELOAD_800US			0xFFE7
#define PREAMBLE_TICKS			20
#define BIT1_TICKS				14
#define BIT0_TICKS				7


volatile uint8_t txBuffer[PACKET_LENGTH_BYTES];
volatile uint8_t currentByte;
volatile uint8_t currentBit;

volatile uint8_t rxBuffer[PACKET_LENGTH_BYTES];
volatile uint8_t rxData[PACKET_LENGTH_BYTES];
volatile uint8_t currentRxByte;
volatile uint8_t currentRxBit;
volatile uint8_t rxComplete;
volatile uint8_t preambleCount;
volatile uint8_t currentRxByteValue;


ISR(TIMER1_OVF_vect)
{
	/* timeout occured */
	TCNT1 = PRELOAD_800US;
	TCCR1B |= (1 << ICES1);
	TIMSK1 |= (1 << ICIE1);
	preambleCount = 0;
	currentRxBit = 0;
	currentRxByte = 0;
	rxComplete = 0;	
	currentRxByteValue = 0;
}


void enqueueSample(const uint8_t bitValue)
{
	if (bitValue == 1)
	{		
		currentRxByteValue |= 1;
	}
	else if (bitValue == 0)
	{		
		currentRxByteValue &= ~1;
	}
	
	++currentRxBit;	
	if ((currentRxBit % BITS_PER_BYTE) == 0)
	{
		rxBuffer[currentRxByte] = currentRxByteValue;
		++currentRxByte;
	}
	else
	{
		currentRxByteValue <<= 1;
	}
	
	if (currentRxBit >= DATA_BITS)
	{
		if (rxComplete == 0)
		{		
			uint8_t i = PACKET_LENGTH_BYTES - 1;
			do
			{
				rxData[i] = rxBuffer[i];
			} while (i--);			
			rxComplete = 1;
		}
		currentRxBit = 0;
		currentRxByte = 0;
		preambleCount = 0;
	}
}


ISR(TIMER1_CAPT_vect)
{
	uint8_t sreg = SREG;
	uint16_t elapsedTimeTicks = ICR1 - PRELOAD_800US;
	
	/* Reset the timeout on any edge */
	TCNT1 = PRELOAD_800US;
	
	if (preambleCount < PREAMBLE_BITS && elapsedTimeTicks >= PREAMBLE_TICKS)
	{
		/* Preamble */
		++preambleCount;
	}
	else if (preambleCount == PREAMBLE_BITS && (TCCR1B & (1 << ICES1)) == 0)
	{		
		/* On falling edge, we fetch the measurement result */		
		if (elapsedTimeTicks >= BIT1_TICKS)
		{
			/* 1 Bit */
			enqueueSample(1);
		}
		else if (elapsedTimeTicks >= BIT0_TICKS)
		{
			/* 0 Bit */
			enqueueSample(0);
		}
	}
	
	/* Toggle input detection edge */
	TCCR1B ^= (1 << ICES1);
	
	SREG = sreg;
}


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
			/* Byte finished - load next one */
			if ((currentBit % BITS_PER_BYTE) == 0)
			{
				temp = txBuffer[currentByte];
				++currentByte;
			}
			
			currentBitValue = ((temp & 0x80) == 0x80);;
			temp = temp << 1;					
		}
		
		++currentBit;
		
		/*
		 * In case of preamble sequence this actually generates an alternating signal.
		 * In case of the data bits this generates the negative edge
		 */
		TX_PIN_TOGGLE();
	}
	else
	{
		/*
		 * 0 bit ... short pulse of 250 us followed by a 500 us gap
		 * 1 bit ... long pulse of 500 us followed by a 250 us gap
		 */
		if ((currentBitValue == 0 && cycle == 1) || (currentBitValue == 1 && cycle == 2))
		{
			TX_PIN_LOW();	
			if (currentBit >= PACKET_LENGTH_BITS)
			{
				currentBit = 0;
				STOP_TX();
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


inline int16_t centigradeToFahrenheit(const int16_t in)
{
	return (in * 18 / 10) + 320;
}


inline uint16_t fahrenheitToRaw(const int16_t in)
{
	return in + 900;
}


inline int16_t rawToFahrenheit(const uint16_t in)
{
	return in - 900;
}


inline int16_t fahrenheitToCentigrade(const int16_t in)
{
	return (in - 320) * 10 / 18;
}


void assemblePacket(const uint8_t id, const uint8_t batteryLow, const uint8_t test, const uint8_t channel, const int16_t temperature, const uint8_t humidity)
{	
	if ((channel == 0) || (humidity > 100) || (temperature < -677) || (temperature > 1590))
	{
		return;
	}
	
	uint16_t temperatureRaw = fahrenheitToRaw(centigradeToFahrenheit(temperature));
	
	txBuffer[0] = id;
	txBuffer[1] = ((batteryLow & 0x1) << 7) | ((test & 0x1) << 6) | ((channel & 0x3) << 4) | ((temperatureRaw >> 8) & 0xF);	
	txBuffer[2] = (temperatureRaw & 0x00FF);
	txBuffer[3] = humidity;
	txBuffer[4] = (txBuffer[0] + txBuffer[1] + txBuffer[2] + txBuffer[3]) % 256;
	txBuffer[5] = 0;
}


void decodePacket(volatile uint8_t *data)
{
	uint8_t check = (data[0] + data[1] + data[2] + data[3] - data[4]) % 256;
	if (check != 0)
	{
		Uart0SendString("CHKinv!\n");
		return;
	}
	
	uint8_t id = data[0];
	uint8_t batteryLow = ((data[1] >> 7) &  0x1);
	uint8_t test = ((data[1] >> 6) & 0x1);
	uint8_t channel = ((data[1] >> 4) & 0x3);
	int16_t temperature = fahrenheitToCentigrade(rawToFahrenheit((data[1] & 0xF) * 256 + data[2]));
	uint8_t humidity = data[3];
	
	Uart0SendValue(id);
	Uart0SendByte(' ');
	Uart0SendValue(batteryLow);
	Uart0SendByte(' ');
	Uart0SendValue(test);
	Uart0SendByte(' ');
	Uart0SendValue(channel);
	Uart0SendByte(' ');
	Uart0SendValue(temperature);
	Uart0SendByte(' ');
	Uart0SendValue(humidity);
	Uart0SendByte('\n');
}


void decodePacket2(const uint8_t *data, uint8_t *id, uint8_t *batteryLow, uint8_t *test, uint8_t *channel, int16_t *temperature, uint8_t *humidity)
{
	uint8_t check = (data[0] + data[1] + data[2] + data[3] - data[4]) % 256;
	if (check != 0)
	{
		return;
	}
	
	if (id)
	{
		*id = data[0];
	}
	
	if (batteryLow)
	{
		*batteryLow = ((data[1] >> 7) &  0x1);
	}
	
	if (test)
	{
		*test = ((data[1] >> 6) & 0x1);
	}
	
	if (channel)
	{
		*channel = ((data[1] >> 4) & 0x3);
	}
	
	if (temperature)
	{
		*temperature = fahrenheitToCentigrade(rawToFahrenheit(data[1] & 0xF) * 256 + data[2]);
	}
	
	if (humidity)
	{
		*humidity = data[3];		
	}
}


int main(void)
{
	uint8_t packetCount = 0;
	uint8_t id = 232; // currently registered ;-) rand() % 255;
	uint8_t batteryLow = 0;
	uint8_t testButtonPressed = 1;
	uint8_t channel = 2;
	uint8_t humidity = 55;
	uint16_t temperature = 222;
	
	DDRB |= LED0_PIN;
	DDRC |= TX_PIN | LED1_PIN;
	
	TIMSK0 = (1 << TOIE0);
	
	TCNT1 = PRELOAD_800US;
	TCCR1B = (1 << ICES1) | (4 << CS10); /* Clk/256 */
	TIMSK1 = (1 << TOIE1); /* Intentionally leave Input Capture Interrupt unset to trigger an initialization timeout */
	
	EnableSerialDebugging();
	
	sei();	
	
    while (1) 
    {		
		if (packetCount > 0 && !TX_IN_PROGRESS)
		{
			START_TX();
			--packetCount;
		}
		
		if (packetCount == 0 && SW0_PRESSED)
		{
			assemblePacket(id, batteryLow, testButtonPressed, channel, temperature, humidity);
			packetCount = PACKET_COUNT;
			_delay_ms(100);
		}
		
		if (rxComplete)
		{
			for (uint8_t i = 0; i < PACKET_LENGTH_BYTES; i++)
			{
				Uart0SendByteHex(rxData[i]);
			}
			Uart0SendByte('\n');
			decodePacket(rxData);
			rxComplete = 0;
		}
    }
	
	return 0;
}

#endif
