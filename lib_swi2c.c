/******************************************************************************
* lib_GPIOCTRL
* A runtime-capable GPIO Library, with Digital Read/Write and
* TODO: Analog Read & Analog Write/PWM
*
*
* See GitHub for details: https://github.com/ADBeta/CH32V003_lib_GPIOCTRL
*
* ADBeta (c) 2024
******************************************************************************/
#include "lib_swi2c.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*** Copied from lib_GPIOCTRL ************************************************/
// https://github.com/ADBeta/CH32V003_lib_GPIOCTRL

/*** GPIO Pin Mode Enumeration ***********************************************/
/// @breif GPIO Pin Mode data. The lower nibble is the raw binary data for the
/// [R32_GPIOx_CFGLR] Register. The upper nibble is used for additional flags 
typedef enum {
	INPUT_ANALOG       = 0x00,
	INPUT_FLOATING     = 0x04,
	// NOTE: Removed to simplify code (speed)
	// Mapped to INPUT_PP, Sets OUTDR based on the upper nibble
	// INPUT_PULLUP       = 0x18,
	// INPUT_PULLDOWN     = 0x08,
	//
	OUTPUT_10MHZ_PP    = 0x01,
	OUTPUT_10MHZ_OD    = 0x05,
	//
	OUTPUT_2MHZ_PP     = 0x02,
	OUTPUT_2MHZ_OD     = 0x06,
} gpio_mode_t;


/*** GPIO Output State Enumerations ******************************************/
/// @breif GPIO Pin State Enum, simple implimentation of a HIGH/LOW System
typedef enum {
	GPIO_LOW     = 0x00,
	GPIO_HIGH    = 0x01,
} gpio_state_t;


/*** Registers for GPIO Port *************************************************/
/// @breif GPIO Port Register, Directly Maps to Memory starting at 
/// [R32_GPIOx_CFGLR] for each PORT Respectively
typedef struct {
	volatile uint32_t CFGLR;  // Configuration Register (lower)
	volatile uint32_t CFGHR;  // Configuration Register (upper)
	volatile uint32_t INDR;   // Input Data Register
	volatile uint32_t OUTDR;  // Output Data Register
	volatile uint32_t BSHR;   // Set/Reset Register
	volatile uint32_t BCR;    // Port Reset Register
	volatile uint32_t LCKR;   // Lock Register
} gpio_port_reg_t;


/*** Register Address Definitions ********************************************/
#define RCC_APB2PCENR ((volatile uint32_t *)0x40021018)
#define APB2PCENR_AFIO   0x01
#define APB2PCENR_IOPxEN 0x04

#define PORTA_GPIO_REGISTER_BASE 0x40010800
// NOTE: PORTB is not available for the CH32V003.
#define PORTB_GPIO_REGISTER_BASE 0x40010C00
#define PORTC_GPIO_REGISTER_BASE 0x40011000
#define PORTD_GPIO_REGISTER_BASE 0x40011400

#define GPIO_PORTA ((gpio_port_reg_t *)PORTA_GPIO_REGISTER_BASE)
// NOTE: PORTB is not available for the CH32V003.
#define GPIO_PORTB ((gpio_port_reg_t *)PORTB_GPIO_REGISTER_BASE)
#define GPIO_PORTC ((gpio_port_reg_t *)PORTC_GPIO_REGISTER_BASE)
#define GPIO_PORTD ((gpio_port_reg_t *)PORTD_GPIO_REGISTER_BASE)

/// @breif The GPIO Ports are places into an array for easy indexing in the
/// GPIO Functions
/// NOTE: Only 3 PORTs are usable in the CH32V003, 4 for other MCUs
static gpio_port_reg_t *gpio_port_reg[4] = {
	GPIO_PORTA,
	NULL,
	GPIO_PORTC,
	GPIO_PORTD,
};


/// @breif Sets the OUTDR Register for the passed Pin
/// @param gpio_pin_t pin, the GPIO Pin & Port Variable (e.g GPIO_PD6)
/// @param gpio_state_t state, GPIO State to be set (e.g GPIO_HIGH)
/// @return None
__attribute__((always_inline))
static inline void gpio_digital_write(const gpio_pin_t pin, const gpio_state_t state)
{
	// Make array of uint8_t from [pin] enum. See definition for details
	uint8_t *byte = (uint8_t *)&pin;

	if(state == GPIO_HIGH)
		gpio_port_reg[ byte[0] ]->OUTDR |=  (0x01 << byte[1]);
	if(state == GPIO_LOW)
		gpio_port_reg[ byte[0] ]->OUTDR &= ~(0x01 << byte[1]);
}

/// @breif Reads the INDR Register of the specified pin and returns state
/// @param gpio_pin_t pin, the GPIO Pin & Port Variable (e.g GPIO_PD6)
/// @return gpio_state_t, the current state of the pin, (e.g GPIO_HIGH)
__attribute__((always_inline))
static inline gpio_state_t gpio_digital_read(const gpio_pin_t pin)
{
	// Make array of uint8_t from [pin] enum. See definition for details
	uint8_t *byte = (uint8_t *)&pin;

	// If the Input Reg has the wanted bit set, return HIGH
	if( (gpio_port_reg[ byte[0] ]->INDR & (0x01 << byte[1])) != 0x00 ) 
		return GPIO_HIGH;

	// else return LOW 
	return GPIO_LOW;
}

/// NOTE: Modified to be slimmer and lightweight
/// @breif Sets the Config and other needed Registers for a passed pin and mode
/// @param gpio_pin_t pin, the GPIO Pin & Port Variable (e.g GPIO_PD6)
/// @param gpio_mode_t mode, the GPIO Mode Variable (e.g OUTPUT_10MHZ_PP)
/// @return None
__attribute__((always_inline))
static inline void gpio_set_mode(const gpio_pin_t pin, const gpio_mode_t mode)
{
	// Make array of uint8_t from [pin] enum. See definition for details
	uint8_t *byte = (uint8_t *)&pin;
	
	// Set the RCC Register to enable clock on the specified port
	*RCC_APB2PCENR |= (APB2PCENR_AFIO | (APB2PCENR_IOPxEN << byte[0]));

	// Clear then set the GPIO Config Register
	gpio_port_reg[ byte[0] ]->CFGLR &=        ~(0x0F  << (4 * byte[1]));
	gpio_port_reg[ byte[0] ]->CFGLR |=           mode << (4 * byte[1]);
}


/*** Software I2C Functions **************************************************/
// Asserting I2C lines is when they are OUTPUT Pulling LOW
#define ASSERT_SCL gpio_set_mode(i2c->pin_scl, OUTPUT_10MHZ_PP); 
#define ASSERT_SDA gpio_set_mode(i2c->pin_sda, OUTPUT_10MHZ_PP);

// Releasing I2C lines is setting them INPUT FLOATING, Pulled HIGH Externally
#define RELEASE_SCL gpio_set_mode(i2c->pin_scl, INPUT_FLOATING); 
#define RELEASE_SDA gpio_set_mode(i2c->pin_sda, INPUT_FLOATING);

/*** Helper Functions ********************************************************/
// Waits for the calculated amount of time (Limits bus speed)
// TODO:
__attribute__((always_inline)) static inline void wait()
{

}


static i2c_err_t clk_stretch(const gpio_pin_t scl)
{
	uint8_t clock_waits = 10;
	while(gpio_digital_read(scl) == GPIO_LOW)
	{
		if(!clock_waits--) return I2C_ERR_TIMEOUT;
		wait();
	}

	return I2C_OK;
}

static i2c_err_t master_tx_bit(const i2c_bus_t *i2c, const bool bit)
{
	// Set the Data line depending on bit
	if(bit)
	{
		RELEASE_SDA;
	} else {
		ASSERT_SDA;
	}

	wait();
	RELEASE_SCL;   // SCL HIGH
	wait();

	// Clock stretch, wait for SCL to go LOW
	i2c_err_t stat = clk_stretch(i2c->pin_scl);
	ASSERT_SCL;   // SCL LOW
	
	// I2C_OK if successful, propegates error if not
	return stat;
}

static bool master_rx_bit(const i2c_bus_t *i2c)
{
	bool bit = 0;
	// Release the SDA pin so the slave can set data, then release SCL
	// to request data
	RELEASE_SDA;
	RELEASE_SCL;

	// Wait for clk stretch, Only read pin if it's OK
	if(clk_stretch(i2c->pin_scl) == I2C_OK)
	{
		bit = (bool)gpio_digital_read(i2c->pin_sda);
	}

	ASSERT_SCL; // SCL LOW
	return bit;
}

/*** Library Functions *******************************************************/
i2c_err_t swi2c_init(i2c_bus_t *i2c)
{
	// Set the Output register to LOW (Pulldown in output)
	gpio_digital_write(i2c->pin_scl, GPIO_LOW);
	gpio_digital_write(i2c->pin_sda, GPIO_LOW);

	RELEASE_SCL;
	RELEASE_SDA;
	
	// Set STOP Condition to get the bus into a known state
	return swi2c_stop(i2c);
}


i2c_err_t swi2c_start(i2c_bus_t *i2c)
{
	// START Condition is SDA Going LOW while SCL is HIGH
	i2c_err_t stat = I2C_OK;

	// If Bus is active, do a repeated START
	if(i2c->_active)
	{
		RELEASE_SDA;    // SDA HIGH
		wait();
		RELEASE_SCL;    // SCL HIGH
		if( (stat = clk_stretch(i2c->pin_scl)) != I2C_OK) return stat;
	}

	// START Condition
	ASSERT_SDA;        // SDA LOW
	ASSERT_SCL;        // SCL LOW
	
	// Mark the I2C Bus as Active
	i2c->_active = true;
	return stat;
}


i2c_err_t swi2c_stop(i2c_bus_t *i2c)
{
	// Stop condition is defined by SDA going HIGH while SCL is HIGH
	i2c_err_t stat = I2C_OK;

	// Pull SDA LOW to make sure Conditions are met
	ASSERT_SDA;     // SDA LOW 
	wait();
	RELEASE_SCL;    // SCL HIGH
	stat = clk_stretch(i2c->pin_scl);
	
	// Set SDA HIGH while SCL is HIGH
	RELEASE_SDA;    // SDA HIGH
	wait();
	
	// If SDA is not what we expect, something went wrong
	// TODO:
	
	i2c->_active = false;
	return stat;
}

i2c_err_t swi2c_master_tx_byte(i2c_bus_t *i2c, uint8_t data)
{
	i2c_err_t stat = I2C_OK;

	// Transmit bits MSB First
	uint8_t index = 8;
	while(index--)
	{
		master_tx_bit(i2c, data & 0x80);
		data = data << 1;
	}
	
	// Read ACK bit (0 = ACK, 1 = NACK)
	if( (stat = master_rx_bit(i2c)) == I2C_NACK) stat = I2C_ERR_NACK;
	return stat;
}

uint8_t swi2c_master_rx_byte(i2c_bus_t *i2c, bool ack)
{
	// Read bits MSB First
	uint8_t index = 8;
	uint8_t byte = 0x00;
	while(index--) byte = (byte << 1) | master_rx_bit(i2c);
	
	// Write ACK Bit, ACK (0) = Read More,  NACK (1) = Stop Reading
	master_tx_bit(i2c, ack);
	return byte;
}






void swi2c_scan(i2c_bus_t *i2c)
{
	// Scan through all possible addresses
	for(uint8_t indx = 0; indx < 128; indx++)
	{
		uint8_t addr = indx << 1;

		swi2c_start(i2c);
		if(swi2c_master_tx_byte(i2c, addr) == I2C_OK) 
			printf("Device 0x%02X Reponded\n", addr);
		swi2c_stop(i2c);
	}
}

i2c_err_t swi2c_master_transmit(i2c_bus_t *i2c, 
     const uint8_t addr, const uint8_t reg, const uint8_t *data, uint16_t size)
{
	// Catch any bad inputs
	if(i2c == NULL || data == NULL || size == 0) return I2C_ERR_INVALID_ARGS;

	i2c_err_t stat = I2C_OK;

	// Gaurd each step from failure
	// Send START Condition and address byte
	if( (stat = swi2c_start(i2c)) == I2C_OK && 
		(stat = swi2c_master_tx_byte(i2c, addr)) == I2C_OK)
	{
		swi2c_master_tx_byte(i2c, reg);
		while(size)
		{
			swi2c_master_tx_byte(i2c, *data);
			++data;
			--size;
		}
	}

	swi2c_stop(i2c);
	return stat;
}

i2c_err_t swi2c_master_receive(i2c_bus_t *i2c, 
           const uint8_t addr, const uint8_t reg, uint8_t *data, uint16_t size)
{
	// Catch any bad inputs
	if(i2c == NULL || data == NULL || size == 0) return I2C_ERR_INVALID_ARGS;

	i2c_err_t stat = I2C_OK;

	// Gaurd each step from failure
	// Send START Condition and address byte
	if( (stat = swi2c_start(i2c)) == I2C_OK && 
		(stat = swi2c_master_tx_byte(i2c, addr)) == I2C_OK)
	{
		// Sed the Register Byte
		swi2c_master_tx_byte(i2c, reg);

		// Repeat the START Condition
		swi2c_start(i2c);
		// Send address in Read Mode
		swi2c_master_tx_byte(i2c, addr | 0x01);

		while(--size >= 1)
		{
			*data = swi2c_master_rx_byte(i2c, I2C_ACK);
			++data;
		}

		// Last byte read has NACK bit set
		*data = swi2c_master_rx_byte(i2c, I2C_NACK);
	}

	swi2c_stop(i2c);
	return stat;
}


