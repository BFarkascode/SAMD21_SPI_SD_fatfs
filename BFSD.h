/*
 * Created: 20/03/2024 09:46:27
 * Author: BalazsFarkas
 * Project: SAMD21_SPI_SD_fatfs
 * Processor: SAMD21G18A
 * File: BFSD.h
 * Header version: 1.0
 */ 


#ifndef BFSD_H_
#define BFSD_H_


#include "sam.h"
#include "stdint.h"
#include <C:\Users\BalazsFarkas(Lumiwor\Documents\Arduino\SD_card_custom_diskio_custom_integrated_fatfs\BFSPI.h>

//card type
#define SD_TYPE_NMC				0
#define SD_TYPE_V1				1
#define SD_TYPE_V2				2
#define SD_TYPE_V2HC			3

//Card commands
/*
Each command is 6 bytes long, where we have a start bit (0), a Tx bit (1), the CMD itself (6 bits), the arg (32 bits), the crc (7 bits) and the end bit (1)
As such, a CMD0 reset command would be 0x40, 0x0, 0x0, 0x0, 0x0, 0x95 where 0x40is the CMD, the four 0x0 are the arg, and 0x95 is the crc with the end bit.
The command is just the command index in hex (CMD55 will be 0x77 since it is 01110111 where 0 is the start bit, 1 is tx bit and 110111 is the command index - 55 in decimal)


*/
//Note: we assume we have an SDHC card

static uint8_t CMD0[6]	  = {0x40, 0x0, 0x0, 0x0, 0x0, 0x95};						//put card to idle
static uint8_t CMD8[6]	  = {0x48, 0x0, 0x0, 0x1, 0xaa, 0x87};					//send interface conditions
																				//VHS value is important - bit 16 must be HIGH to indicate a range of 2.7-3.6V. The "check pattern" byte can be anything (picked 0xaa from youtube video)
																				//R1 should be 0x1 or we have an error       
static uint8_t CMD9[6]	  = {0x49, 0x0, 0x0, 0x0, 0x00, 0xff};					//send interface conditions                                                                         
static uint8_t CMD10[6]	  = {0x4A, 0x0, 0x0, 0x0, 0x0, 0xff};						//get CID values
static uint8_t CMD12[6]	  = {0x4C, 0x0, 0x0, 0x0, 0x0, 0xff};						//STOP Tx command
static uint8_t CMD13[6]	  = {0x4D, 0x0, 0x0, 0x0, 0x0, 0xff};						//get CID values
static uint8_t CMD38[6]	  = {0x66, 0x0, 0x0, 0x0, 0x0, 0xff};						//erase - works with CMD32 and CMD33
static uint8_t ACMD41[6]	= {0x69, 0x40, 0x0, 0x0, 0x0, 0x77};	        //we call ACMD41 and then define that we want to have HCS as 1 (bit 30)	
static uint8_t CMD55[6]	  = {0x77, 0x0, 0x0, 0x0, 0x0, 0x65};						//selects application specific commands - crc is "don't care" after CMD8
static uint8_t CMD58[6]	  = {0x7a, 0x0, 0x0, 0x0, 0x0, 0xff};						//check card ccs value - it is bit 30 in the R3 response

//Function prototypes
uint8_t SDCard_init(void);
uint8_t SDCard_detect(uint8_t gpio_port, uint8_t gpio_number);
void SDCard_enable(uint8_t gpio_port, uint8_t gpio_number);
void SDCard_disable(uint8_t gpio_port, uint8_t gpio_number);
void SDCard_get_ID(void);											//id comes as a block read of 16 bytes and a 16-bit crc
void SDCard_SPI_speed_change(void);
uint8_t SDCard_read_single_block(uint32_t read_addr, uint8_t* read_buf_ptr);
uint8_t SDCard_write_single_block(uint32_t write_addr, uint8_t* write_buf_ptr);
uint8_t SDCard_read_multi_block(uint32_t read_sector_addr, uint8_t* read_buf_ptr, uint16_t Rx_sector_cnt);
uint8_t SDCard_write_multi_block(uint32_t write_sector_addr, uint8_t* write_buf_ptr, uint16_t Tx_sector_cnt);
void SD_wait_for_bus_idle(void);
void SDCard_get_ID(void);
void SDCard_get_status(void);

#endif /*BFSD*/