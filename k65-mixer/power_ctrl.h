/*
 * Power Control
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef POWER_CTRL_H
#define POWER_CTRL_H

// states
#define POWER_STATE_BOOT 0
#define POWER_STATE_OFF 1
#define POWER_STATE_ON 2
#define POWER_STATE_ERROR 3

// init the power control
void power_ctrl_init(void);

// run the power control timer task
void power_ctrl_timer_task(void);

// gets the power control state
int power_ctrl_get_state(void);

#endif

