/*
 * K65 Phenol - Mod Processor IO Control Routines
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 * Hardware I/O:
 *  RA0/AN0		- MOD1 pot 1				- analog
 *	RA1/AN1		- MOD1 pot 2				- analog
 *	RA2			- MOD1 mode sw 1			- input - active low
 *	RA3			- MOD2 mode sw 3			- input - active low
 *	RA4			- MOD2 mode sw 1			- input - active low
 *	RA7			- MOD2 mode sw 2			- input - active low
 *	RA8			- MOD1 gate sw				- input - active low
 *	RA9			- MOD2 gate sw				- input - active low
 *
 *	RB0			- MOD1 mode sw 3 / PGD		- input - active low / ICP data
 *	RB1			- MOD1 mode sw 2 / PGC		- input - active low / ICP clock
 *	RB2/AN4		- MOD1 pot 3				- analog
 *	RB3/AN5		- MOD2 pot 1				- analog
 *	RB4			- MOD1 gate LED				- output - active high
 *	RB5			- MOD2 gate LED				- output - active high
 *	RB7			- MOD1 mode LED1			- output - active high
 *	RB8			- MOD1 mode LED2			- output - active high
 *	RB9			- MOD1 mode LED3			- output - active high
 *	RB15/AN9	- MOD2 pot 2				- analog
 *
 *  RC0/AN6		- MOD2 pot 3				- analog
 *	RC1/AN7		- MOD3 speed pot			- analog
 *	RC2/AN8		- MOD1 CV in				- analog
 *	RC3/AN12	- MOD2 CV in				- analog
 *	RC4			- MOD1 gate in				- input - active low
 *	RC5			- MOD2 gate in				- input - active low
 *	RC6			- MOD2 mode LED1			- output - active high
 *	RC7			- MOD2 mode LED2			- output - active high
 *	RC8			- MOD2 mode LED3			- output - active high
 *	RC9			- analog power ctrl			- input - active high
 */
#include "ioctl.h"
#include <plib.h>
#include "analog_filter.h"
#include "switch_filter.h"
#include "clamp.h"

// hardware ADC / filtering channels
#define AN_POT_MOD1_POT1 0
#define AN_POT_MOD1_POT2 1
#define AN_POT_MOD1_POT3 2
#define AN_POT_MOD2_POT1 3
#define AN_POT_MOD2_POT3 4
#define AN_POT_MOD3_SPEED 5
#define AN_CV_MOD1 6
#define AN_POT_MOD2_POT2 7
#define AN_CV_MOD2 8

// calibration
#define CV_MOD_OFFSET 0  // when floating the CV input produces no offset

// hardware defines
#define IOCTL_MOD1_MODE_SW1 PORTAbits.RA2
#define IOCTL_MOD2_MODE_SW3 PORTAbits.RA3
#define IOCTL_MOD2_MODE_SW1 PORTAbits.RA4
#define IOCTL_MOD2_MODE_SW2 PORTAbits.RA7
#define IOCTL_MOD1_GATE_SW PORTAbits.RA8  // not debounced
#define IOCTL_MOD2_GATE_SW PORTAbits.RA9  // not debounced
#define IOCTL_MOD1_MODE_SW3 PORTBbits.RB0
#define IOCTL_MOD1_MODE_SW2 PORTBbits.RB1
#define IOCTL_MOD1_GATE_IN PORTCbits.RC4  // not debounced
#define IOCTL_MOD2_GATE_IN PORTCbits.RC5  // not debounced
#define IOCTL_ANALOG_POWER_CTRL PORTCbits.RC9
#define IOCTL_MOD1_GATE_LED LATBbits.LATB4
#define IOCTL_MOD2_GATE_LED LATBbits.LATB5
#define IOCTL_MOD1_MODE_LED1 LATBbits.LATB7
#define IOCTL_MOD1_MODE_LED2 LATBbits.LATB8
#define IOCTL_MOD1_MODE_LED3 LATBbits.LATB9
#define IOCTL_MOD2_MODE_LED1 LATCbits.LATC6
#define IOCTL_MOD2_MODE_LED2 LATCbits.LATC7
#define IOCTL_MOD2_MODE_LED3 LATCbits.LATC8

// state
int ioctl_scan_phase;
int ioctl_mode_leds[2][3];

// local functions
void ioctl_mode_led_write(int chan, int led, int state);

// init the ioctl
void ioctl_init(void) {
	// outputs
	PORTSetPinsDigitalOut(IOPORT_B, BIT_4 | BIT_5 | BIT_6 | BIT_7 | BIT_8 | BIT_9);
	PORTSetPinsDigitalOut(IOPORT_C, BIT_6 | BIT_7 | BIT_8);

	// inputs
	PORTSetPinsDigitalIn(IOPORT_A, BIT_2 | BIT_3 | BIT_4 | BIT_7 | BIT_8 | BIT_9);
	PORTSetPinsDigitalIn(IOPORT_B, BIT_0 | BIT_1);
	PORTSetPinsDigitalIn(IOPORT_C, BIT_4 | BIT_5 | BIT_9);	

	// weak pullups
	CNPUA = 0x03fc;
	CNPUB = 0x0003;

	// analog inputs
	OpenADC10(ADC_MODULE_ON | ADC_IDLE_STOP | ADC_FORMAT_INTG | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON | ADC_SAMP_ON,		ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_ON | ADC_SAMPLES_PER_INT_9 | ADC_BUF_16 | ADC_ALT_INPUT_OFF,		ADC_SAMPLE_TIME_16 | ADC_CONV_CLK_INTERNAL_RC | ADC_CONV_CLK_32Tcy,
		ENABLE_AN0_ANA | ENABLE_AN1_ANA | ENABLE_AN4_ANA | ENABLE_AN5_ANA | \
		ENABLE_AN6_ANA | ENABLE_AN7_ANA | ENABLE_AN8_ANA | ENABLE_AN9_ANA | \
		ENABLE_AN12_ANA,
		SKIP_SCAN_AN2 | SKIP_SCAN_AN3 | SKIP_SCAN_AN10 | SKIP_SCAN_AN11);

	ANSELB &= 0xfffc;  // recover RB0 and RB1 to digital mode - OpenADC10 seems to take them

	// init stuff
	analog_filter_init();
	analog_filter_set_smoothing(AN_POT_MOD1_POT1, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MOD1_POT2, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MOD1_POT3, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MOD2_POT1, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MOD2_POT3, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MOD3_SPEED, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_CV_MOD1, CV_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MOD2_POT2, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_CV_MOD2, CV_SMOOTHING);

	switch_filter_init(10, 2, 0);

	// reset stuff
	ioctl_set_gate_led(0, 0);
	ioctl_set_gate_led(1, 0);
	ioctl_set_mode_led(0, 0, 0);
	ioctl_set_mode_led(0, 1, 0);
	ioctl_set_mode_led(0, 2, 0);
	ioctl_set_mode_led(1, 0, 0);
	ioctl_set_mode_led(1, 1, 0);
	ioctl_set_mode_led(1, 2, 0);

	ioctl_scan_phase = 0;
}

// run the ioctl timer task
void ioctl_timer_task(void) {
	int chan, led;

	// inputs
	switch(ioctl_scan_phase & 0x07) {
		case 0:
			analog_filter_set_val(AN_CV_MOD1, ReadADC10(AN_CV_MOD1) << 2);
			analog_filter_set_val(AN_POT_MOD1_POT1, ReadADC10(AN_POT_MOD1_POT1) << 2);
			switch_filter_set_val(SW_MOD1_SW1, 1, ~IOCTL_MOD1_MODE_SW1);
			break;
		case 1:
			analog_filter_set_val(AN_CV_MOD2, ReadADC10(AN_CV_MOD2) << 2);
			analog_filter_set_val(AN_POT_MOD1_POT2, ReadADC10(AN_POT_MOD1_POT2) << 2);
			switch_filter_set_val(SW_MOD1_SW2, 1, ~IOCTL_MOD1_MODE_SW2);
			break;
		case 2:
			analog_filter_set_val(AN_CV_MOD1, ReadADC10(AN_CV_MOD1) << 2);
			analog_filter_set_val(AN_POT_MOD1_POT3, ReadADC10(AN_POT_MOD1_POT3) << 2);
			switch_filter_set_val(SW_MOD1_SW3, 1, ~IOCTL_MOD1_MODE_SW3);
			break;
		case 3:
			analog_filter_set_val(AN_CV_MOD2, ReadADC10(AN_CV_MOD2) << 2);
			analog_filter_set_val(AN_POT_MOD2_POT1, ReadADC10(AN_POT_MOD2_POT1) << 2);
			switch_filter_set_val(SW_MOD2_SW1, 1, ~IOCTL_MOD2_MODE_SW1);
			break;
		case 4:
			analog_filter_set_val(AN_CV_MOD1, ReadADC10(AN_CV_MOD1) << 2);
			analog_filter_set_val(AN_POT_MOD2_POT3, ReadADC10(AN_POT_MOD2_POT3) << 2);
			switch_filter_set_val(SW_MOD2_SW2, 1, ~IOCTL_MOD2_MODE_SW2);
			break;
		case 5:
			analog_filter_set_val(AN_CV_MOD2, ReadADC10(AN_CV_MOD2) << 2);
			analog_filter_set_val(AN_POT_MOD3_SPEED, ReadADC10(AN_POT_MOD3_SPEED) << 2);
			switch_filter_set_val(SW_MOD2_SW3, 1, ~IOCTL_MOD2_MODE_SW3);
			break;
		case 6:
			analog_filter_set_val(AN_CV_MOD1, ReadADC10(AN_CV_MOD1) << 2);
			analog_filter_set_val(AN_POT_MOD2_POT2, ReadADC10(AN_POT_MOD2_POT2) << 2);
			break;
		case 7:
			analog_filter_set_val(AN_CV_MOD2, ReadADC10(AN_CV_MOD2) << 2);
			break;
	}

	// LEDs
	chan = (ioctl_scan_phase & 0x08) >> 3;
	led = ioctl_scan_phase & 0x07;
	switch(ioctl_mode_leds[chan][led]) {
		case 0:  // off
			ioctl_mode_led_write(chan, led, 0);
			break;
		case 1:  // on
			ioctl_mode_led_write(chan, led, 1);
			break;
		case 2:  // blink
			if(ioctl_scan_phase & 0x700) {
				ioctl_mode_led_write(chan, led, 1);
			}
			else {
				ioctl_mode_led_write(chan, led, 0);
			}
			break;
	}
	ioctl_scan_phase ++;
}

// get the value of a pot - output is 8 bit
int ioctl_get_pot(unsigned char pot) {
	switch(pot) {
		case POT_MOD1_POT1:
			return analog_filter_get_val(AN_POT_MOD1_POT1) >> 4;
		case POT_MOD1_POT2:
			return analog_filter_get_val(AN_POT_MOD1_POT2) >> 4;
		case POT_MOD1_POT3:
			return analog_filter_get_val(AN_POT_MOD1_POT3) >> 4;
		case POT_MOD2_POT1:
			return analog_filter_get_val(AN_POT_MOD2_POT1) >> 4;
		case POT_MOD2_POT2:
			return analog_filter_get_val(AN_POT_MOD2_POT2) >> 4;
		case POT_MOD2_POT3:
			return analog_filter_get_val(AN_POT_MOD2_POT3) >> 4;
		case POT_MOD3_SPEED:
			return analog_filter_get_val(AN_POT_MOD3_SPEED) >> 4;
	}
	return 0;
}

// get the value of a CV input - output is 12 bit
int ioctl_get_cv(unsigned char cv) {
	switch(cv) {
		case CV_MOD1:
			return clamp(analog_filter_get_val(AN_CV_MOD1) + CV_MOD_OFFSET, 0x000, 0xfff);
		case CV_MOD2:
			return clamp(analog_filter_get_val(AN_CV_MOD2) + CV_MOD_OFFSET, 0x000, 0xfff);
	}
	return 0;
}

// get a switch filter event - encoders
int ioctl_get_sw_event(void) {
	return switch_filter_get_event();
}

// get the status of the gate sw
int ioctl_get_gate_sw(int chan) {
	if(chan) {
		return (~IOCTL_MOD2_GATE_SW) & 0x01;
	}
	return (~IOCTL_MOD1_GATE_SW) & 0x01;
}

// get the status of the gate in
int ioctl_get_gate_in(int chan) {
	if(chan) {
		return (~IOCTL_MOD2_GATE_IN) & 0x01;
	}
	return (~IOCTL_MOD1_GATE_IN) & 0x01;
}

// get the status of the power control input
int ioctl_get_analog_power_ctrl(void) {
	return IOCTL_ANALOG_POWER_CTRL;
}

// set the gate LED state
void ioctl_set_gate_led(int chan, int state) {
	if(chan) {
		IOCTL_MOD2_GATE_LED = state;
	}
	else {
		IOCTL_MOD1_GATE_LED = state;		
	}
}

// set the mode LED state - 0 = off, 1 = on, 2 = blink
void ioctl_set_mode_led(int chan, int led, int val) {
	if(chan < 0 || chan > 1) {
		return;
	}
	if(led < 0 || led > 2) {
		return;
	}
	if(val < 0 || val > 2) {
		return;
	}
	ioctl_mode_leds[chan][led] = val;
}

//
// local functions
//
// set a mode LED output
void ioctl_mode_led_write(int chan, int led, int state) {
	if(chan < 0 || chan > 1) {
		return;
	}
	if(led < 0 || led > 2) {
		return;
	}
	if(chan) {
		switch(led) {
			case 0:  // mod2 type
				IOCTL_MOD2_MODE_LED1 = state & 0x01;
				break;
			case 1:  // mod2 mod
				IOCTL_MOD2_MODE_LED2 = state & 0x01;
				break;
			case 2:  // mod2 out
				IOCTL_MOD2_MODE_LED3 = state & 0x01;
				break;
		}
	}
	else {
		switch(led) {
			case 0:  // mod1 type
				IOCTL_MOD1_MODE_LED1 = state & 0x01;
				break;
			case 1:  // mod1 mod
				IOCTL_MOD1_MODE_LED2 = state & 0x01;
				break;
			case 2:  // mod1 out
				IOCTL_MOD1_MODE_LED3 = state & 0x01;
				break;
		}
	}
}
