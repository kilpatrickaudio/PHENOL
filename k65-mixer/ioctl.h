/*
 * K65 Phenol - Mixer Unit IO Control Routines
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef IOCTL_H
#define IOCTL_H

// states for the MIDI IN and sequencer LEDs
#define SEQ_LED_OFF 0
#define SEQ_LED_BLINK_ONCE 1
#define SEQ_LED_BLINK 2
#define SEQ_LED_ON 3

// pot channels
#define POT_MIXER_MASTER 0
#define POT_MIXER_DELAY_TIME 1
#define POT_MIXER_DELAY_MIX 2
#define POT_MIXER_IN1_LEVEL 3
#define POT_MIXER_IN2_LEVEL 4
#define POT_MIXER_PAN1 5
#define POT_MIXER_PAN2 6
#define POT_SMOOTHING 200

// switch channels
#define SW_REC 0
#define SW_PLAY 1

// DC in channels
#define DC_IN_VSENSE 0
#define CV_SMOOTHING 100

#define DAC_VAL_NOM 0x7ff

// init the ioctl
void ioctl_init(void);

// run the ioctl timer task
void ioctl_timer_task(void);

// get the value of a pot - output is 8 bit
int ioctl_get_pot(unsigned char pot);

// get the DC voltage sense input voltage in counts - 0-4095 = 0-36.3V (8.862mV/count)
int ioctl_get_dc_vsense(void);

// get the state of the divider input
int ioctl_get_divider_in(void);

// get the state of the power switch
int ioctl_get_power_sw(void);

// set the state of the mixer delay LED - 0-255 = 0-63ms
void ioctl_set_mixer_delay_led(int timeout);

// set the state of the mixer output LEDs - 0-255 = 0-63ms
void ioctl_set_mixer_output_leds(int left, int right);

// set the state of the MIDI in LED - 0 = off, 1 = blink once, 2 = blink, 3 = on
void ioctl_set_midi_in_led(int state);

// set the state of the MIDI rec LED - 0 = off, 1 = blink once, 2 = blink, 3 = on
void ioctl_set_midi_rec_led(int state);

// set the state of the MIDI play LED - 0 = off, 1 = blink once, 2 = blink, 3 = on
void ioctl_set_midi_play_led(int state);

// adjust the LED debug state
void ioctl_set_led_debug(int led, int state);

// set the state of the divider outputs
void ioctl_set_divider_outputs(int a, int b, int c, int d);

// set a CV output
void ioctl_set_midi_cv_out(int chan, int val);

// set the state of the MIDI gate out
void ioctl_set_midi_gate_out(int state);

// set the state of the MIDI clock out
void ioctl_set_midi_clock_out(int state);

// set the status of the power control output
void ioctl_set_analog_power_ctrl(int state);

#endif


