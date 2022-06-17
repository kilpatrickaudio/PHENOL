/*
 * K65 Phenol - Mod Processor Main
 *
 * Copyright 2014: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 * Hardware I/O:
 *
 *  RA0/AN0		- MOD1 pot 1				- analog
 *	RA1/AN1		- MOD1 pot 2				- analog
 *	RA2			- MOD1 mode sw 1			- input - active low
 *	RA3			- MOD2 mode sw 3			- input - active low
 *	RA4			- MOD2 mode sw 1			- input - active low
 *	RA7			- MOD2 mode sw 2			- input - active low
 *	RA8			- MOD1 gate sw				- input - active low
 *	RA9			- MOD2 gate sw				- input - active low
 *	RA10		- n/c
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
 *	RB10		- !MOD DAC CS0				- output - active low
 *	RB11		- !MOD DAC CS1				- output - active low
 *	RB13/RPB13	- MOD DAC MOSI				- SD01
 *	RB14		- MOD DAC SCK				- SCK1
 *	RB15/AN9	- MOD2 pot 2				- analog
 *
 *  RC0/AN6		- MOD2 pot 3				- analog
 *	RC1/AN7		- MOD3 speed pot			- analog
 *	RC2/AN8		- MOD1 CV in				- analog
 *	RC3/AN12	- MOD2 CV in				- analog
 *	RC4			- MOD1 gate in				- analog
 *	RC5			- MOD2 gate in				- analog
 *	RC6			- MOD2 mode LED1			- output - active high
 *	RC7			- MOD2 mode LED2			- output - active high
 *	RC8			- MOD2 mode LED3			- output - active high
 *	RC9			- analog power ctrl			- input - active high
 *
 */
#include <plib.h>
#include "HardwareProfile.h"
#include "TimeDelay.h"
#include "dac.h"
#include "ioctl.h"
#include "env_proc.h"

// fuse settings
#pragma config UPLLEN   = OFF      // USB PLL disabled
#pragma config FNOSC = FRCPLL  	   // internal oscillator - internal
#pragma config POSCMOD = OFF 	   // primary oscillator - HS mode
#pragma config FPLLIDIV = DIV_2    // PLL input divide by 2 - bring 8MHz down to 4MHz for PLL
#pragma config FPLLMUL  = MUL_20   // PLL multiplier - x20 = 80MHz
#pragma config FPLLODIV = DIV_2    // PLL output divider - /2 = 40MHz
#pragma config UPLLIDIV = DIV_2    // USB PLL Input Divider - 4MHz output
#pragma config FCKSM    = CSDCMD   // clock switching and fail safe monitor: clock switching disabled, clock monitoring disabled
#pragma config OSCIOFNC = OFF      // CLKO enable: Disabled
#pragma config IESO     = OFF      // internal/external switch-over: disabled
#pragma config FSOSCEN  = OFF      // secondary oscillator enable: disabled
#pragma config FPBDIV   = DIV_1    // peripheral clock divisor: /1
#pragma config FWDTEN   = ON       // watchdog timer: enabled
#pragma config WDTPS    = PS1024   // watchdog timer postscale: 1024:1
#pragma config CP       = ON       // Code Protect: ON
#pragma config BWP      = OFF      // Boot Flash Write Protect: OFF
#pragma config PWP      = OFF      // Program Flash Write Protect: OFF
#pragma config IOL1WAY	= OFF	   // reconfigure PPS is allowed more than once
#pragma config PMDL1WAY	= OFF	   // reconfigure the peripheral module disable is allowed more than once
#pragma config DEBUG    = OFF      // Background Debugger Enable: OFF

// running vars
int timer_div;  // divide counter for running timer tasks
int power_state;

#define STARTUP_DELAY_TIMEOUT 2000

// main!
int main(void) {
	// enable multi-vectored interrupts
	INTEnableSystemMultiVectoredInt();
		
	// enable optimal performance
	SYSTEMConfigPerformance(GetSystemClock());
	mOSCSetPBDIV(OSC_PB_DIV_1);  // Use 1:1 CPU core:peripheral clocks
	DelayMs(50);
	DDPCONbits.JTAGEN = 0;  // disable JTAG

	// disable all analog
	ANSELA = 0x0000;
	ANSELB = 0x0000;
	ANSELC = 0x0000;

	// unused I/Os set as output
	PORTSetPinsDigitalOut(IOPORT_A, BIT_10);
	LATAbits.LATA10 = 0;

	// configure timer - task timer - 250us period
	OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_256, 39);
	ConfigIntTimer1(T1_INT_ON | T1_INT_PRIOR_1);

	// sample clock timer / interrupt - 10kHz
	OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_64, 62);
	ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);

	// set up modules
	ioctl_init();
	dac_init();
	env_proc_init();  // ioctl and dac must be set up already

	// last thing before run
	INTDisableInterrupts();
    INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);  // multi-vector mode
	INTSetVectorPriority(INT_TIMER_1_VECTOR, INT_PRIORITY_LEVEL_1);  // timer 1 prio 1
    INTSetVectorPriority(INT_TIMER_2_VECTOR, INT_PRIORITY_LEVEL_2);  // timer 2 prio 2	INTEnable(INT_SOURCE_TIMER(TMR1), INT_ENABLED);  // timer 1 interrupt
	INTEnable(INT_SOURCE_TIMER(TMR2), INT_ENABLED);  // timer 2 interrupt
    INTEnableInterrupts();

	// main loop!
	while(1) {
		ClearWDT();
	}
}

//
// INTERRUPT VECTORS
//
// task timer
void __ISR(_TIMER_1_VECTOR, ipl1) Timer1Handler(void) {
	int temp;
    static int startup_delay = STARTUP_DELAY_TIMEOUT;
	INTClearFlag(INT_T1);

	// run always
	ioctl_timer_task();
	
	temp = ioctl_get_analog_power_ctrl();
	if(power_state != temp) {
		power_state = temp;
	}
	// we are off - force everything off
	if(!power_state) {
		ioctl_set_gate_led(0, 0);
		ioctl_set_gate_led(1, 0);
		ioctl_set_mode_led(0, 0, 0);
		ioctl_set_mode_led(0, 1, 0);
		ioctl_set_mode_led(0, 2, 0);
		ioctl_set_mode_led(1, 0, 0);
		ioctl_set_mode_led(1, 1, 0);
		ioctl_set_mode_led(1, 2, 0);
		dac_write_dac(0, DAC_VAL_OFF);
		dac_write_dac(1, DAC_VAL_OFF);
		dac_write_dac(2, DAC_VAL_OFF);
		dac_write_dac(3, DAC_VAL_OFF);
        startup_delay = STARTUP_DELAY_TIMEOUT;
	}
    // startup delay
    else if(startup_delay) {
        startup_delay --;
        // lamp test
		ioctl_set_gate_led(0, 1);
		ioctl_set_gate_led(1, 1);
		ioctl_set_mode_led(0, 0, 1);
		ioctl_set_mode_led(0, 1, 1);
		ioctl_set_mode_led(0, 2, 1);
		ioctl_set_mode_led(1, 0, 1);
		ioctl_set_mode_led(1, 1, 1);
		ioctl_set_mode_led(1, 2, 1);
		dac_write_dac(0, DAC_VAL_OFF);
		dac_write_dac(1, DAC_VAL_OFF);
		dac_write_dac(2, DAC_VAL_OFF);
		dac_write_dac(3, DAC_VAL_OFF);
        // init stuff at the end
        if(startup_delay == 0) {
			env_proc_init();
        }
    }
	// we are on - run normally
	else {
		// 1ms
		if((timer_div & 0x03) == 0) {
			env_proc_timer_task();
		}
	}

	timer_div ++;
}

// timer 2 interrupt - sample clock
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void) {
	INTClearFlag(INT_T2);
#ifdef TIMER2_INT_DEBUG
    LATAbits.LATA10 = 1;
#endif
	if(power_state) {
		env_proc_sample_task();
	}
#ifdef TIMER2_INT_DEBUG
    LATAbits.LATA10 = 0;
#endif
}
