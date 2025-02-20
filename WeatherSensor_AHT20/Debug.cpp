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


void SerialDebugging::waitForBufferEmpty()
{
	while (!(USART0.STATUS & USART_DREIF_bm));
}


void SerialDebugging::waitForTransmitComplete()
{
	while (!(USART0.STATUS & USART_TXCIF_bm));
	USART0.STATUS |= USART_TXCIF_bm;
}


void SerialDebugging::sendByte(const uint8_t character)
{
	waitForBufferEmpty();
	USART0.TXDATAL = character;
}


void SerialDebugging::sendText(const char* text)
{
	while (*text > 0)
	{
		sendByte(*text);
		text++;
	}
	waitForTransmitComplete();
}


void SerialDebugging::sendValue(const int16_t value)
{
	char buffer[7];
	itoa(value, buffer, 10);
	sendText(buffer);
	waitForTransmitComplete();
}


void SerialDebugging::begin()
{
#ifdef USE_ALTERNATE_USART_PORT
	PORTMUX.CTRLB |= PORTMUX_USART0_ALTERNATE_gc;
	PORTA.DIRSET = PIN1_bm;
#else
	PORTB.DIRSET = PIN2_bm;	// Unsure whether this is actually necessary or not
#endif
	USART0.BAUD = BAUD_VALUE;
	USART0.CTRLC = USART_CHSIZE_8BIT_gc;
	USART0.CTRLB = USART_RXEN_bm | USART_TXEN_bm | (USE_CLK2X ? USART_RXMODE_CLK2X_gc : USART_RXMODE_NORMAL_gc);
}
