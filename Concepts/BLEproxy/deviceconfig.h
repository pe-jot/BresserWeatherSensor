#pragma once

#define _DEBUG_TRACE_

#define STRINGIFY(_s)				STRGFY(_s)
#define STRGFY(_s)					#_s
#define TOBOOL(_b)					_b ? "true" : "false"

#ifdef _DEBUG_TRACE_
  #define DebugPRINT				Serial.print
  #define DebugPRINTLN				Serial.println
  #define DebugTRACE(_message)		{ Serial.print("("); Serial.print(__FUNCTION__); Serial.print(" L"); Serial.print(__LINE__); Serial.print(") "); Serial.print(_message); }
#else
  #define DebugPRINT
  #define DebugPRINTLN
  #define DebugTRACE
#endif

// Board: Arduino Nano 33 IoT
#define DEVICE_NAME					"nano33iot"
#define SOFTWARE_NAME				"ArduinoAprsBeacon 0.1"

#define SHELLY_BLUHT_ADDRESS		"7c:c6:b6:61:e3:a0"

#define PWR_LED_PIN					12			/* Re-pinned */
#define USER_LED_PIN				13			/* Default */

#define DEBUG_BAUDRATE				115200

#define PowerLED(_state_)			digitalWrite(PWR_LED_PIN, _state_)
#define UserLED(_state_)			digitalWrite(USER_LED_PIN, _state_)

enum LedState {
	OFF = 0,
	ON = 1
};
