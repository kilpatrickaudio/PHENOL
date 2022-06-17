/*
 * K65 Phenol Mixer - Audio Subsystem
 *
 * Copyright 2014: Kilpatrick 
 * Written by: Andrew Kilpatrick
 *
 * Hardware I/O:
 *	RA4			- codec MCLK				- REFCLKO
 *	RA1			- codec ADC dat				- SDI1
 *	RA9			- codec DAC dat				- SDO1
 *
 *	RB4			- codec LRCLK				- !SS1
 *	RB8			- codec I2C clock			- SCL1
 *	RB9			- codec I2C data			- SDA1
 *	RB14		- codec BCLK				- SCK1
 *
 */
#include <plib.h>
#include <dsplib_def.h>
#include <inttypes.h>
#include "audio_sys.h"
#include "audio_sys_ctrl.h"
#include "WM8731_ctrl.h"
#include "audio_proc.h"

// hardware defines
#define I2C1_SCL_TRIS TRISBbits.TRISB8
#define I2C1_SDA_TRIS TRISBbits.TRISB9
#define I2C1_SCL LATBbits.LATB8
#define I2C1_SDA LATBbits.LATB9

// audio buffers
int16_t audio_rec_buf[AUDIO_BUF_SIZE];
int16_t audio_play_buf[AUDIO_BUF_SIZE];
int audio_stream_p;

// XXX debugging mode
//#define SINE_OUTPUT_TEST  // use sine table
#ifdef SINE_OUTPUT_TEST
// XXX debug - -6dB tone
int sine48[] = {
0	,
2139	,
4240	,
6270	,
8192	,
9974	,
11585	,
12998	,
14189	,
15137	,
15826	,
16244	,
16384	,
16244	,
15826	,
15137	,
14189	,
12998	,
11585	,
9974	,
8192	,
6270	,
4240	,
2139	,
0	,
-2139	,
-4240	,
-6270	,
-8192	,
-9974	,
-11585	,
-12998	,
-14189	,
-15137	,
-15826	,
-16244	,
-16384	,
-16244	,
-15826	,
-15137	,
-14189	,
-12998	,
-11585	,
-9974	,
-8192	,
-6270	,
-4240	,
-2139	
};
int sine_count = 0;
#endif 

// init the audio system - set up I2C
void audio_sys_init(void) {
	// map pins - check that this is correct for your hardware
	PPSUnLock;
	PPSOutput(3, RPA4, REFCLKO);  // RPA4 - codec MCLK
	PPSInput(2, SDI1, RPA1);  // RPA1 - codec ADC dat
	PPSOutput(1, RPB4, SS1);  // RPB4 - codec LRCLK
	PPSOutput(2, RPA9, SDO1);  // RPA9 - codec DAC dat
	PPSLock;

	// 
	// init the i2c control interface - i2c port 1
	// - note: pullups must be turned on or wired up
	//	before i2c will work here
	//
	I2C1BRG = 0x00c2;  // 100kHz at 40MHz PBCLOCK
   	I2C1CON = 0xb000;

   	// i2c errata
	I2C1_SCL = 0;
	I2C1_SDA = 0;
	I2C1_SCL_TRIS = 1;
	I2C1_SDA_TRIS = 0;
	I2C1CONbits.DISSLW = 1;  // affects RA0 and RA1 voltages?!
	// initiate start condition
  	I2C1CONbits.SEN = 1;
 	while(I2C1CONbits.SEN == 1);
	// initiate stop condition
    I2C1CONbits.PEN = 1;
	while(I2C1CONbits.PEN);

	// reset stuff
	audio_stream_p = 0;
}

// start the audio system - start I2S
void audio_sys_start(void) {
	int temp;
	//
	// set up the MCLK refclk output
	//
	REFOCON = 0;  // clear everything
	REFOCONbits.ACTIVE = 0;  // make sure this is off before changing settings
	REFOTRIM = 131 << 23;  // fractional division
	REFOCONbits.RODIV = 3;  // whole division
	REFOCONbits.SIDL = 1;  // disable in idle
	REFOCONbits.OE = 1;  // output enable
	REFOCONbits.RSLP = 0;  // disable in sleep
	REFOCONbits.ROSEL = 0x07;  // system PLL output
	REFOCONbits.ON = 1;  // turn on the module
	REFOCONbits.ACTIVE = 1;  // start the refclock

	//
	// I2S master on port 1
	//
	// reset stuff
	IEC1bits.SPI1TXIE = 0;  // disable interrupts
	SPI1CON = 0;  // clear
	SPI1CON2 = 0;  // clear
	temp = SPI1BUF;  // clear receive buffer
	// set up hardware interrupt - SPI1 transmit complete
	IFS1bits.SPI1TXIF = 0;  // clear SPI1 TX flag
	IPC7bits.SPI1IP = 4;  // SPI1 main priority
	IPC7bits.SPI1IS = 0;  // SPI1 sub priority
	IEC1bits.SPI1TXIE = 1;  // enable interrupts

	// set up the SPI1 port
	SPI1BRG = 0x01;  // 64fs bitclock
	SPI1CONbits.MCLKSEL = 1;  // refclock is master clock
	SPI1CONbits.MODE32 = 0;  // audio frame = 16 bit data
	SPI1CONbits.MODE16 = 1;  // audio frame = 16 bit fifo / 64 bit frame
	SPI1CONbits.CKP = 1;  // invert clock polarity
	SPI1CONbits.ENHBUF = 1;  // enhanced buffer mode
	SPI1CONbits.MSTEN = 1;  // master mode
	SPI1CONbits.STXISEL = 0x02;  // interrupt when half empty
	SPI1CON2bits.AUDMOD = 0x00;  // i2s mode
	SPI1CON2bits.IGNROV = 1;  // ignore receive overflow
	SPI1CON2bits.IGNTUR = 1;  // ignore transmit underrun
	SPI1CON2bits.AUDEN = 1;  // audio codec enable
	// fill SPI1 buffer with silence
	SPI1BUF = 0;
	SPI1BUF = 0;
	SPI1BUF = 0;
	SPI1BUF = 0;
	// turn on SPI
	SPI1CONbits.ON = 1;
}

// write to the i2c control bus for audio devices - I2C1
void audio_sys_write_ctrl(unsigned char addr, unsigned char tx_data[], int tx_data_len) {
	int i;
	// start condition
	StartI2C1();
	IdleI2C1();
	// send address
	MasterWriteI2C1((addr << 1) | 0);
	IdleI2C1();
 	if(I2C1STATbits.ACKSTAT) return;            // NACK'ed by slave ?

	// send tx len data bytes
	for(i = 0; i < tx_data_len; i ++) {	
		MasterWriteI2C1(tx_data[i]);
		IdleI2C1();
 		if(I2C1STATbits.ACKSTAT) return;            // NACK'ed by slave ?
	}

	// stop condition
	StopI2C1();
	IdleI2C1();
}

// read from the i2c control bus for audio devices - I2C1
void audio_sys_read_ctrl(unsigned char addr, unsigned char tx_data[], int tx_data_len,
						unsigned char rx_data[], int rx_data_len) {
	int i;
	// start condition
	StartI2C1();
	IdleI2C1();

	// address - with write bit
	MasterWriteI2C1((addr << 1) | 0);
	IdleI2C1();
 	if(I2C1STATbits.ACKSTAT) return;  // NACK'ed by slave ?

	// send tx len data bytes
	for(i = 0; i < tx_data_len; i ++) {	
		MasterWriteI2C1(tx_data[i]);
		IdleI2C1();
 		if(I2C1STATbits.ACKSTAT) return;  // NACK'ed by slave ?
	}

	// restart condition
	RestartI2C1();
	IdleI2C1();

	// address - this time with read bit
	MasterWriteI2C1((addr << 1) | 1);
	IdleI2C1();
 	if(I2C1STATbits.ACKSTAT) return;  // NACK'ed by slave ?

	// receive rx len data bytes
	for(i = 0; i < rx_data_len; i ++) {
 		I2C1CONbits.RCEN = 1;
// 		while(!DataRdyI2C1());	// this causes an extra byte to be written at the end
		rx_data[i] = MasterReadI2C1();
 		if(i < (rx_data_len - 1)) {
 			// ACK the slave
 			I2C1CONbits.ACKDT = 0;
 			I2C1CONbits.ACKEN = 1;
 		}
 		else {
 			// NACK the slave
 			I2C1CONbits.ACKDT = 1;
 			I2C1CONbits.ACKEN = 1;
 		}
 		while(I2C1CONbits.ACKEN == 1);  // wait for ACK/NAK
	}

	// stop condition
	StopI2C1();
	IdleI2C1();
}

// audio output interrupt - SPI1 TX done
void __ISR(_SPI_1_VECTOR, ipl4) SPI_I2S_TRANSMIT(void) {
#ifdef SINE_OUTPUT_TEST
	int inL, inR;
#endif
    while(SPI1STATbits.TXBUFELM <= 4) {
#ifdef SINE_OUTPUT_TEST
		// get ADC samples
		inL = SPI1BUF;
		inR = SPI1BUF;
		// play sines
		SPI1BUF = sine48[sine_count];
		SPI1BUF = sine48[sine_count];
		sine_count ++;
		if(sine_count == 48) sine_count = 0;
#else
		// implement ping pong buffers
		audio_rec_buf[audio_stream_p] = SPI1BUF;
		audio_rec_buf[audio_stream_p+1] = SPI1BUF;
		SPI1BUF = audio_play_buf[audio_stream_p];
		SPI1BUF = audio_play_buf[audio_stream_p+1];
		audio_stream_p = (audio_stream_p + 2) & AUDIO_BUF_MASK;
#endif
	}
    IFS1bits.SPI1TXIF = 0; // clear interrupt flag
}
