#ifndef CONFIG_H_
#define CONFIG_H_

#define F_CPU					8000000UL

#define SW0_PIN					(1 << PB7)
#define SW0_PRESSED				((PINB & SW0_PIN) == 0)

#define LED0_PIN				(1 << PB5)
#define LED1_PIN				(1 << PC5)

#define TX_PIN					(1 << PC0)
#define TX_PIN_LOW()			PORTC &= ~TX_PIN;
#define TX_PIN_HIGH()			PORTC |=  TX_PIN;
#define TX_PIN_TOGGLE()			PORTC ^=  TX_PIN;

// Using TWI1 => replacements for twi library
#define TWI_PORT				PORTE
#define SDA_PIN					PE0
#define SCL_PIN					PE1
#define TWI_vect				TWI1_vect
#define TWAR					TWAR1
#define TWBR					TWBR1
#define TWCR					TWCR1
#define TWDR					TWDR1
#define TWSR					TWSR1

#endif