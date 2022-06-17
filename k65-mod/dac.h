/*
 * MCP4822 DAC Driver
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef DAC_H
#define DAC_H

// output definitions
#define DAC_MOD1 0
#define DAC_MOD2 1
#define DAC_MOD3_RAND 2
#define DAC_MOD3_SINE 3

#define DAC_VAL_OFF 0x000  // 0V DAC output

// init the DAC driver
void dac_init(void);

// write to the DAC
void dac_write_dac(unsigned char chan, unsigned int val);

#endif
