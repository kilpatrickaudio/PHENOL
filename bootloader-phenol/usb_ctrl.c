/*
 * USB Controller - Handles USB Setup and Callbacks
 *
 * Copyright 2012: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#include "usb_ctrl.h"
#include "midi.h"
#include "USB/usb.h"
#include "USB/usb_function_midi.h"
#include "TimeDelay.h"

// USB 
unsigned char ReceivedDataBuffer[64];
USB_AUDIO_MIDI_EVENT_PACKET midiData;
USB_HANDLE USBTxHandle = 0;
USB_HANDLE USBRxHandle = 0;
int usb_detect_timeout;
// transmit message state
int tx_data_len;
int tx_data_count;
int tx_sysex_data;

// init the USB
void usb_ctrl_init(void) {
	usb_detect_timeout = 0;
    USBTxHandle = NULL;
    USBRxHandle = NULL;
    USBDeviceInit();
	// reset transmitter stuff
	tx_data_len = -1;
	tx_data_count = 0;
	tx_sysex_data = 0;
}

// poll the USB and handle transactions - call this at least every 1ms on the main loop
void usb_ctrl_poll(void) {
	int rx_msg_len, rx_packet_len;
    int i, j;
    int tx_byte, tx_status, tx_chan;
	// run USB subsystem tasks
	USBDeviceTasks();

	// if we're not configured or suspended, get out of here
   	if((USBDeviceState < CONFIGURED_STATE) || (USBSuspendControl == 1)) {
		return;
	}

   	// we have received a MIDI packet from the host
	if(USBRxHandle != NULL && !USBHandleBusy(USBRxHandle) && usb_detect_timeout) {
        // handle each message that might be in the packet
        rx_packet_len = USBHandleGetLength(USBRxHandle);
        for(j = 0; j < rx_packet_len; j += 4) {
    		// determine the length of our message
    		switch(ReceivedDataBuffer[j]) {
			    case MIDI_CIN_2_BYTE_MESSAGE:
				    rx_msg_len = 2;
				    break;
    			case MIDI_CIN_3_BYTE_MESSAGE:
	    			rx_msg_len = 3;
		    		break;
    			case MIDI_CIN_SYSEX_START:
	    			rx_msg_len = 3;
		    		break;
			    case MIDI_CIN_1_BYTE_MESSAGE:
    				rx_msg_len = 1;
	    			break;
		    	case MIDI_CIN_SYSEX_ENDS_2:
			    	rx_msg_len = 2;
    				break;
	    		case MIDI_CIN_SYSEX_ENDS_3:
		    		rx_msg_len = 3;
			    	break;
    			case MIDI_CIN_NOTE_OFF:
	    			rx_msg_len = 3;
		    		break;
    			case MIDI_CIN_NOTE_ON:
	    			rx_msg_len = 3;
		    		break;
    			case MIDI_CIN_POLY_KEY_PRESS:
	    			rx_msg_len = 3;
		    		break;
    			case MIDI_CIN_CONTROL_CHANGE:
	    			rx_msg_len = 3;
		    		break;
    			case MIDI_CIN_PROGRAM_CHANGE:
	    			rx_msg_len = 2;
		    		break;
			    case MIDI_CIN_CHANNEL_PREASURE:
    				rx_msg_len = 2;
	    			break;
		    	case MIDI_CIN_PITCH_BEND_CHANGE:
			    	rx_msg_len = 3;
    				break;
	    		case MIDI_CIN_SINGLE_BYTE:
		    		rx_msg_len = 1;
			    	break;
			    default:
				    rx_msg_len = 0;
		    };

		    // make sure we only hear cable number 0 - make sure we actually have data
		    if((ReceivedDataBuffer[j] & 0xf0) == 0 && rx_msg_len > 0) {
		    	// send MIDI RX data to MIDI system
			    for(i = 0; i < rx_msg_len; i ++) {
				    midi_rx_byte(MIDI_PORT_USB, ReceivedDataBuffer[j + i + 1]);
			    }
		    }
        }
        // get ready for next packet (this will overwrite the old data)
       	USBRxHandle = USBRxOnePacket(MIDI_EP,(BYTE*)&ReceivedDataBuffer, 64);
   	}

    // process and deliver messages to the host
    while(!USBHandleBusy(USBTxHandle) && usb_detect_timeout &&
            midi_tx_avail(MIDI_PORT_USB)) {
	    // get next data byte from the TX FIFO
		// note that these message are always intact:
		// - no need to worry about running status
		// - no system realtime messages interleaved inside other messages
		tx_byte = midi_tx_get_byte(MIDI_PORT_USB);
		// we have a status byte
		if(tx_byte & 0x80) {
			// system common / realtime messages
			if((tx_byte & 0xf0) == 0xf0) {
				tx_status = tx_byte;
				tx_chan = 0;
			}
			// channel messages
			else {
				tx_status = tx_byte & 0xf0;  // clear channel bits
				tx_chan = tx_byte & 0x0f;  // extract channel number
			}
			switch(tx_status) {
				case MIDI_NOTE_OFF:
					tx_data_len = 3;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_NOTE_OFF;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_NOTE_ON:
					tx_data_len = 3;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_NOTE_ON;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_KEY_PRESSURE:
					tx_data_len = 3;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_POLY_KEY_PRESS;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_CONTROL_CHANGE:
					tx_data_len = 3;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_CONTROL_CHANGE;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_PROG_CHANGE:
					tx_data_len = 2;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_PROGRAM_CHANGE;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_CHAN_PRESSURE:
					tx_data_len = 2;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_CHANNEL_PREASURE;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_PITCH_BEND:
					tx_data_len = 3;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_PITCH_BEND_CHANGE;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_SONG_POSITION:
					tx_data_len = 3;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_SSP;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_SONG_SELECT:
					tx_data_len = 2;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_SONG_SELECT;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_SYSEX_START:
					tx_data_len = 3;
					tx_data_count = 1;
					tx_sysex_data = 1;  // following messages might not have status
					midiData.CodeIndexNumber = MIDI_CIN_SYSEX_START;
					midiData.DATA_0 = tx_byte;  // status + channel
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;						
					break;
				case MIDI_SYSEX_END:
					// we have 1 data byte in position 0
					if(tx_data_count == 1)  {
						tx_data_len = 2;
						tx_data_count = 2;
						midiData.CodeIndexNumber = MIDI_CIN_SYSEX_ENDS_2;
						midiData.DATA_1 = tx_byte;
						midiData.DATA_2 = 0;
					}
					// we have 2 data bytes in position 0 and 1
					else if(tx_data_count == 2) {
						tx_data_len = 3;
						tx_data_count = 3;
						midiData.CodeIndexNumber = MIDI_CIN_SYSEX_ENDS_3;
						midiData.DATA_2 = tx_byte;
					}
					// we have no data bytes
					else {
						tx_data_len = 1;
						tx_data_count = 1;
						midiData.CodeIndexNumber = MIDI_CIN_SYSEX_ENDS_1;
						midiData.DATA_0 = tx_byte;							
						midiData.DATA_1 = 0;
						midiData.DATA_2 = 0;						
					}
					tx_sysex_data = 0;
					break;
				case MIDI_TIMING_TICK:
					tx_data_len = 1;
					tx_data_count = 1;
					tx_sysex_data = 0;
					midiData.CodeIndexNumber = MIDI_CIN_1_BYTE_MESSAGE;
					midiData.DATA_0 = MIDI_TIMING_TICK;
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_START_SONG:
					tx_data_len = 1;
					tx_data_count = 1;
					midiData.CodeIndexNumber = MIDI_CIN_1_BYTE_MESSAGE;
					midiData.DATA_0 = MIDI_START_SONG;
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_CONTINUE_SONG:
					tx_data_len = 1;
					tx_data_count = 1;
					midiData.CodeIndexNumber = MIDI_CIN_1_BYTE_MESSAGE;
					midiData.DATA_0 = MIDI_CONTINUE_SONG;
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_STOP_SONG:
					tx_data_len = 1;
					tx_data_count = 1;
					midiData.CodeIndexNumber = MIDI_CIN_1_BYTE_MESSAGE;
					midiData.DATA_0 = MIDI_STOP_SONG;
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_ACTIVE_SENSING:
					tx_data_len = 1;
					tx_data_count = 1;
					midiData.CodeIndexNumber = MIDI_CIN_1_BYTE_MESSAGE;
					midiData.DATA_0 = MIDI_ACTIVE_SENSING;
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				case MIDI_SYSTEM_RESET:
					tx_data_len = 1;
					tx_data_count = 1;
					midiData.CodeIndexNumber = MIDI_CIN_1_BYTE_MESSAGE;
					midiData.DATA_0 = MIDI_SYSTEM_RESET;
					midiData.DATA_1 = 0;
					midiData.DATA_2 = 0;
					break;
				default:  // unsupported status types
					tx_data_len = -1;
					tx_data_count = 0;
					tx_sysex_data = 0;
			};
		}
		// we have a data byte - make sure we should be receiving now (tx_data_len > 0)
		else if(tx_data_len > 0) {
			if(tx_data_count == 0) {
				midiData.DATA_0 = tx_byte;
			}
			else if(tx_data_count == 1) {
				midiData.DATA_1 = tx_byte;
			}
			else if(tx_data_count == 2) {
				midiData.DATA_2 = tx_byte;
			}
			tx_data_count ++;
		}
		// is it time to package and send the message over USB?
		if(tx_data_count == tx_data_len) {
      		midiData.CableNumber = 0;
			// message data is already set above
			USBTxHandle = USBTxOnePacket(MIDI_EP, (BYTE*)&midiData, 4);
			// we are receiving sysex data bytes
			if(tx_sysex_data == 1) {
				tx_data_len = 3;
				tx_data_count = 0;
				midiData.CodeIndexNumber = MIDI_CIN_SYSEX_CONTINUE;
			}
			// we are receiving normal bytes
			else {
				tx_data_len = -1;
			}
		}
	}

	// USB detection
	if(usb_detect_timeout) {
		usb_detect_timeout --;
	}
}

// check to see if the USB is enabled - 0 = disabled, 1 = enabled
unsigned char usb_ctrl_get_enabled(void) {
	if(usb_detect_timeout) return 1;
	return 0;
}

// ******************************************************************************************************
// ************** USB Callback Functions ****************************************************************
// ******************************************************************************************************
// The USB firmware stack will call the callback functions USBCBxxx() in response to certain USB related
// events.  For example, if the host PC is powering down, it will stop sending out Start of Frame (SOF)
// packets to your device.  In response to this, all USB devices are supposed to decrease their power
// consumption from the USB Vbus to <2.5mA* each.  The USB module detects this condition (which according
// to the USB specifications is 3+ms of no bus activity/SOF packets) and then calls the USBCBSuspend()
// function.  You should modify these callback functions to take appropriate actions for each of these
// conditions.  For example, in the USBCBSuspend(), you may wish to add code that will decrease power
// consumption from Vbus to <2.5mA (such as by clock switching, turning off LEDs, putting the
// microcontroller to sleep, etc.).  Then, in the USBCBWakeFromSuspend() function, you may then wish to
// add code that undoes the power saving things done in the USBCBSuspend() function.

// The USBCBSendResume() function is special, in that the USB stack will not automatically call this
// function.  This function is meant to be called from the application firmware instead.  See the
// additional comments near the function.

// Note *: The "usb_20.pdf" specs indicate 500uA or 2.5mA, depending upon device classification. However,
// the USB-IF has officially issued an ECN (engineering change notice) changing this to 2.5mA for all 
// devices.  Make sure to re-download the latest specifications to get all of the newest ECNs.

/******************************************************************************
 * Function:        void USBCBSuspend(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Call back that is invoked when a USB suspend is detected
 *
 * Note:            None
 *****************************************************************************/
void USBCBSuspend(void) {
}

/******************************************************************************
 * Function:        void USBCBWakeFromSuspend(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The host may put USB peripheral devices in low power
 *					suspend mode (by "sending" 3+ms of idle).  Once in suspend
 *					mode, the host may wake the device back up by sending non-
 *					idle state signalling.
 *
 *					This call back is invoked when a wakeup from USB suspend
 *					is detected.
 *
 * Note:            None
 *****************************************************************************/
void USBCBWakeFromSuspend(void) {
}

/********************************************************************
 * Function:        void USBCB_SOF_Handler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The USB host sends out a SOF packet to full-speed
 *                  devices every 1 ms. This interrupt may be useful
 *                  for isochronous pipes. End designers should
 *                  implement callback routine as necessary.
 *
 * Note:            None
 *******************************************************************/
void USBCB_SOF_Handler(void) {
	// detect USB present
	usb_detect_timeout = 10;
}

/*******************************************************************
 * Function:        void USBCBErrorHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The purpose of this callback is mainly for
 *                  debugging during development. Check UEIR to see
 *                  which error causes the interrupt.
 *
 * Note:            None
 *******************************************************************/
void USBCBErrorHandler(void) {
}

/*******************************************************************
 * Function:        void USBCBCheckOtherReq(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        When SETUP packets arrive from the host, some
 * 					firmware must process the request and respond
 *					appropriately to fulfill the request.  Some of
 *					the SETUP packets will be for standard
 *					USB "chapter 9" (as in, fulfilling chapter 9 of
 *					the official USB specifications) requests, while
 *					others may be specific to the USB device class
 *					that is being implemented.  For example, a HID
 *					class device needs to be able to respond to
 *					"GET REPORT" type of requests.  This
 *					is not a standard USB chapter 9 request, and
 *					therefore not handled by usb_device.c.  Instead
 *					this request should be handled by class specific
 *					firmware, such as that contained in usb_function_hid.c.
 *
 * Note:            None
 *******************************************************************/
void USBCBCheckOtherReq(void) {
}

/*******************************************************************
 * Function:        void USBCBStdSetDscHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The USBCBStdSetDscHandler() callback function is
 *					called when a SETUP, bRequest: SET_DESCRIPTOR request
 *					arrives.  Typically SET_DESCRIPTOR requests are
 *					not used in most applications, and it is
 *					optional to support this type of request.
 *
 * Note:            None
 *******************************************************************/
void USBCBStdSetDscHandler(void) {
}

/*******************************************************************
 * Function:        void USBCBInitEP(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is called when the device becomes
 *                  initialized, which occurs after the host sends a
 * 					SET_CONFIGURATION (wValue not = 0) request.  This
 *					callback function should initialize the endpoints
 *					for the device's usage according to the current
 *					configuration.
 *
 * Note:            None
 *******************************************************************/
void USBCBInitEP(void) {
    //enable the HID endpoint
    USBEnableEndpoint(MIDI_EP,USB_OUT_ENABLED|USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);

    //Re-arm the OUT endpoint for the next packet
    USBRxHandle = USBRxOnePacket(MIDI_EP,(BYTE*)&ReceivedDataBuffer, 64);
}

/********************************************************************
 * Function:        void USBCBSendResume(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The USB specifications allow some types of USB
 * 					peripheral devices to wake up a host PC (such
 *					as if it is in a low power suspend to RAM state).
 *					This can be a very useful feature in some
 *					USB applications, such as an Infrared remote
 *					control	receiver.  If a user presses the "power"
 *					button on a remote control, it is nice that the
 *					IR receiver can detect this signalling, and then
 *					send a USB "command" to the PC to wake up.
 *					
 *					The USBCBSendResume() "callback" function is used
 *					to send this special USB signalling which wakes 
 *					up the PC.  This function may be called by
 *					application firmware to wake up the PC.  This
 *					function will only be able to wake up the host if
 *                  all of the below are true:
 *					
 *					1.  The USB driver used on the host PC supports
 *						the remote wakeup capability.
 *					2.  The USB configuration descriptor indicates
 *						the device is remote wakeup capable in the
 *						bmAttributes field.
 *					3.  The USB host PC is currently sleeping,
 *						and has previously sent your device a SET 
 *						FEATURE setup packet which "armed" the
 *						remote wakeup capability.   
 *
 *                  If the host has not armed the device to perform remote wakeup,
 *                  then this function will return without actually performing a
 *                  remote wakeup sequence.  This is the required behavior, 
 *                  as a USB device that has not been armed to perform remote 
 *                  wakeup must not drive remote wakeup signalling onto the bus;
 *                  doing so will cause USB compliance testing failure.
 *                  
 *					This callback should send a RESUME signal that
 *                  has the period of 1-15ms.
 *
 * Note:            This function does nothing and returns quickly, if the USB
 *                  bus and host are not in a suspended condition, or are 
 *                  otherwise not in a remote wakeup ready state.  Therefore, it
 *                  is safe to optionally call this function regularly, ex: 
 *                  anytime application stimulus occurs, as the function will
 *                  have no effect, until the bus really is in a state ready
 *                  to accept remote wakeup. 
 *
 *                  When this function executes, it may perform clock switching,
 *                  depending upon the application specific code in 
 *                  USBCBWakeFromSuspend().  This is needed, since the USB
 *                  bus will no longer be suspended by the time this function
 *                  returns.  Therefore, the USB module will need to be ready
 *                  to receive traffic from the host.
 *
 *                  The modifiable section in this routine may be changed
 *                  to meet the application needs. Current implementation
 *                  temporary blocks other functions from executing for a
 *                  period of ~3-15 ms depending on the core frequency.
 *
 *                  According to USB 2.0 specification section 7.1.7.7,
 *                  "The remote wakeup device must hold the resume signaling
 *                  for at least 1 ms but for no more than 15 ms."
 *                  The idea here is to use a delay counter loop, using a
 *                  common value that would work over a wide range of core
 *                  frequencies.
 *                  That value selected is 1800. See table below:
 *                  ==========================================================
 *                  Core Freq(MHz)      MIP         RESUME Signal Period (ms)
 *                  ==========================================================
 *                      48              12          1.05
 *                       4              1           12.6
 *                  ==========================================================
 *                  * These timing could be incorrect when using code
 *                    optimization or extended instruction mode,
 *                    or when having other interrupts enabled.
 *                    Make sure to verify using the MPLAB SIM's Stopwatch
 *                    and verify the actual signal on an oscilloscope.
 *******************************************************************/
void USBCBSendResume(void) {
}

/*******************************************************************
 * Function:        BOOL USER_USB_CALLBACK_EVENT_HANDLER(
 *                        int event, void *pdata, WORD size)
 *
 * PreCondition:    None
 *
 * Input:           int event - the type of event
 *                  void *pdata - pointer to the event data
 *                  WORD size - size of the event data
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is called from the USB stack to
 *                  notify a user application that a USB event
 *                  occured.  This callback is in interrupt context
 *                  when the USB_INTERRUPT option is selected.
 *
 * Note:            None
 *******************************************************************/
BOOL USER_USB_CALLBACK_EVENT_HANDLER(int event, void *pdata, WORD size) {
    switch( event ) {
        case EVENT_TRANSFER:
            //Add application specific callback task or callback function here if desired.
            break;
        case EVENT_SOF:
            USBCB_SOF_Handler();
            break;
        case EVENT_SUSPEND:
            USBCBSuspend();
            break;
        case EVENT_RESUME:
            USBCBWakeFromSuspend();
            break;
        case EVENT_CONFIGURED:
            USBCBInitEP();
            break;
        case EVENT_SET_DESCRIPTOR:
            USBCBStdSetDscHandler();
            break;
        case EVENT_EP0_REQUEST:
            USBCBCheckOtherReq();
            break;
        case EVENT_BUS_ERROR:
            USBCBErrorHandler();
            break;
        case EVENT_TRANSFER_TERMINATED:
            //Add application specific callback task or callback function here if desired.
            //The EVENT_TRANSFER_TERMINATED event occurs when the host performs a CLEAR
            //FEATURE (endpoint halt) request on an application endpoint which was 
            //previously armed (UOWN was = 1).  Here would be a good place to:
            //1.  Determine which endpoint the transaction that just got terminated was 
            //      on, by checking the handle value in the *pdata.
            //2.  Re-arm the endpoint if desired (typically would be the case for OUT 
            //      endpoints).
            break;
        default:
            break;
    }
    return TRUE;
}
