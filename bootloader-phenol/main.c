/*
 * K65 Phenol - Mixer Bootloader
 *
 * Copyright 2015: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 * Hardware I/O:
 *
 * Pins used for bootloader:
 *	RA2			- osc1						- crystal
 *	RA3			- osc2						- crystal
 *	RA7			- analog pwr ctrl			- output - active high
 *	RB10		- USB D+					- USB data
 *	RB11		- USB D-					- USB data
 *
 *	RC5			- mixer master L LED		- output - active high
 *	RC7			- mixer master R LED		- output - active high
 *
 * Pins unused for bootloader:
 *  RA0/AN0		- mixer master pot			- analog 
 *	RA1			- codec ADC dat				- SDI1
 *	RA4			- codec MCLK				- REFCLKO
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
 *	RB13/AN11	- DC in vsense				- analog
 *	RB14		- codec BCLK				- SCK1
 *	RB15		- MIDI DAC LED SCK			- SCK2
 *
 *	RC0/AN6		- mixer in 1 level pot		- analog
 *	RC1/AN7		- mixer in 2 level pot		- analog
 *	RC2/AN8		- mixer pan 1 pot			- analog
 *	RC3/AN12	- mixer pan 2 pot			- analog
 *	RC4			- MIDI LED CS				- output - active low
 *	RC6			- MIDI DAC LED MOSI			- SDO2
 *	RC8			- MIDI RX					- U2RX
 *	RC9			- MIDI DAC CS				- output - active low / !SS2
 *
 */
#include <plib.h>
#include <stdio.h>
#include "HardwareProfile.h"
#include "TimeDelay.h"
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
#pragma config CP       = ON       // Code Protect: ON
#pragma config BWP      = OFF      // Boot Flash Write Protect: OFF
//#pragma config BWP      = ON      // Boot Flash Write Protect: ON
#pragma config PWP      = OFF      // Program Flash Write Protect: OFF
//#pragma config PWP      = PWP16K   // Program Flash Write Protect: lower 64KB
#pragma config IOL1WAY	= OFF	   // reconfigure PPS is allowed more than once
#pragma config PMDL1WAY	= OFF	   // reconfigure the peripheral module disable is allowed more than once
#pragma config DEBUG    = OFF      // Background Debugger Enable: OFF
#pragma config FUSBIDIO = OFF      // USBID pin is GPIO
#pragma config FVBUSONIO = OFF   	// VBUSON pin is GPIO

// debug stuff
//#define DEBUG_FAKE_WRITES
//#define DEBUG_SYSEX_ECHO
//#define DEBUG_PRESS_BUTTON

// running vars
int timer_div;
int flashing;

// I/O stuff
#define ANALOG_POWER_CTRL LATAbits.LATA7
#define MASTER_LED_L LATCbits.LATC5
#define MASTER_LED_R LATCbits.LATC7
#define POWER_SW PORTAbits.RA8
#define REC_SW PORTBbits.RB0
#define PLAY_SW PORTBbits.RB1
#define DAC_LED_SCLK LATBbits.LATB15
#define DAC_LED_MOSI LATCbits.LATC6
#define LED_CS LATCbits.LATC4
#define DAC_CS LATCbits.LATC9

// protocol stuff
#define MMA_ID0 0x00
#define MMA_ID1 0x01
#define MMA_ID2 0x72
#define DEV_ID 0x48
#define CMD_FIRMWARE_LOAD 0x06
#define CMD_FIRMWARE_OK 0x07
#define CMD_FIRMWARE_BLANK 0x08
#define CMD_FIRMWARE_BLANKED 0x09
#define CMD_RESET_DEVICE 0x7e  // used to respond with BOOTLOADER_ALIVE
#define CMD_BOOTLOADER_ALIVE 0x7f
#define TX_MSG_MAX 256

// ROM addresses
#define USER_APP_ADDR 		0x9d00b000
#define PROG_BASE_ADDR		0x9d00b000
#define PROG_TOP_ADDR       0x9d01ffff
#define FLASH_PAGE_SIZE		1024
#define FLASH_CHUNK_SIZE	64

// local functions
void jump_to_app();
void write_chunk(unsigned int segaddr, unsigned char flash_buf[]);
void blank_progmem(void);

// main!
int main(void) {
    int booting = 1;
    int i;
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

    // set up I/O needed in bootloader
    PORTSetPinsDigitalOut(IOPORT_A, BIT_7);
    PORTSetPinsDigitalOut(IOPORT_C, BIT_5 | BIT_7);
	PORTSetPinsDigitalIn(IOPORT_A, BIT_8 | BIT_10);    CNPUA = 0x0100;  // power switch
    CNPUB = 0x0003;  // rec and play switch
    MASTER_LED_L = 0;
    MASTER_LED_R = 0;
    ANALOG_POWER_CTRL = 0;  // force analog and mod controller disabled

    // shift register stuff
    PORTSetPinsDigitalOut(IOPORT_B, BIT_15);
    PORTSetPinsDigitalOut(IOPORT_C, BIT_4 | BIT_6 | BIT_9);
    DAC_LED_SCLK = 0;
    DAC_LED_MOSI = 0;
    DAC_CS = 1;
    LED_CS = 1;
    // fill shift register with 0s
    DAC_LED_SCLK = 0;
    LED_CS = 0;
    Delay10us(1);
    for(i = 0; i < 8; i ++) {
        DAC_LED_SCLK = 0;
        Delay10us(1);
        DAC_LED_SCLK = 1;
        Delay10us(1);
    }
    LED_CS = 1;

	// set up interrupts
	INTDisableInterrupts();
    // no interrupts are used for bootloader

    // boot loop - choose if we should go to bootloader or just boot app
    timer_div = 0;
    while(booting) {
		ClearWDT();        
       	DelayMs(100);

//        // if user holds down the power button then we do the bootloader
//        if(!POWER_SW) {
//            booting = 0;  // fall through to main loop
//        }

        // if user holds down the rec and play switches then we do the bootloader
        if(!REC_SW && !PLAY_SW) {
            booting = 0;  // fall through to main loop
        }

#ifdef DEBUG_PRESS_BUTTON
        // XXX debug
        if(timer_div == 1) {
            booting = 0;  // fake that we pressed the button
        }
#endif

        // if we timed out just try to run the app
//        if(timer_div == 5) {
        if(timer_div == 2) {
            jump_to_app();
        }
        timer_div ++;
    }

    // connect to USB
    usb_ctrl_init();
	midi_init(DEV_ID);  // K65 device code

    // set up the bootloader
	flashing = 0;

	// main loader loop
	while(1) {
		ClearWDT();

		// poll USB stuff
		// - receive USB MIDI messages and stick them into the MIDI RX routines
		// - get TX data to send and possibly send off a message
		usb_ctrl_poll();
        // process and deliver received messages
        midi_rx_task(MIDI_PORT_USB);
        while(midi_rx_task(MIDI_PORT_USB)) {
    		ClearWDT();
        }
    	Delay10us(10);

        // light dance - flashing
        if(flashing) {
            MASTER_LED_L = 1;
            if((timer_div & 0x200) == 0) {
                MASTER_LED_R = 0;
            }
            else {
                MASTER_LED_R = 1;
            }
        }
        // light dance - idle
        else {
            if((timer_div & 0x200) == 0) {
                MASTER_LED_L = 1;
                MASTER_LED_R = 0;
            }
            else {
                MASTER_LED_L = 0;
                MASTER_LED_R = 1;
            }
        }
        timer_div ++;
	}
}

// write a chunk to flash memory
void write_chunk(unsigned int segaddr, unsigned char flash_buf[]) {
	unsigned int i;
	unsigned int tempw;
	unsigned char chksum;
    unsigned char tx_msg[TX_MSG_MAX];

    // make sure the data is within our desired range for the program memory
    if(((segaddr | 0x80000000) >= PROG_BASE_ADDR) &&
            ((segaddr | 0x80000000) < PROG_TOP_ADDR)) {
    	// write the chunk to flash
	    for(i = 0; i < FLASH_CHUNK_SIZE; i += 4) {
		    tempw = flash_buf[i] | 
			    	(flash_buf[i + 1] << 8) | 
				    (flash_buf[i + 2] << 16) |
    				(flash_buf[i + 3] << 24);
#ifndef DEBUG_FAKE_WRITES
	    	NVMWriteWord((void *)((segaddr + i) | 0x80000000), tempw);
#endif
    	}
    }

	// calculate the checksum of the flash buffer (PC will check it)
    chksum = 0;
	for(i = 0; i < FLASH_CHUNK_SIZE; i ++) {
		chksum = (chksum + flash_buf[i]) & 0x7f;
	}
	// send a the OK status / checksum
    i = 0;
    tx_msg[i++] = MMA_ID0;
    tx_msg[i++] = MMA_ID1;
    tx_msg[i++] = MMA_ID2;
    tx_msg[i++] = CMD_FIRMWARE_OK;
    tx_msg[i++] = chksum;
    _midi_tx_sysex_msg(MIDI_PORT_USB, tx_msg, i);
}

// blank progmem
void blank_progmem(void) {
    unsigned int i;
    unsigned char tx_msg[TX_MSG_MAX];

	// blank the program memory
    for(i = PROG_BASE_ADDR; i < PROG_TOP_ADDR; i += FLASH_PAGE_SIZE) {
        NVMErasePage((void *)i);
    }

	// send a the OK status
    i = 0;
    tx_msg[i++] = MMA_ID0;
    tx_msg[i++] = MMA_ID1;
    tx_msg[i++] = MMA_ID2;
    tx_msg[i++] = CMD_FIRMWARE_BLANKED;
    _midi_tx_sysex_msg(MIDI_PORT_USB, tx_msg, i);
}

// jump to the application starting position
void jump_to_app() {	
	void (*fptr)(void);
	fptr = (void (*)(void))USER_APP_ADDR;
	fptr();
}

//
// MIDI callbacks
//
//
// SETUP MESSAGES
//
// MIDI send activity - for blinking LED
void _midi_send_act(unsigned char port) {
}

// MIDI receive activity - for blinking LED
void _midi_receive_act(unsigned char port) {
}

//
// CHANNEL MESSAGES
//
// note off - note on with velocity = 0 calls this
void _midi_rx_note_off(unsigned char port,
		unsigned char channel, 
		unsigned char note) {
}

// note on - note on with velocity > 0 calls this
void _midi_rx_note_on(unsigned char port,
		unsigned char channel, 
		unsigned char note, 
		unsigned char velocity) {
}

// key pressure
void _midi_rx_key_pressure(unsigned char port,
		unsigned char channel, 
		unsigned char note,
		unsigned char pressure) {
}

// control change - called for controllers 0-119 only
void _midi_rx_control_change(unsigned char port,
		unsigned char channel,
		unsigned char controller,
		unsigned char value) {
}

// channel mode - all sounds off
void _midi_rx_all_sounds_off(unsigned char port,
		unsigned char channel) {
}

// channel mode - reset all controllers
void _midi_rx_reset_all_controllers(unsigned char port,
		unsigned char channel) {
}

// channel mode - local control
void _midi_rx_local_control(unsigned char port,
		unsigned char channel, 
		unsigned char value) {
}

// channel mode - all notes off
void _midi_rx_all_notes_off(unsigned char port,
		unsigned char channel) {
}

// channel mode - omni off
void _midi_rx_omni_off(unsigned char port,
		unsigned char channel) {
}

// channel mode - omni on
void _midi_rx_omni_on(unsigned char port,
		unsigned char channel) {
}

// channel mode - mono on
void _midi_rx_mono_on(unsigned char port,
		unsigned char channel) {
}

// channel mode - poly on
void _midi_rx_poly_on(unsigned char port,
		unsigned char channel) {
}

// program change
void _midi_rx_program_change(unsigned char port,
		unsigned char channel,
		unsigned char program) {
}

// channel pressure
void _midi_rx_channel_pressure(unsigned char port,
		unsigned char channel,
		unsigned char pressure) {
}

// pitch bend
void _midi_rx_pitch_bend(unsigned char port,
		unsigned char channel,
		int bend) {
}

//
// SYSTEM COMMON MESSAGES
//
// song position
void _midi_rx_song_position(unsigned char port,
		unsigned int pos) {
}

// song select
void _midi_rx_song_select(unsigned char port,
		unsigned char song) {
}

//
// SYSEX MESSAGES
//
// sysex message received
void _midi_rx_sysex_msg(unsigned char port,
		unsigned char data[], 
		unsigned char len) {
    unsigned char tx_msg[TX_MSG_MAX];
#ifdef DEBUG_SYSEX_ECHO
    // echo
    _midi_tx_sysex_msg(port, data, len);
#else
    int i, outcount;
    int addr;
    unsigned char buf[128];
    // check if we have enough data
    if(len < 4) {
        return;
    }
    // check that the MMA ID matches
    if(data[0] != MMA_ID0) {
        return;
    }
    if(data[1] != MMA_ID1) {
        return;
    }
    if(data[2] != MMA_ID2) {
        return;
    }
    // parse the command
    switch(data[3]) {
        case CMD_FIRMWARE_LOAD:  // load a block of data
            if(len != 140) {
                return;
            }
            // buf address
            addr = (data[4] & 0x0f) << 28;
            addr |= (data[5] & 0x0f) << 24;
            addr |= (data[6] & 0x0f) << 20;
            addr |= (data[7] & 0x0f) << 16;
            addr |= (data[8] & 0x0f) << 12;
            addr |= (data[9] & 0x0f) << 8;
            addr |= (data[10] & 0x0f) << 4;
            addr |= (data[11] & 0x0f);
            // get the data bytes
            outcount = 0;
            for(i = 0; i < 128; i += 2) {
                buf[outcount] = ((data[i + 12] & 0x0f) << 4) | (data[i + 12 + 1] & 0x0f);
                outcount ++;
            }
            // flash the chunk
            write_chunk(addr, buf);
            break;
        case CMD_FIRMWARE_BLANK:  // erase the program memory
            blank_progmem();
            break;
        case CMD_RESET_DEVICE:  // reset the device
            // send bootloader alive status
            i = 0;
            tx_msg[i++] = MMA_ID0;
            tx_msg[i++] = MMA_ID1;
            tx_msg[i++] = MMA_ID2;
            tx_msg[i++] = CMD_BOOTLOADER_ALIVE;
            _midi_tx_sysex_msg(MIDI_PORT_USB, tx_msg, i);
            break;
    }
#endif
}

//
// SYSTEM REALTIME MESSAGES
//
// timing tick
void _midi_rx_timing_tick(unsigned char port) {
}

// start song
void _midi_rx_start_song(unsigned char port) {
}

// continue song
void _midi_rx_continue_song(unsigned char port) {
}

// stop song
void _midi_rx_stop_song(unsigned char port) {
}

// active sensing
void _midi_rx_active_sensing(unsigned char port) {
}

// system reset
void _midi_rx_system_reset(unsigned char port) {
}

//
// SPECIAL CALLBACKS
//
void _midi_restart_device(void) {
}