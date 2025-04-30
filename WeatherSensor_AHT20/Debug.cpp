/*
 * Debug.h
 *
 * Created: 06.01.2019 13:06:01
 *  Author: pe-jot
 */ 

#include <avr/io.h>
#include "Debug.h"
#include <stdlib.h>


SerialDebugging::SerialDebugging()
{}


void SerialDebugging::sendByte(const uint8_t character)
{
	// Wait for data register empty flag
	while ((USART0.STATUS & USART_DREIF_bm) == 0);
	USART0.TXDATAL = character;
}


void SerialDebugging::sendText(const char* text)
{
	while (*text > 0)
	{
		sendByte(*text);
		text++;
	}
	
	// Wait for transmit complete flag (in order to make sure transmission is complete before entering a sleep)
	// Is set when the entire frame in the Transmit Shift register has been shifted out, and there is no new data in the transmit buffer.
	// Needs to be explicitly cleared.
	while ((USART0.STATUS & USART_TXCIF_bm) == 0);
	USART0.STATUS |= USART_TXCIF_bm;
}


void SerialDebugging::sendValue(const int16_t value)
{
	char buffer[7];
	itoa(value, buffer, 10);
	sendText(buffer);
}


void SerialDebugging::sendHexValue(const uint32_t value)
{
	char buffer[9];
	itoa(value, buffer, 16);
	sendText(buffer);
}


void SerialDebugging::begin()
{
	// Port mapping verified for ATtiny816
#ifdef USE_ALTERNATE_USART_PORT
	PORTMUX.CTRLB |= PORTMUX_USART0_ALTERNATE_gc;
	PORTA.DIRSET |= PIN1_bm;
#else
	PORTB.DIRSET |= PIN2_bm;
#endif
	USART0.BAUD = BAUD_VALUE;
	USART0.CTRLC = USART_CHSIZE_8BIT_gc;
	USART0.CTRLB = USART_RXEN_bm | USART_TXEN_bm | (USE_CLK2X ? USART_RXMODE_CLK2X_gc : USART_RXMODE_NORMAL_gc);
}
