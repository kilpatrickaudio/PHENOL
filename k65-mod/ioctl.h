/*
 * K65 Phenol - Mod Processor IO Control Routines
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef IOCTL_H
#define IOCTL_H

// pot channels
#define POTS_PER_BANK 3
#define POT_MOD1_POT1 0
#define POT_MOD1_POT2 1
#define POT_MOD1_POT3 2
#define POT_MOD2_POT1 3
#define POT_MOD2_POT2 4
#define POT_MOD2_POT3 5
#define POT_MOD3_SPEED 6
#define POT_SMOOTHING 25

// switch channels
#define SW_MOD1_SW1 0  // type
#define SW_MOD1_SW2 1  // mod
#define SW_MOD1_SW3 2  // out
#define SW_MOD2_SW1 3  // type
#define SW_MOD2_SW2 4  // mod
#define SW_MOD2_SW3 5  // out

// CV channels
#define CV_MOD1 0
#define CV_MOD2 1
#define CV_SMOOTHING 25

// init the ioctl
void ioctl_init(void);

// run the ioctl timer task
void ioctl_timer_task(void);

// get the value of a pot - output is 8 bit
int ioctl_get_pot(unsigned char pot);

// get the value of a CV input - output is 12 bit
int ioctl_get_cv(unsigned char cv);

// get a switch filter event - encoders
int ioctl_get_sw_event(void);

// get the status of the gate sw
int ioctl_get_gate_sw(int chan);

// get the status of the gate in
int ioctl_get_gate_in(int chan);

// get the status of the power control input
int ioctl_get_analog_power_ctrl(void);

// set the gate LED state
void ioctl_set_gate_led(int chan, int state);

// set the mode LED state - 0 = off, 1 = on, 2 = blink
void ioctl_set_mode_led(int chan, int led, int val);

#endif


