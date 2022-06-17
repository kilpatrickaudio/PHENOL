/*
 * MCP4822 DAC / LED Shift Register Driver
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef DAC_LED_H
#define DAC_LED_H

// output definitions
#define DAC_CV1 0
#define DAC_CV2 1

// init the DAC / LED driver
void dac_led_init(void);

// write to the DAC
void dac_led_write_dac(unsigned char chan, unsigned int val);

// write to the LEDs
void dac_led_write_led(unsigned int val);

#endif

