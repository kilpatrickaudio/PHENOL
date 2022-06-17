/*
 * K65 Phenol - Mixer Unit
 *
 * Copyright 2014: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 * Hardware I/O:
 *
 *  RA0/AN0		- mixer master pot			- analog
 *	RA1			- codec ADC dat				- SDI1
 *	RA2			- osc1						- crystal
 *	RA3			- osc2						- crystal
 *	RA4			- codec MCLK				- REFCLKO
 *	RA7			- analog pwr ctrl			- output - active high
 *	RA8			- power sw					- input - active low
 *	RA9			- codec DAC dat				- SDO1
 *	RA10		- divider in				- input - active low
 *
 *	RB0			- rec SW / PGD				- rec SW (active low) / ICP data
 *	RB1			- play SW / PGC				- play SW (active low) / ICP clock
 *	RB2/AN4		- mixer delay time pot		- analog
 *	RB3/AN5		- mixer delay mix pot		- analog
 *	RB4			- codec LRCLK				- !SS1
 *	RB5 		- MIDI gate out				- output - active high
 *	RB7			- MIDI clock out			- output - active high
 *	RB8			- codec I2C clock			- SCL1
 *	RB9			- codec I2C data			- SDA1
 *	RB10		- USB D+					- USB data
 *	RB11		- USB D-					- USB data
 *	RB13/AN11	- DC in vsense				- analog
 *	RB14		- codec BCLK				- SCK1
 *	RB15		- MIDI DAC LED SCK			- SCK2
 *
 *	RC0/AN6		- mixer in 1 level pot		- analog
 *	RC1/AN7		- mixer in 2 level pot		- analog
 *	RC2/AN8		- mixer pan 1 pot			- analog
 *	RC3/AN12	- mixer pan 2 pot			- analog
 *	RC4			- MIDI LED CS				- output - active low
 *	RC5			- mixer master L LED		- output - active high
 *	RC6			- MIDI DAC LED MOSI			- SDO2
 *	RC7			- mixer master R LED		- output - active high
 *	RC8			- MIDI RX					- U2RX
 *	RC9			- MIDI DAC CS				- output - active low / !SS2
 *
 */
#include <plib.h>
#include <stdio.h>
#include "HardwareProfile.h"
#include "TimeDelay.h"
#include "ioctl.h"
#include "phenol_midi.h"
#include "power_ctrl.h"
#include "audio_sys.h"
#include "WM8731_ctrl.h"
#include "audio_proc.h"
#include "pulse_div.h"
#include "cv_gate_ctrl.h"
#include "voice.h"
#include "seq.h"
#include "midi_clock.h"
#include "usb_ctrl.h"
#include "midi.h"

// fuse settings
#pragma config UPLLEN   = ON        // USB PLL Enabled
#pragma config FNOSC = PRIPLL  		// internal oscillator - crystal
#pragma config POSCMOD = XT 		// primary oscillator - XT mode
#pragma config FPLLIDIV = DIV_2    // PLL input divide by 2 - bring 8MHz down to 4MHz for PLL
#pragma config FPLLMUL  = MUL_20   // PLL multiplier - x20 = 80MHz
#pragma config FPLLODIV = DIV_2    // PLL output divider - /2 = 40MHz
#pragma config UPLLIDIV = DIV_2  	// USB PLL Input Divider - 4MHz output
#pragma config FCKSM    = CSDCMD   // clock switching and fail safe monitor: clock switching disabled, clock monitoring disabled
#pragma config OSCIOFNC = OFF      // CLKO enable: Disabled
#pragma config IESO     = OFF      // internal/external switch-over: disabled
#pragma config FSOSCEN  = OFF      // secondary oscillator enable: disabled
#pragma config FPBDIV   = DIV_1    // peripheral clock divisor: /1
#pragma config FWDTEN   = ON       // watchdog timer: enabled
#pragma config WDTPS    = PS1024   // watchdog timer postscale: 16384:1
#pragma config CP       = ON       // Code Protect: OFF
#pragma config BWP      = OFF      // Boot Flash Write Protect: OFF
#pragma config PWP      = OFF      // Program Flash Write Protect: OFF
#pragma config IOL1WAY	= OFF	   // reconfigure PPS is allowed more than once
#pragma config PMDL1WAY	= OFF	   // reconfigure the peripheral module disable is allowed more than once
#pragma config DEBUG    = OFF      // Background Debugger Enable: OFF
#pragma config FUSBIDIO = OFF      // USBID pin is GPIO
#pragma config FVBUSONIO = OFF   	// VBUSON pin is GPIO

#define STARTUP_DELAY_TIMEOUT 2000

// running vars
int timer_div;  // divide counter for running timer tasks

// main!
int main(void) {
	timer_div = 0;

	// enable multi-vectored interrupts
	INTEnableSystemMultiVectoredInt();
		
	// make sure we clear this on startup
	ClearWDT();

	// disable all analog	
	ANSELA = 0x0000;
	ANSELB = 0x0000;
	ANSELC = 0x0000;

	// enable optimal performance
	SYSTEMConfigPerformance(GetSystemClock());
	mOSCSetPBDIV(OSC_PB_DIV_1);  // Use 1:1 CPU core:peripheral clocks
	DelayMs(50);
	DDPCONbits.JTAGEN = 0;  // disable JTAG
	ClearWDT();

	// configure timer - task timer - 250us period
	OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_256, 39);	
	ConfigIntTimer1(T1_INT_ON | T1_INT_PRIOR_1);

	// set up modules
	ioctl_init();  // must be run before other init routines
	usb_ctrl_init();  // set up the USB stuff
	cv_gate_ctrl_init();  // must be run before voice_init()
	voice_init();
	phenol_midi_init();
	power_ctrl_init();
	audio_sys_init();  // must be run before WM8731_ctrl_init()
	WM8731_ctrl_init(); 
	audio_proc_init();
    midi_clock_init();
	pulse_div_init();
	seq_init();

	// MIDI port - UART2
	PPSUnLock;
	PPSInput(2, U2RX, RPC8);
	PPSLock;
	UARTConfigure(UART2, UART_ENABLE_PINS_TX_RX_ONLY);
    UARTSetFifoMode(UART2, UART_INTERRUPT_ON_RX_NOT_EMPTY);
	UARTSetLineControl(UART2, UART_DATA_SIZE_8_BITS | UART_PARITY_NONE | UART_STOP_BITS_1);
	UARTSetDataRate(UART2, GetPeripheralClock(), 31250);
	UARTEnable(UART2, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX));

	// set up interrupts
	INTDisableInterrupts();
    INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);  // multi-vector mode
	INTSetVectorPriority(INT_TIMER_1_VECTOR, INT_PRIORITY_LEVEL_2);  // timer 1 prio 2
    INTSetVectorPriority(INT_UART_2_VECTOR, INT_PRIORITY_LEVEL_1);  // UART2 prio 1
	INTEnable(INT_SOURCE_TIMER(TMR1), INT_ENABLED);  // timer 1 interrupt
	INTEnable(INT_SOURCE_UART_RX(UART2), INT_ENABLED);  // UART2 RX interrupt
    INTEnableInterrupts();

	// start audio streaming
	ClearWDT();
	DelayMs(50);
	WM8731_ctrl_start();
	DelayMs(50);
	audio_sys_start();

	// main loop!
	while(1) {
		ClearWDT();

		// poll USB stuff
		// - receive USB MIDI messages and stick them into the MIDI RX routines
		// - get TX data to send and possibly send off a message
		usb_ctrl_poll();
    	Delay10us(10);
	}
}

//
// INTERRUPT VECTORS
//
// task timer
void __ISR(_TIMER_1_VECTOR, ipl2) Timer1Handler(void) {
	static int power_state = POWER_STATE_BOOT;  // starting default
    static int startup_delay = STARTUP_DELAY_TIMEOUT;
	INTClearFlag(INT_T1);

	// run always
	ioctl_timer_task();

	// we are on - run normally
	if(power_state == POWER_STATE_ON) {
        // startup delay - lamp check
        if(startup_delay) {
            startup_delay --;
            ioctl_set_midi_rec_led(SEQ_LED_ON);
            ioctl_set_midi_play_led(SEQ_LED_ON);
		    ioctl_set_divider_outputs(1, 1, 1, 1);
            cv_gate_ctrl_note(0, NOTE_LOW, 1);
            ioctl_set_midi_clock_out(1);
            ioctl_set_midi_in_led(1);
            ioctl_set_mixer_delay_led(255);
	        ioctl_set_mixer_output_leds(255, 255);  // 1.23 fix for correct blinky
            if(startup_delay == 0) {
                ioctl_set_midi_rec_led(SEQ_LED_OFF);
                ioctl_set_midi_play_led(SEQ_LED_OFF);
		        ioctl_set_divider_outputs(0, 0, 0, 0);
                cv_gate_ctrl_note(0, 60, 1);
                cv_gate_ctrl_note(0, 60, 0);
                ioctl_set_midi_clock_out(0);
                ioctl_set_midi_in_led(0);
                ioctl_set_mixer_delay_led(0);
	            ioctl_set_mixer_output_leds(0, 0);
                phenol_midi_reset();
                seq_init();  // reinit the seq
            }
        }
        // normal processing
        else {
    		audio_proc_process();
	    	pulse_div_timer_task();
		    phenol_midi_timer_task();
    		midi_rx_task(MIDI_PORT_DIN);
	    	midi_rx_task(MIDI_PORT_USB);
		    // sequencer interval = 1ms
    		if((timer_div & 0x03) == 0) {
                midi_clock_timer_task();
		    	seq_timer_task();
    		}
#ifdef DEBUG_MIDI
	    	if((timer_div & 0xfff) == 0) {
		    	_midi_tx_active_sensing(MIDI_PORT_USB);
		    }
#endif
        }
	}
	// we are off - force everything off
	else {
		// error LED blinky
		if(power_state == POWER_STATE_ERROR) {
			ioctl_set_mixer_delay_led((timer_div >> 8) & 0x01);
			ioctl_set_mixer_output_leds((timer_div >> 8) & 0x01, (timer_div >> 8) & 0x01);
		}
		else {
			ioctl_set_mixer_delay_led(0);
			ioctl_set_mixer_output_leds(0, 0);
		}
		ioctl_set_midi_rec_led(SEQ_LED_OFF);
		ioctl_set_midi_play_led(SEQ_LED_OFF);
		ioctl_set_divider_outputs(0, 0, 0, 0);
		ioctl_set_midi_cv_out(0, DAC_VAL_NOM);
		ioctl_set_midi_cv_out(1, DAC_VAL_NOM);
		ioctl_set_midi_gate_out(0);
		ioctl_set_midi_clock_out(0);
        ioctl_set_midi_in_led(0);
        ioctl_set_mixer_output_leds(0, 0);
		audio_proc_silence();
        startup_delay = STARTUP_DELAY_TIMEOUT;
	}

	// 64ms - power control
	if((timer_div & 0xff) == 0) {
		power_ctrl_timer_task();
		power_state = power_ctrl_get_state();
	}

	timer_div ++;
}

// MIDI RX interrupt
void __ISR(_UART2_VECTOR, ipl1) IntUart2Handler(void) {
	// is this an RX interrupt?
	if(INTGetFlag(INT_U2RX)) {
		INTClearFlag(INT_U2RX);
	 	if(!UARTReceivedDataIsAvailable(UART2)) return;  // required to prevent junk reception of 0x00
		midi_rx_byte(MIDI_PORT_DIN, UARTGetDataByte(UART2));
		U2STAbits.OERR = 0;
	}
}
