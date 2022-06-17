/*********************************************************************
 *
 *                  PIC32 Boot Loader
 *
 *********************************************************************
 * FileName:        main.c
 * Dependencies:
 * Processor:       PIC32
 *
 * Complier:        MPLAB C32
 *                  MPLAB IDE
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the “Company”) for its PIC32 Microcontroller is intended
 * and supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip PIC32 Microcontroller products.
 * The software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *
 * $Id:  $
 * $Name: $
 *
 **********************************************************************/
#include <stdlib.h>
#include <plib.h>
#include "GenericTypeDefs.h"
#include "msg_cmd.h"

// 80Mhz Core/Periph, Pri Osc w/PLL, Write protect Boot Flash
#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2, FPLLODIV = DIV_1, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_1
#pragma config ICESEL = ICS_PGx2, BWP = ON


#define BL_CSUM_ADDR 		0x9D0017F4
#define BL_PROGSZ_ADDR 		0x9D0017F0
#define FLASH_PROG_BASE		0x9D000000
#define USER_APP_ADDR 		0x9D001000
#define FLASH_PAGE_SIZE		4096


#define BL_MINOR_VERSION 1
#define BL_MAJOR_VERSION 0


#define SYS_FREQ 				(80000000L)

#define mLED_1              LATAbits.LATA2
#define mLED_2              LATAbits.LATA3

#define SECONDS	(SYS_FREQ/2)
#define MILLI_SECONDS	(SECONDS/1000)

#define MAX_DATA_BFR 4200

UINT32 gBuffer[MAX_DATA_BFR/4];

void PutChar(BYTE txChar);
void HandleCommand();
BYTE gRxData;
void PutResponse(WORD responseLen);

////////////////////////////////////////////////////////////
BOOL GetData()
{
	BYTE dummy;
	
	//check for receive errors
	if((U2STA & 0x000E) != 0x0000)
	{
		dummy = U2RXREG; 			//dummy read to clear FERR/PERR
		U2STAbits.OERR = 0;			//clear OERR to keep receiving
	}
	
	//get the data
	if(U2STA&1)
	{
		gRxData = U2RXREG;		//get data from UART RX FIFO
		return TRUE;
	}
	else
		return FALSE;
}

////////////////////////////////////////////////////////////
void JumpToApp()
{	
	void (*fptr)(void);
	fptr = (void (*)(void))USER_APP_ADDR;
	fptr();
}	

////////////////////////////////////////////////////////////
BOOL VerifyFlash()
{
	DWORD temp, ProgSz;
	DWORD* fptr;
	DWORD csum = 0;

	fptr = (DWORD*)BL_PROGSZ_ADDR;

	ProgSz = *fptr;

	if( ProgSz > BMXPFMSZ )
		return FALSE;

	fptr = (DWORD*)FLASH_PROG_BASE;

	for( temp = 0; temp < ProgSz/4; temp++ )
		csum += fptr[temp];
		
	csum = ~csum + 1;

	return (csum == -1);
}


#define BlinkLED() (mLED_1 = ((ReadCoreTimer() & 0x0200000) != 0))


////////////////////////////////////////////////////////////
int main(void)
{
	unsigned int pb_clock;
	
	SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);

	mCTClearIntFlag();
    OpenCoreTimer(0.5*SECONDS);

	//setup LED ports
	TRISAbits.TRISA2 = 0;

	BOOL FlashGood = FALSE;
	
	//check flash contents
	FlashGood = VerifyFlash();

	// SETUP UART COMMS: No parity, one stop bit, autobaud, polled
	OpenUART2(UART_EN|UART_EN_ABAUD|UART_BRGH_FOUR, UART_RX_ENABLE | UART_TX_ENABLE, 0 );	
	
	U2MODEbits.ABAUD = 1;		//set autobaud mode

	while(U2MODEbits.ABAUD  )		//wait for sync character U	
	{
		if( FlashGood && mCTGetIntFlag() )
		{
			//timeout while waiting for sync from pc - jump to user app
			JumpToApp();
		}
			
		BlinkLED();		
	} 		

   	mPORTAToggleBits(BIT_3);

	BYTE checksum;
	WORD dataCount;
	BYTE* pBfr = (BYTE*)gBuffer;
	 	
	checksum = 0;	//reset data and checksum
	dataCount = 0;

	while(1)
	{	
		while( !GetData() )
			BlinkLED();
				
		switch(gRxData)
		{   
			case STX: 			//Start over if STX
				checksum = 0;
				dataCount = 0;
				PutChar(STX);
				break;

			case ETX:			//End of packet if ETX
				checksum = ~checksum +1; //test checksum
				
				if(checksum == 0)  //good packet
					HandleCommand();
				
				checksum = 0;
				dataCount = 0;	//restart
				U2MODEbits.ABAUD = 1;
				break;

			case DLE:			//If DLE, treat next as data
			
				while( !GetData() ) //wait for next byte
					BlinkLED();
				
				//let it fall thru.....
				
			default:			//get data, put in buffer
			
				if( dataCount < MAX_DATA_BFR )
				{
					checksum += gRxData;
					pBfr[dataCount++] = gRxData;
				}	
						
				break;
		}			
	}
		
	return 0;
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
void HandleCommand()
{
	
	WORD_VAL Command;
	WORD_VAL length;
	
	DWORD sourceAddr;
	WORD responseBytes = 0;

	BYTE* pBfr;
	pBfr = (BYTE*)gBuffer;

	Command.byte.LB = pBfr[0];	//get command from gBuffer	
	Command.byte.HB = pBfr[1];
	
	length.byte.LB = pBfr[2];	//get data length from gBuffer
	length.byte.HB = pBfr[3];
	
	if( length.Val == 0 )  	//Go to user app
	{
		JumpToApp();
	}

	//get 32-bit address from gBuffer
	sourceAddr = gBuffer[1];		

	int len;

	//Handle Commands		
	switch(Command.Val)
	{
		//-=-=-=-=-=-=-=-=-=-=-=-=-=-=
		case RD_VER:						//Read version	

			pBfr[0] = BMXPFMSZ & 0xFF;
			pBfr[1] = (BMXPFMSZ >> 8) & 0xFF;
			pBfr[2] = (BMXPFMSZ >> 16) & 0xFF;
			pBfr[3] = (BMXPFMSZ >> 24) & 0xFF;
			
			pBfr[4] = BL_MINOR_VERSION;
			pBfr[5] = BL_MAJOR_VERSION;
			pBfr[6] = (DEVID >> 12) & 0xFF;
			
			responseBytes = 12; //set length of reply
			break;
		
		//-=-=-=-=-=-=-=-=-=-=-=-=-=-=
		case RD_FLASH:		//Read flash memory. must be 32bit alligned
		{
			int temp;
			DWORD*pDat = (DWORD*)gBuffer[1];
			
			//len is number of 32bit words to read
			
			for( temp = 0; temp < length.Val; temp++ )
			{
				if( &pDat[temp] == (DWORD*)BL_CSUM_ADDR || 
				    &pDat[temp] == (DWORD*)BL_PROGSZ_ADDR )
					gBuffer[2+temp] = -1;
				else
					gBuffer[2+temp] = pDat[temp];
			}	

			responseBytes = (length.Val*4) + 8; //set length of reply
			break;
		}
		
		//-=-=-=-=-=-=-=-=-=-=-=-=-=-=
		case WT_FLASH:
		case WT_FLASH_FF:
		{			
			//write 32bit words. len is number of words. 
			//Write Data must be multiple of 128 inst words (1 row, 512 bytes)

			if( length.Val == 0 )
			{
				responseBytes = 2; //set length of reply
	 			break;
	 		}		

			int RowCount, nRow;
			
			RowCount = length.Val / 128;
			
			if( length.Val % 128 )
				RowCount++;
				
			nRow = 0;
			
			while(nRow < RowCount)
			{
				int RowAddr;
				
				RowAddr = nRow*128;
				
				if( ((gBuffer[1]+(RowAddr*4)) & 0xFFF) == 0 )
					NVMErasePage( (void*)(gBuffer[1]+(RowAddr*4)) );
				
				if( Command.Val != WT_FLASH_FF )	//only program non FF blocks
				{
					// Write 128 words 
					NVMWriteRow((DWORD*)(gBuffer[1]+(RowAddr*4)), (DWORD*)&gBuffer[2+RowAddr]);
				}	
				
				nRow++;
			}	

			responseBytes = 2; //set length of reply
 			break;
 		}		

		//-=-=-=-=-=-=-=-=-=-=-=-=-=-=
		case ER_FLASH:						
		{
			void* pFlash = (void*)FLASH_PROG_BASE;
			int temp;
			
			for( temp = 0; temp < (BMXPFMSZ/FLASH_PAGE_SIZE); temp++ )
				NVMErasePage( pFlash + (temp*FLASH_PAGE_SIZE) );
		}	

		responseBytes = 2; //set length of reply
		break;
	
		//-=-=-=-=-=-=-=-=-=-=-=-=-=-=
		case VERIFY_OK: //Write checksum and program size to the pfm so next time we power up we can launch this user app
		{
			//gBuffer[1] contains the prog size in bytes.  must be on DWORD boundry
			int temp;
			DWORD* fptr;
			DWORD csum = 0;
			
			if( !((*(DWORD*)BL_CSUM_ADDR) == 0xFFFFFFFF && 
				(*(DWORD*)BL_PROGSZ_ADDR) == 0xFFFFFFFF) )
			{
				//send the error response
				pBfr[0] = ERR_FLASH_NOT_ERASE & 0xFF;
				pBfr[1] = (ERR_FLASH_NOT_ERASE >> 8) & 0xFF;
				responseBytes = 2; //set length of reply
				break;
			}	
			
			fptr = (DWORD*)FLASH_PROG_BASE;

			NVMWriteWord((void*)(BL_PROGSZ_ADDR), gBuffer[1]); //write flash prog size in bytes


			for( temp = 0; temp < gBuffer[1]/4; temp++ )
				csum += fptr[temp];
				
			csum = ~csum + 1;


			NVMWriteWord((void*)(BL_CSUM_ADDR), csum); //write checksum


			if( !((*(DWORD*)BL_CSUM_ADDR) == csum && 
				(*(DWORD*)BL_PROGSZ_ADDR) == gBuffer[1]) )
			{
				//send the error response
				pBfr[0] = ERR_FLASH_PROG & 0xFF;
				pBfr[1] = (ERR_FLASH_PROG >> 8) & 0xFF;
				responseBytes = 2; //set length of reply
				break;
			}	

			responseBytes = 2; //set length of reply
			break;
		}	

		//-=-=-=-=-=-=-=-=-=-=-=-=-=-=
		default:
			break;
	}
	
	if( responseBytes )
		PutResponse( responseBytes );
}


/********************************************************************
* Function: 	void PutResponse()
*
* Precondition: UART Setup, data in buffer
*
* Input: 		None.
*
* Output:		None.
*
* Side Effects:	None.
*
* Overview: 	Transmits responseBytes bytes of data from buffer 
				with UART as a response to received command.
*
* Note:		 	None.
********************************************************************/
void PutResponse(WORD responseLen)
{
	WORD i;
	BYTE data;
	BYTE checksum;
	BYTE *ptr;

	ptr = (BYTE *)gBuffer;

	PutChar(STX);			//Put 2 STX characters
	PutChar(STX);
	PutChar(STX);
	PutChar(STX);
	PutChar(STX);
	PutChar(STX);

	//Output buffer as response packet
	checksum = 0;
	for(i = 0; i < responseLen; i++)
	{	
		data = ptr[i];	//get data from response buffer
		checksum += data;	//accumulate checksum

		//if control character, stuff DLE
		if(data == STX || data == ETX || data == DLE)
			PutChar(DLE);

		PutChar(data);  	//send data
	}

	checksum = ~checksum + 1;	//keep track of checksum

	//if control character, stuff DLE
	if(checksum == STX || checksum == ETX || checksum == DLE){
		PutChar(DLE);
	}

	PutChar(checksum);		//put checksum
	PutChar(ETX);			//put End of text

	while( BusyUART2() );	//wait for transmit to finish
	
}//end PutResponse()




/********************************************************************
* Function: 	void PutChar(BYTE Char)
*
* Precondition: UART Setup
*
* Input: 		Char - Character to transmit
*
* Output: 		None
*
* Side Effects:	Puts character into destination pointed to by ptrChar.
*
* Overview: 	Transmits a character on UART2. 
*	 			Waits for an empty spot in TXREG FIFO.
*
* Note:		 	None
********************************************************************/
void PutChar(BYTE txChar)
{
	while( BusyUART2() );
	putcUART2(txChar);
}


