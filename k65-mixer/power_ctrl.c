/*
 * Power Control
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#include "power_ctrl.h"
#include "ioctl.h"
#include "HardwareProfile.h"

struct pwr_ctrl {
	int state;  // current state
	int state_timeout;  // timeout for state change
	int sw_state;  // switch state
};
struct pwr_ctrl power_ctrl;

// setting
#define POWER_STATE_TIMEOUT 8
#define POWER_LOW_VOLTAGE_CUTOFF 2031

// local functions
void power_ctrl_set_state(int state);

// init the power control
void power_ctrl_init(void) {
	power_ctrl_set_state(POWER_STATE_BOOT);
	power_ctrl.sw_state = 0;
}

// run the power control timer task
void power_ctrl_timer_task(void) {
	int temp;

	if(power_ctrl.state_timeout) {
		power_ctrl.state_timeout --;
		return;
	}

	// check power supply voltage
	if(ioctl_get_dc_vsense() < POWER_LOW_VOLTAGE_CUTOFF) {
		power_ctrl_set_state(POWER_STATE_ERROR);
	}

	// check state
	switch(power_ctrl.state) {
		case POWER_STATE_BOOT:
#ifdef POWER_ON_POWER_APPLIED
			power_ctrl_set_state(POWER_STATE_ON);
#else
			power_ctrl_set_state(POWER_STATE_OFF);
#endif
			break;
		case POWER_STATE_OFF:
			// check power switch
			temp = ioctl_get_power_sw();
			if(temp != power_ctrl.sw_state && temp) {
				power_ctrl_set_state(POWER_STATE_ON);
			}
			power_ctrl.sw_state = temp;
			break;
		case POWER_STATE_ON:
			// check power switch
			temp = ioctl_get_power_sw();
			if(temp != power_ctrl.sw_state && temp) {
				power_ctrl_set_state(POWER_STATE_OFF);
			}
			power_ctrl.sw_state = temp;
			break;
		case POWER_STATE_ERROR:
			// error blinky in main routine
			break;
	}
}

// gets the power control state
int power_ctrl_get_state(void) {
	return power_ctrl.state;
}

// set the power state
void power_ctrl_set_state(int state) {
	switch(state) {
		case POWER_STATE_BOOT:
			ioctl_set_analog_power_ctrl(0);  // force analog off
			power_ctrl.state = POWER_STATE_BOOT;
			break;
		case POWER_STATE_OFF:
			ioctl_set_analog_power_ctrl(0);
			power_ctrl.state = POWER_STATE_OFF;
			break;
		case POWER_STATE_ON:
			ioctl_set_analog_power_ctrl(1);
			power_ctrl.state = POWER_STATE_ON;
			break;
		case POWER_STATE_ERROR:
			ioctl_set_analog_power_ctrl(0);  // force analog off
			power_ctrl.state = POWER_STATE_ERROR;
			break;
	}
	power_ctrl.state_timeout = POWER_STATE_TIMEOUT;
}
