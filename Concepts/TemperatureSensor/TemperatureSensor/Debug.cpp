#include <avr/io.h>
#include "Debug.h"
#include <util/setbaud.h>
#include <stdlib.h>


SerialDebugging::SerialDebugging()
{}


void SerialDebugging::sendByte(const uint8_t character)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = character;
}


void SerialDebugging::sendText(const char* text)
{
	while (*text > 0)
	{
		sendByte(*text);
		text++;
	}
}


void SerialDebugging::sendValue(const int16_t value)
{
	char buffer[7];
	itoa(value, buffer, 10);
	sendText(buffer);
}


void SerialDebugging::begin()
{
    UCSR0A = (USE_2X << U2X0);
    UCSR0B = (0 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (0x0 << UMSEL00) | (0x0 << UPM00) | (0 << USBS0) | (0x3 << UCSZ00) ;
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
}
