#ifndef _SERIAL_DEBUG_H_
#define _SERIAL_DEBUG_H_

#include <stdlib.h>

#define BAUD 250000
#include <util/setbaud.h>

// Note: mEDBG seems to need DTR set to forward data to the console window!


static void Uart0SendByte (uint8_t c)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}


void Uart0SendString (const char* txt)
{
	while (*txt > 0)
	{
		Uart0SendByte(*txt);
		txt++;
	}
}


#define TRACE(_MESSAGE_) Uart0SendString(_MESSAGE_);


void Uart0SendValue(int16_t value)
{
	char buffer[7];
	itoa(value, buffer, 10);
	Uart0SendString(buffer);
}


void Uart0SendByteHex(uint8_t value)
{
	char buffer[3];
	itoa(value, buffer, 16);
	Uart0SendString(buffer);
	Uart0SendByte(' ');
}


//#define UART_RX_TX
static void EnableSerialDebugging (void)
{
	UCSR0A = (USE_2X << U2X0);
#ifdef UART_RX_TX
	UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
#else
	UCSR0B = (1 << TXEN0);
#endif
	UCSR0C |= (0x3 << UCSZ00);
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
	
	Uart0SendByte(0);
}

#endif /* _SERIAL_DEBUG_H_ */
