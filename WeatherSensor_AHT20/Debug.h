/*
 * Debug.h
 *
 * Created: 13.03.2019 21:41:25
 *  Author: pe-jot
 */ 

#pragma once

#include "config.h"

#define BAUD_RATE						115200UL
#define USE_CLK2X						1

#define BAUD_VALUE 						((64UL * (F_CPU_UART)) / ((USE_CLK2X ? 8UL : 16UL) * (BAUD_RATE)))

#if BAUD_VALUE < 64 || BAUD_VALUE > 65535
#  error "Achieved baud rate invalid!"
#endif

#ifdef ENABLE_DEBUG
  #define DEBUG_TEXT					debug.sendText
  #define DEBUG_BYTE					debug.sendByte
  #define DEBUG_VALUE					debug.sendValue
#else
  #define DEBUG_TEXT					(void)
  #define DEBUG_BYTE					(void)
  #define DEBUG_VALUE					(void)
#endif

class SerialDebugging
{
public:
	SerialDebugging();
	
	void begin();
	void sendByte(const uint8_t character);
	void sendText(const char* text);
	void sendValue(const int16_t value);
};
