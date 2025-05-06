#ifndef DEBUG_H
#define DEBUG_H

#include "config.h"

#define BAUD 250000

class SerialDebugging
{
public:
	SerialDebugging();
	
	void begin();
	void sendByte(const uint8_t character);
	void sendText(const char* text);
	void sendValue(const int16_t value);
};

#endif /* DEBUG_H */
