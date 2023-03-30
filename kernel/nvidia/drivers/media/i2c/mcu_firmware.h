#ifndef _MCU_FIRMWARE_H
#define _MCU_FIRMWARE_H

/*   Local Defines */
#define MAX_BUF_LEN 2048

#define MAX_PAGES 		512
#define TOTAL_PAGES 		1536
#define NUM_ERASE_CYCLES	(TOTAL_PAGES / MAX_PAGES)

#define FLASH_START_ADDRESS 	0x08000000
#define FLASH_SIZE 		192*1024
#define FLASH_READ_LEN		256

#define CR 			13                   /*   Carriage return */
#define LF 			10                   /*   Line feed */

/*MCU Buffer size increased - Fix for loading menu based controls */
#define MCU_BUFFER_SIZE 	1024

/*   TODO: Only necessary commands added */
enum _i2c_cmds
{
	BL_GET_VERSION = 0x01,
	BL_GO = 0x21,
	BL_READ_MEM = 0x11,
	BL_WRITE_MEM = 0x31,
	BL_WRITE_MEM_NS = 0x32,
	BL_ERASE_MEM = 0x44,
	BL_ERASE_MEM_NS = 0x45,
};

enum _i2c_resp
{
	RESP_ACK = 0x79,
	RESP_NACK = 0x1F,
	RESP_BUSY = 0x76,
};

enum 
{
	NUM_LANES_1 = 0x01,
	NUM_LANES_2 = 0x02,
	NUM_LANES_3 = 0x02,
	NUM_LANES_4 = 0x04,
	NUM_LANES_UNKWN = 0xFF,
};

enum _ihex_rectype
{
	/*   Normal data */
	REC_TYPE_DATA = 0x00,
	/*  End of File */
	REC_TYPE_EOF = 0x01,

	/*   Extended Segment Address */
	REC_TYPE_ESA = 0x02,
	/*   Start Segment Address */
	REC_TYPE_SSA = 0x03,

	/*   Extended Linear Address */
	REC_TYPE_ELA = 0x04,
	/*   Start Linear Address */
	REC_TYPE_SLA = 0x05,
};

typedef struct __attribute__ ((packed)) _ihex_rec {
	unsigned char datasize;
	unsigned short int addr;
	unsigned char rectype;
	unsigned char recdata[];
} IHEX_RECORD;

unsigned int g_bload_flashaddr = 0x0000;

uint8_t *fw_version = NULL;

/* MCU communication variables */
unsigned char mc_data[MCU_BUFFER_SIZE];
unsigned char mc_ret_data[MCU_BUFFER_SIZE];

/*   Buffer to Send Bootloader CMDs */
unsigned char g_bload_buf[MAX_BUF_LEN] = { 0 };

unsigned short int g_bload_crc16 = 0x0000;

const char g_mcu_fw_buf[] =
#include "e-CAM130A_CUXVR_mcu_fw.bin"
;
#endif                        //_MCU_FIRMWARE_H