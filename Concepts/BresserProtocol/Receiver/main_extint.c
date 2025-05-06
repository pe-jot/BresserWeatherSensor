#ifdef _USE_EXTERNAL_INTERRUPT_

/* !!! Sketch-up === not working !!! */

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

#define FALLING_EDGE_INT		0x2
#define RISING_EDGE_INT			0x3


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
	/* timeout occurred */
	TCNT1 = PRELOAD_800US;	
	EICRA = (RISING_EDGE_INT << ISC00);
	EIMSK |= (1 << INT0);
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
		uint8_t i = PACKET_LENGTH_BYTES - 1;
		do 
		{
			rxData[i] = rxBuffer[i];
		} while (i--);
		
		rxComplete = 1;
		currentRxBit = 0;
		currentRxByte = 0;
		preambleCount = 0;
	}
}


ISR(INT0_vect)
{
	uint8_t sreg = SREG;
	uint16_t elapsedTimeTicks = TCNT1 - PRELOAD_800US;
		
	/* Reset the timeout on any edge */
	TCNT1 = PRELOAD_800US;
	
	Uart0SendValue(elapsedTimeTicks);
	Uart0SendByte('\n');
	
	if (preambleCount < PREAMBLE_BITS && elapsedTimeTicks >= PREAMBLE_TICKS)
	{
		LED_TOGGLE(LED0_PIN, LED0_PORT);
		/* Preamble */
		++preambleCount;
	}
	else if (preambleCount == PREAMBLE_BITS && (TCCR1B & (1 << ICES1)) == 0)
	{
		LED_TOGGLE(LED1_PIN, LED1_PORT);
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
	
	/* Toggle interrupt edge bit */	
	EICRA ^= (1 << ISC00);
	
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


void decodePacket(const uint8_t *data)
{
	uint8_t id = data[0];
	
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
	TIMSK1 = (1 << TOIE1);
	/* Intentionally leave EXTINT unset to trigger an initialization timeout */
	
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
			rxComplete = 0;
		}
    }
	
	return 0;
}

#endif
