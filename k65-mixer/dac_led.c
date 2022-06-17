/*
 * MCP4822 DAC / LED Shift Register Driver
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 * Hardware:
 *	RC9			- MIDI DAC CS				- output - active low / !SS2
 *	RC4			- MIDI LED CS				- output - active low
 *	RC6			- MIDI DAC LED MOSI			- SDO2
 *	RB15		- MIDI DAC LED SCK			- SCK2
 *
 */
#include <plib.h>
#include "dac_led.h"

// hardware defines
#define DAC_CS LATCbits.LATC9
#define LED_CS LATCbits.LATC4

// init the DAC module
void dac_led_init(void) {
	// chip select lines
	PORTSetPinsDigitalOut(IOPORT_C, BIT_4 | BIT_9);
	DAC_CS = 1;
	LED_CS = 1;

	PPSUnLock;
	PPSOutput(3, RPC6, SDO2);
	PPSLock;

	// DAC - SPI1
	SpiChnOpen(SPI_CHANNEL2, SPI_OPEN_MSTEN | SPI_OPEN_CKP_HIGH | \
		SPI_OPEN_MODE32, 8);	

	// zero the outputs
	dac_led_write_dac(0, 0x7ff);
	dac_led_write_dac(1, 0x7ff);
}

// write to the DAC
void dac_led_write_dac(unsigned char chan, unsigned int val) {
	unsigned int data = 0x30000000;
	if(chan & 0x01) data = 0xb0000000;
	data |= (val & 0xfff) << 16;
	// write to the DAC
	DAC_CS = 0;
	SpiChnPutC(SPI_CHANNEL2, data);
	while(SpiChnIsBusy(SPI_CHANNEL2)) ClearWDT();
	DAC_CS = 1;
}

// write to the LEDs
void dac_led_write_led(unsigned int val) {
	// write to the LEDs
	LED_CS = 0;
	SpiChnPutC(SPI_CHANNEL2, val & 0xff);
	while(SpiChnIsBusy(SPI_CHANNEL2)) ClearWDT();
	LED_CS = 1;
}