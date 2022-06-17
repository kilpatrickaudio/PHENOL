/*
 * K65 Phenol Mixer - IO Control Routines
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 * Hardware I/O:
 *  RA0/AN0		- mixer master pot			- analog
 *	RA7			- analog pwr ctrl			- output - active high
 *	RA8			- power sw					- input - active low
 *	RA10		- divider in				- input - active low
 *
 *	RB0			- rec SW / PGD				- rec SW (active low) / ICP data
 *	RB1			- play SW / PGC				- play SW (active low) / ICP clock
 *	RB2/AN4		- mixer delay time pot		- analog
 *	RB3/AN5		- mixer delay mix pot		- analog
 *	RB5 		- MIDI gate out				- output - active high
 *	RB7			- MIDI clock out			- output - active high
 *	RB13/AN11	- DC in vsense				- analog
 *
 *	RC0/AN6		- mixer in 1 level pot		- analog
 *	RC1/AN7		- mixer in 2 level pot		- analog
 *	RC2/AN8		- mixer pan 1 pot			- analog
 *	RC3/AN12	- mixer pan 2 pot			- analog
 *	RC5			- mixer master L LED		- output - active high
 *	RC7			- mixer master R LED		- output - active high
 *
 */
#include "ioctl.h"
#include "dac_led.h"
#include <plib.h>
#include "analog_filter.h"
#include "switch_filter.h"

// debug timing using master output LEDs
//#define LED_DEBUG

// timeout for blinking
#define SEQ_LED_TIMEOUT 512

// hardware ADC / filtering channels
#define AN_POT_MIXER_MASTER 0
#define AN_POT_MIXER_DELAY_TIME 1
#define AN_POT_MIXER_DELAY_MIX 2
#define AN_POT_MIXER_IN1_LEVEL 3
#define AN_POT_MIXER_IN2_LEVEL 4
#define AN_POT_MIXER_PAN1 5
#define AN_DC_IN_VSENSE 6
#define AN_POT_MIXER_PAN2 7

// hardware defines
#define IOCTL_POWER_SW PORTAbits.RA8
#define IOCTL_DIVIDER_IN PORTAbits.RA10
#define IOCTL_MIDI_REC_SW PORTBbits.RB0
#define IOCTL_MIDI_PLAY_SW PORTBbits.RB1
#define IOCTL_ANALOG_PWR_CTRL LATAbits.LATA7
#define IOCTL_MIDI_GATE_OUT LATBbits.LATB5
#define IOCTL_MIDI_CLOCK_OUT LATBbits.LATB7
#define IOCTL_MIXER_MASTER_L_LED LATCbits.LATC5
#define IOCTL_MIXER_MASTER_R_LED LATCbits.LATC7

// LED shift register bits
#define LED_MIDI_PLAY_LED 0x01
#define LED_MIDI_REC_LED 0x02
#define LED_MIDI_IN_LED 0x04
#define LED_MIXER_DELAY_TIME_LED 0x08
#define LED_DIVIDER_OUT2 0x10
#define LED_DIVIDER_OUT3 0x20
#define LED_DIVIDER_OUT4 0x40
#define LED_DIVIDER_OUT6 0x80

// state
int ioctl_scan_phase;
int ioctl_mixer_outL_led_timeout;  // blink time for mixer out L LED
int ioctl_mixer_outR_led_timeout;  // blink time for mixer out R LED
int ioctl_mixer_delay_led_timeout;  // blink timeout for delay time LED
int ioctl_midi_clock_timeout;  // pulse time for MIDI clock output (jack and LED)
int ioctl_midi_in_led_timeout;  // blink time for MIDI in LED
int ioctl_led_reg;  // control register for the LED shift register (8 additional LEDs)
// these LEDs can blink once or multiple times
int ioctl_midi_rec_led_state;  // MIDI rec LED state (as per defines in .h file)
int ioctl_midi_rec_led_timeout;  // MIDI rec LED timeout
int ioctl_midi_play_led_state;  // MIDI play LED state (as per defines in .h file)
int ioctl_midi_play_led_timeout;  // MIDI play LED timeout

// init the ioctl
void ioctl_init(void) {
	// outputs
	PORTSetPinsDigitalOut(IOPORT_A, BIT_7);
	PORTSetPinsDigitalOut(IOPORT_B, BIT_5 | BIT_7);
	PORTSetPinsDigitalOut(IOPORT_C, BIT_5 | BIT_7);

	// inputs
	PORTSetPinsDigitalIn(IOPORT_A, BIT_8 | BIT_10);
	PORTSetPinsDigitalIn(IOPORT_B, BIT_0 | BIT_1);

	// weak pull-ups
	CNPUA = 0x0100;  // power switch
	CNPUB = 0x0003;  // rec and play switches

	// analog inputs
	OpenADC10(ADC_MODULE_ON | ADC_IDLE_STOP | ADC_FORMAT_INTG | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON | ADC_SAMP_ON,		ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_ON | ADC_SAMPLES_PER_INT_8 | ADC_BUF_16 | ADC_ALT_INPUT_OFF,		ADC_SAMPLE_TIME_16 | ADC_CONV_CLK_INTERNAL_RC | ADC_CONV_CLK_32Tcy,
		ENABLE_AN0_ANA | ENABLE_AN4_ANA | ENABLE_AN5_ANA | ENABLE_AN6_ANA | \
		ENABLE_AN7_ANA | ENABLE_AN8_ANA | ENABLE_AN11_ANA | ENABLE_AN12_ANA,
		SKIP_SCAN_AN1 | SKIP_SCAN_AN2 | SKIP_SCAN_AN3 | SKIP_SCAN_AN9 | SKIP_SCAN_AN10);

	ANSELB &= 0xfffc;  // recover RB0 and RB1 to digital mode - OpenADC10 seems to take them
	// init stuff
	analog_filter_init();
	switch_filter_init(40, 10, 0);
	dac_led_init();

	analog_filter_set_smoothing(AN_POT_MIXER_MASTER, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MIXER_DELAY_TIME, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MIXER_DELAY_MIX, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MIXER_IN1_LEVEL, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MIXER_IN2_LEVEL, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MIXER_PAN1, POT_SMOOTHING);
	analog_filter_set_smoothing(AN_DC_IN_VSENSE, CV_SMOOTHING);
	analog_filter_set_smoothing(AN_POT_MIXER_PAN2, POT_SMOOTHING);

	// reset stuff
	ioctl_led_reg = 0;
	ioctl_set_mixer_delay_led(0);
	ioctl_set_midi_in_led(0);
	ioctl_set_mixer_output_leds(0, 0);
	ioctl_set_divider_outputs(0, 0, 0, 0);
	ioctl_set_midi_gate_out(0);
	ioctl_set_midi_clock_out(0);
	ioctl_set_midi_rec_led(SEQ_LED_OFF);
	ioctl_set_midi_play_led(SEQ_LED_OFF);
	ioctl_set_analog_power_ctrl(0);

	ioctl_scan_phase = 0;
}

// run the ioctl timer task
void ioctl_timer_task(void) {
	switch(ioctl_scan_phase & 0x07) {
		case 0:
			analog_filter_set_val(AN_POT_MIXER_MASTER, ReadADC10(AN_POT_MIXER_MASTER) << 2);
			switch_filter_set_val(SW_REC, 1, ~IOCTL_MIDI_REC_SW);
			break;
		case 1:
			analog_filter_set_val(AN_POT_MIXER_DELAY_TIME, ReadADC10(AN_POT_MIXER_DELAY_TIME) << 2);
			switch_filter_set_val(SW_PLAY, 1, ~IOCTL_MIDI_PLAY_SW);
			break;
		case 2:
			analog_filter_set_val(AN_POT_MIXER_DELAY_MIX, ReadADC10(AN_POT_MIXER_DELAY_MIX) << 2);
			switch_filter_set_val(SW_REC, 1, ~IOCTL_MIDI_REC_SW);
			break;
		case 3:
			analog_filter_set_val(AN_POT_MIXER_IN1_LEVEL, ReadADC10(AN_POT_MIXER_IN1_LEVEL) << 2);
			switch_filter_set_val(SW_PLAY, 1, ~IOCTL_MIDI_PLAY_SW);
			break;
		case 4:
			analog_filter_set_val(AN_POT_MIXER_IN2_LEVEL, ReadADC10(AN_POT_MIXER_IN2_LEVEL) << 2);
			switch_filter_set_val(SW_REC, 1, ~IOCTL_MIDI_REC_SW);
			break;
		case 5:
			analog_filter_set_val(AN_POT_MIXER_PAN1, ReadADC10(AN_POT_MIXER_PAN1) << 2);
			switch_filter_set_val(SW_PLAY, 1, ~IOCTL_MIDI_PLAY_SW);
			break;
		case 6:
			analog_filter_set_val(AN_POT_MIXER_PAN2, ReadADC10(AN_POT_MIXER_PAN2) << 2);
			switch_filter_set_val(SW_REC, 1, ~IOCTL_MIDI_REC_SW);
			break;
		case 7:
			analog_filter_set_val(AN_DC_IN_VSENSE, ReadADC10(AN_DC_IN_VSENSE) << 2);
			switch_filter_set_val(SW_PLAY, 1, ~IOCTL_MIDI_PLAY_SW);
			break;
	}

	//
	// LED blinking
	//
#ifndef LED_DEBUG
	// output L
	if(ioctl_mixer_outL_led_timeout) {
		IOCTL_MIXER_MASTER_L_LED = 1;
		ioctl_mixer_outL_led_timeout --;
	}
	else {
		IOCTL_MIXER_MASTER_L_LED = 0;
	}

	// output R
	if(ioctl_mixer_outR_led_timeout) {
		IOCTL_MIXER_MASTER_R_LED = 1;
		ioctl_mixer_outR_led_timeout --;
	}
	else {
		IOCTL_MIXER_MASTER_R_LED = 0;
	}
#endif

	// delay time LED blink
	if(ioctl_mixer_delay_led_timeout) {
		ioctl_led_reg |= LED_MIXER_DELAY_TIME_LED;
		ioctl_mixer_delay_led_timeout --;
	}
	else {
		ioctl_led_reg &= ~LED_MIXER_DELAY_TIME_LED;
	}

	// MIDI IN LED blink
	if(ioctl_midi_in_led_timeout) {
		ioctl_led_reg |= LED_MIDI_IN_LED;
		ioctl_midi_in_led_timeout --;
	}
	else {
		ioctl_led_reg &= ~LED_MIDI_IN_LED;
	}

	// MIDI clock out blink
	if(ioctl_midi_clock_timeout) {
		IOCTL_MIDI_CLOCK_OUT = 1;
		ioctl_midi_clock_timeout --;
	}
	else {
		IOCTL_MIDI_CLOCK_OUT = 0;	
	}

	// MIDI rec LED
	switch(ioctl_midi_rec_led_state) {
		case SEQ_LED_BLINK_ONCE:
			if(ioctl_midi_rec_led_timeout) {
				ioctl_led_reg |= LED_MIDI_REC_LED;
				ioctl_midi_rec_led_timeout --;
			}
			else {
				ioctl_led_reg &= ~LED_MIDI_REC_LED;
				ioctl_midi_rec_led_state = SEQ_LED_OFF;
			}
			break;
		case SEQ_LED_BLINK:
			if(ioctl_scan_phase & 0x200) {
				ioctl_led_reg |= LED_MIDI_REC_LED;
			}
			else {
				ioctl_led_reg &= ~LED_MIDI_REC_LED;
			}
			break;
	}

	// MIDI play LED
	switch(ioctl_midi_play_led_state) {
		case SEQ_LED_BLINK_ONCE:
			if(ioctl_midi_play_led_timeout) {
				ioctl_led_reg |= LED_MIDI_PLAY_LED;
				ioctl_midi_play_led_timeout --;
			}
			else {
				ioctl_led_reg &= ~LED_MIDI_PLAY_LED;
				ioctl_midi_play_led_state = SEQ_LED_OFF;
			}
			break;
		case SEQ_LED_BLINK:
			if(ioctl_scan_phase & 0x200) {
				ioctl_led_reg |= LED_MIDI_PLAY_LED;
			}
			else {
				ioctl_led_reg &= ~LED_MIDI_PLAY_LED;
			}
			break;
	}

	// do DAC or LED update
	dac_led_write_led(ioctl_led_reg);
	ioctl_scan_phase ++;
}

// get the value of a pot - output is 8 bit
int ioctl_get_pot(unsigned char pot) {
	switch(pot) {
		case POT_MIXER_MASTER:
			return analog_filter_get_val(AN_POT_MIXER_MASTER) >> 4;
		case POT_MIXER_DELAY_TIME:
			return analog_filter_get_val(AN_POT_MIXER_DELAY_TIME) >> 4;
		case POT_MIXER_DELAY_MIX:
			return analog_filter_get_val(AN_POT_MIXER_DELAY_MIX) >> 4;
		case POT_MIXER_IN1_LEVEL:
			return analog_filter_get_val(AN_POT_MIXER_IN1_LEVEL) >> 4;
		case POT_MIXER_IN2_LEVEL:
			return analog_filter_get_val(AN_POT_MIXER_IN2_LEVEL) >> 4;
		case POT_MIXER_PAN1:
			return analog_filter_get_val(AN_POT_MIXER_PAN1) >> 4;
		case POT_MIXER_PAN2:
			return analog_filter_get_val(AN_POT_MIXER_PAN2) >> 4;
	}
	return 0;
}

// get the DC voltage sense input voltage in counts - 0-4095 = 0-36.3V (8.862mV/count)
int ioctl_get_dc_vsense(void) {
	return analog_filter_get_val(AN_DC_IN_VSENSE);
}

// get the state of the divider input
int ioctl_get_divider_in(void) {
	if(IOCTL_DIVIDER_IN) {
		return 0;
	}
	return 1;
}

// get the state of the power switch
int ioctl_get_power_sw(void) {
 	if(IOCTL_POWER_SW) {
		return 0;
	}
	return 1;
}

// set the state of the mixer delay LED - 0-255 = 0-63ms
void ioctl_set_mixer_delay_led(int timeout) {
	ioctl_mixer_delay_led_timeout = timeout & 0xff;
}

// set the state of the mixer output LEDs - 0-255 = 0-63ms
void ioctl_set_mixer_output_leds(int left, int right) {
	ioctl_mixer_outL_led_timeout = left & 0xff;
	ioctl_mixer_outR_led_timeout = right & 0xff;
}

// set the state of the MIDI in LED - 0 = off, 1 = blink once, 2 = blink, 3 = on
void ioctl_set_midi_in_led(int timeout) {
	ioctl_midi_in_led_timeout = timeout & 0xff;
}

// set the state of the MIDI rec LED
void ioctl_set_midi_rec_led(int state) {
	switch(state) {
		case SEQ_LED_OFF:
			ioctl_led_reg &= ~LED_MIDI_REC_LED;
			break;
		case SEQ_LED_BLINK_ONCE:
			ioctl_midi_rec_led_timeout = SEQ_LED_TIMEOUT;
			break;
		case SEQ_LED_BLINK:
			// XXX handled by the task timer
			break;
		case SEQ_LED_ON:
			ioctl_led_reg |= LED_MIDI_REC_LED;
			break;
	}
	ioctl_midi_rec_led_state = state;
}

// set the state of the MIDI play LED
void ioctl_set_midi_play_led(int state) {
	switch(state) {
		case SEQ_LED_OFF:
			ioctl_led_reg &= ~LED_MIDI_PLAY_LED;
			break;
		case SEQ_LED_BLINK_ONCE:
			ioctl_midi_play_led_timeout = SEQ_LED_TIMEOUT;
			break;
		case SEQ_LED_BLINK:
			// XXX handled by the task timer
			break;
		case SEQ_LED_ON:
			ioctl_led_reg |= LED_MIDI_PLAY_LED;
			break;
	}
	ioctl_midi_play_led_state = state;
}

// adjust the LED debug state
void ioctl_set_led_debug(int led, int state) {
#ifdef LED_DEBUG
	if(led) {
		IOCTL_MIXER_MASTER_R_LED = state & 0x01;
	}
	else {
		IOCTL_MIXER_MASTER_L_LED = state & 0x01;
	}
#endif
}

// set the state of the divider outputs
void ioctl_set_divider_outputs(int a, int b, int c, int d) {
	ioctl_led_reg &= ~(LED_DIVIDER_OUT2 | LED_DIVIDER_OUT3 | LED_DIVIDER_OUT4 | LED_DIVIDER_OUT6);
	if(a) {
		ioctl_led_reg |= LED_DIVIDER_OUT2;
	}
	if(b) {
		ioctl_led_reg |= LED_DIVIDER_OUT3;
	}
	if(c) {
		ioctl_led_reg |= LED_DIVIDER_OUT4;
	}
	if(d) {
		ioctl_led_reg |= LED_DIVIDER_OUT6;
	}
}

// set a CV output
void ioctl_set_midi_cv_out(int chan, int val) {
 	dac_led_write_dac(chan, val);  // XXX inline this
}

// set the state of the MIDI gate out
void ioctl_set_midi_gate_out(int state) {
	IOCTL_MIDI_GATE_OUT = state & 0x01;
}

// set the state of the MIDI clock out - 0-254 = 0-63ms, 255 = on
void ioctl_set_midi_clock_out(int timeout) {
	ioctl_midi_clock_timeout = timeout & 0xff;
}

// set the status of the power control output
void ioctl_set_analog_power_ctrl(int state) {
	IOCTL_ANALOG_PWR_CTRL = state & 0x01;
}