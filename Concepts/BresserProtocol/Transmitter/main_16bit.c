#ifdef _USE_16BIT_TIMER_

/*
 * CPU = ATmega328PB
 * CLK = 8 MHz
 */ 

#define F_CPU					8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#define SW0_PIN					(1 << PB7)
#define SW0_PRESSED				((PINB & SW0_PIN) == 0)

#define LED0_PIN				(1 << PB5)
#define LED1_PIN				(1 << PC5)

#define TX_PIN					(1 << PC0)
#define TX_PIN_LOW()			PORTC &= ~TX_PIN;
#define TX_PIN_HIGH()			PORTC |=  TX_PIN;
#define TX_PIN_TOGGLE()			PORTC ^=  TX_PIN;

#define PRELOAD_750US			0xE890	/* = 6000 ticks */
#define PRELOAD_500US			0xF830	/* = 4000 ticks */
#define PRELOAD_250US			0xF060	/* = 2000 ticks */

#define DATA_BITS				41
#define PREAMBLE_BITS			8
#define BITS_PER_BYTE			8
#define PACKET_LENGTH_BITS		(DATA_BITS + PREAMBLE_BITS)
#define PACKET_LENGTH_BYTES		(PACKET_LENGTH_BITS / BITS_PER_BYTE)
#define PACKET_COUNT			15

#define TX_IN_PROGRESS			(TCCR1B & 0x7)

#define START_TX() { \
	currentBit = 0; \
	currentByte = 0; \
	TX_PIN_LOW(); \
	TCCR1B = (1 << CS10); \
	TCNT1 = PRELOAD_750US; \
	OCR1A = 0xFF; \
	TIFR1 = (1 << OCF1A) | (1 << TOV1); \
}

#define STOP_TX() { \
	TCCR1B = 0; \
	TX_PIN_LOW(); \
}


volatile uint8_t txBuffer[PACKET_LENGTH_BYTES];
volatile uint8_t currentByte;
volatile uint8_t currentBit;


ISR(TIMER1_COMPA_vect)
{
	TX_PIN_LOW();
	if (currentBit >= PACKET_LENGTH_BITS)
	{
		currentBit = 0;
		STOP_TX();
	}
}


ISR(TIMER1_OVF_vect)
{
	static uint8_t temp = 0;
	uint8_t sreg = SREG;
	
	TCNT1 = PRELOAD_750US;
		
	if (currentBit < PREAMBLE_BITS)
	{
		OCR1A = 0xFF;
	}
	else
	{
		/* Byte finished - load next one */
		if ((currentBit % BITS_PER_BYTE) == 0)
		{
			temp = txBuffer[currentByte];
			++currentByte;
		}
		
		/*
		 * 0 bit ... short pulse of 250 us followed by a 500 us gap
		 * 1 bit ... long pulse of 500 us followed by a 250 us gap
		 */
		OCR1A = (temp & 0x80) ? PRELOAD_500US : PRELOAD_250US;
		temp = temp << 1;					
	}
	
	++currentBit;
	
	/*
	 * In case of preamble sequence this actually generates an alternating signal.
	 * In case of the data bits this generates the negative edge
	 */
	TX_PIN_TOGGLE();
	SREG = sreg;
}


inline int16_t centigradeToFahrenheit(int16_t in)
{
	return (in * 18 / 10) + 320;
}


inline uint16_t fahrenheitToRaw(int16_t in)
{
	return in + 900;
}


void assemblePacket(uint8_t id, uint8_t batteryLow, uint8_t test, uint8_t channel, int16_t temperature, uint8_t humidity)
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


int main(void)
{
	uint8_t packetCount = PACKET_COUNT;
	uint8_t id = 232; // currently registered ;-) rand() % 255;
	uint8_t batteryLow = 0;
	uint8_t testButtonPressed = 1;
	uint8_t channel = 2;
	uint8_t humidity = 55;
	uint16_t temperature = 222;
	
	DDRB |= LED0_PIN;
	DDRC |= TX_PIN | LED1_PIN;
	
	TIMSK1 = (1 << OCIE1A) | (1 << TOIE1);
	
	sei();
	
	assemblePacket(id, batteryLow, testButtonPressed, channel, temperature, humidity);
	
    while (1) 
    {
		while (packetCount)
		{
			START_TX();
			while (TX_IN_PROGRESS);
			--packetCount;
		}
		
		if (SW0_PRESSED)
		{
			packetCount = PACKET_COUNT;
			_delay_ms(100);
		}
    }
	
	return 0;
}

#endif /* _USE_16BIT_TIMER_ */
