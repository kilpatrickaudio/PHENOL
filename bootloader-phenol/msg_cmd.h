
//Constant Defines *****************************************************************
//Protocol Commands
#define RD_FLASH 	0x01
#define WT_FLASH	0x02
#define ER_FLASH	0x03
#define	RD_EEDATA	0x04
#define WT_EEDATA	0x05
#define RD_CONFIG	0x06
#define WT_CONFIG	0x07
#define VERIFY_OK	0x08
#define RD_VER 		0x09


#define WT_FLASH_FF	0x22	//Write Command containing all FF in the packet


//Error replys
#define ERR_FLASH_NOT_ERASE		0x45  //Flash is not erased
#define ERR_FLASH_PROG			0x47  //Programming csum/prog size failed

//Communications Control bytes
#define STX 0x55
#define ETX 0x04
#define DLE 0x05

#define MAX_PACKET_SIZE		1000	//Max data payload

