/*
 * K65 Phenol Mixer - Pulse Divider
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef PULSE_DIV_H
#define PULSE_DIV_H

// init the pulse divider
void pulse_div_init(void);

// run the pulse divider timer task
void pulse_div_timer_task(void);

// reset the pulse divider outputs and count
void pulse_div_reset(void);

#endif
