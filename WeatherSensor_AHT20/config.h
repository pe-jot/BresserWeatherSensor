/*
 * Device:	ATtiny816
 * Board:	adafruit ATtiny816 Breakout
 *
 * Pinout:		  ____
 *	(PA04)	0	-|    |- VIN
 *	(PA05)	1	-|    |- 3V3
 *	(PA06)	2	-|    |- GND
 *	(PA07)	3	-|    |- 16		(PA03)
 *	(PB5)	4	-|    |- 15		(PA02)
 *	(PB4)	5	-|    |- 14		(PA01)
 *	(PB3)	RXD	-|    |- UPDI
 *	(PB2)	TXD	-|    |- 13		(PC3)
 *	(PB1)	SDA	-|    |- 12		(PC2)
 *	(PB0)	SCL	-|____|- 11		(PC1)
 *
 *	(PC0)	LED (Onboard)
 */ 

#pragma once

#define SCL_BIT							PIN0_bm
#define SDA_BIT							PIN1_bm

#define LED_BIT							PIN0_bm
#define LED_OFF()						PORTC.OUTCLR = LED_BIT
#define LED_ON()						PORTC.OUTSET = LED_BIT
#define LED_TOGGLE()					PORTC.OUTTGL = LED_BIT

#define TXPWR_BIT						PIN4_bm 
#define TXPWR_OFF()						PORTA.OUTCLR = TXPWR_BIT
#define TXPWR_ON()						PORTA.OUTSET = TXPWR_BIT

#define TXPWR_GND_BIT					PIN5_bm

#define TX_PIN_BIT						PIN6_bm
#define TX_PIN_LOW()					PORTA.OUTCLR = TX_PIN_BIT
#define TX_PIN_HIGH()					PORTA.OUTSET = TX_PIN_BIT
#define TX_PIN_TOGGLE()					PORTA.OUTTGL = TX_PIN_BIT

#define F_CPU_FULLSPEED					5000000UL
#define F_CPU							F_CPU_FULLSPEED
#define F_CPU_TWI						F_CPU_FULLSPEED	/* Desired CPU clock during TWI operation */
#define F_CPU_USART						F_CPU_FULLSPEED /* Desired CPU clock during USART operation */

#define CONFIGURE_OUTPUTS() { \
	PORTA.DIRSET = TXPWR_BIT | TXPWR_GND_BIT | TX_PIN_BIT; \
	PORTA.OUTCLR = TXPWR_BIT | TXPWR_GND_BIT | TX_PIN_BIT; \
	PORTC.DIRSET = LED_BIT; \
	PORTC.OUTCLR = LED_BIT; \
}

#define CONFIGURE_TWI_IO() { \
	PORTB.PIN0CTRL = PORT_PULLUPEN_bm; \
	PORTB.PIN1CTRL = PORT_PULLUPEN_bm; \
}

#define CONFIGURE_IOPORTS() { \
	CONFIGURE_OUTPUTS(); \
	CONFIGURE_TWI_IO(); \
}

#define UNLOCK_PROTECTED_REGISTERS()	CCP = CCP_IOREG_gc
