/*
 * MCP4822 DAC Driver
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 * Hardware:
 *	RB10		- !MOD DAC CS0				- output - active low
 *	RB11		- !MOD DAC CS1				- output - active low
 *	RB13/RPB13	- MOD DAC MOSI				- SD01
 *	RB14/RPB14	- MOD DAC SCK				- SCK1
 *
 */
#include <plib.h>
#include "dac.h"

// hardware defines
#define DAC_CS0 LATBbits.LATB10
#define DAC_CS1 LATBbits.LATB11

unsigned int reverse(register unsigned int);

// init the DAC module
void dac_init(void) {
	// chip select lines
	PORTSetPinsDigitalOut(IOPORT_B, BIT_10);
	PORTSetPinsDigitalOut(IOPORT_B, BIT_11);
	DAC_CS0 = 1;
	DAC_CS1 = 1;

	PPSUnLock;
	PPSOutput(3, RPB13, SDO1);
	PPSLock;

	// DAC - SPI1
	SpiChnOpen(SPI_CHANNEL1, SPI_OPEN_MSTEN | SPI_OPEN_CKP_HIGH | \
		SPI_OPEN_MODE32, 8);	

	// zero the outputs
	dac_write_dac(0, DAC_VAL_OFF);
	dac_write_dac(1, DAC_VAL_OFF);
	dac_write_dac(2, DAC_VAL_OFF);
	dac_write_dac(3, DAC_VAL_OFF);
}

// write to the DAC
void dac_write_dac(unsigned char chan, unsigned int val) {
	unsigned int data = 0x30000000;
	if(chan & 0x01) data = 0xb0000000;
	data |= (val & 0xfff) << 16;

	// wait until SPI is ready
	while(SpiChnIsBusy(SPI_CHANNEL1)) ClearWDT();
	DAC_CS0 = 1;
	DAC_CS1 = 1;

	// second DAC unit
	if(chan & 0x02) {
		// write to the DAC
		DAC_CS1 = 0;
		SpiChnPutC(SPI_CHANNEL1, data);
	}
	// first DAC unit
	else {
		// write to the DAC
		DAC_CS0 = 0;
		SpiChnPutC(SPI_CHANNEL1, data);
	}

/*
	// second DAC unit
	if(chan & 0x02) {
		// write to the DAC
		DAC_CS1 = 0;
		SpiChnPutC(SPI_CHANNEL1, data);
		while(SpiChnIsBusy(SPI_CHANNEL1)) ClearWDT();
//		SpiChnReadC(SPI_CHANNEL1);
		DAC_CS1 = 1;
	}
	// first DAC unit
	else {
		// write to the DAC
		DAC_CS0 = 0;
		SpiChnPutC(SPI_CHANNEL1, data);
		while(SpiChnIsBusy(SPI_CHANNEL1)) ClearWDT();
//		SpiChnReadC(SPI_CHANNEL1);
		DAC_CS0 = 1;
	}
*/
}
