/*
 * Created: 19/03/2024 16:11:04
 * Author: BalazsFarkas
 * Project: SAMD21_SPI_SD_fatfs
 * Processor: SAMD21G18A
 * File: diskio.cpp
 * Program version: 1.0
 */ 


/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/


// -- //

  //Modified  on 04.03.2024 by BF for SAMD21 Bootloader project
  //This file is the main fatfs controller of the SD card. As such, the funcitons are adjusted in order to properly interface with the custom SD driver I have written for the card.
  //As such, the function's inputs and outputs must not be changed.

// -- //

#include "diskio.h" /* Declarations of disk functions */
#include <C:\Users\BalazsFarkas(Lumiwor\Documents\Arduino\SD_card_custom_diskio_custom_integrated_fatfs\diskio.h>


/*-----------------------------------------------------------------------*/
/* Custom variables                                                      */
/*-----------------------------------------------------------------------*/

  uint16_t Tx_timeout_cnt;  //this will be decreased by an IRQ
  uint16_t Rx_timeout_cnt;  //this will be decreased by an IRQ

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

/*

Just a dummy since we only have one SD card.

*/

DSTATUS disk_status(
  BYTE pdrv /* Physical drive nmuber to identify the drive */
) {
  return 0x0;  //disk status is a dummy in our design which should not change the DRSTATUS value
               //only mount_volume in the fatfs function (see ff.cpp) that calls both init and status. It is called BEFORE init so it won't reset the init return answer.
}


/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

/*

This is to initialize the card.
Fatfs doesn't care, how it is done. It just wants the card running and a valid status for it:
"0" - all is well
"1" - init error
"2" - no card found

Write protect doesn't work in our card so that answer is discarded.

*/

DSTATUS disk_initialize(
  BYTE pdrv /* Physical drive nmuber to identify the drive */
) {

  uint8_t init_result = SDCard_init();

  SDCard_SPI_speed_change();

  return init_result;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

/*

This is to send commands.
Currently only 3 commands are recognised: SYNCH, SIZE GET and SECTOR GET. Other commands are not being called by fatfs here - either not used (TRIM) or specifically made for other types of cards (MMC).

*/

DRESULT disk_ioctl(
  BYTE pdrv, /* Physical drive nmuber (0..) */  //we ignore this
  BYTE cmd,                                     /* Control code */
  void *buff                                    /* Buffer to send/receive control data */
) {

  //----
  //Note: with pdrv, we do nothing
  //Note: I removed the power management cmd section. It doesn't seem to be used.
  //Note: the status of the card is only updated within the init function and the status function. As such, after an unsuccesfull init, we can just break the execution completely.
  //The trick is that if we have an extern value storing the state of the card locally too, we can poll that value and potentially skip execution. We can give back error messages upon continous re-engagement for the card
  //----

  DRESULT response = RES_ERROR;  //this is our response byte, resets to R/W error
  uint8_t dummy[1] = {0xFF};

  SDCard_enable(0, 8);

  switch (cmd) {

    //----SECTOR COUNT command----//

    case GET_SECTOR_COUNT: { //if we get the sector count command
                            //we need to send CMD9 to the card and load the reply into the buff pointer
                            //we extract the csize from the csd and then project the value to the buff pointer

      uint8_t csd_read_buf[19];                          //block size is 19 bytes
      SERCOM4_SPI_Master_SD_write(&CMD9[0], 6);          //CMD10		-	ask for ID
      SERCOM4_SPI_Master_SD_read(&csd_read_buf[0], 2);   //CMD10 response is R1 in SPI mode	-		we overwrite this response
      SERCOM4_SPI_Master_SD_read(&csd_read_buf[0], 19);  //CSD register readout				-		CID will be read out as simple readout block command: 1 byte start token, 16 bytes of data plus 2 bytes of CRC
      SERCOM4_SPI_Master_SD_write(&dummy[0], 1);

      uint16_t capacity = (((uint16_t)(csd_read_buf[9] << 8)) | csd_read_buf[10]);  //c_size is the number of blocks, not byte
                                                                                    //Note: this is upside down due to the different way how we read out
      *(uint32_t *)buff = (uint32_t)capacity << 10;                                 //we get the byte number

      response = RES_OK;

    } break;

    //----SECTOR SIZE command----//

    case GET_SECTOR_SIZE:  {//if we get the sector size command, the sector size is hard wired to 512
      *(WORD *)buff = 512;
      response = RES_OK;  //we should just leave the switch state here to go and deselect the SDcard after (so no return statements here)
    } break;


    //----SYNC command----//

    case CTRL_SYNC: { //this is a timeout step: we do a while loop until we get 0xFF as an SPI Rx byte AND we haven't run out of a timer_cnt
                     //the timer is started at a value then decreased by a timer IRQ

      uint8_t Rx_byte_sync[1] = { 0 };  //Rx array for sync

      do {  //do this loop while the incoming SPI Rx byte is NOT 0xFF AND our timer still hasn't reached 0

        SERCOM4_SPI_Master_SD_read(&Rx_byte_sync[0], 1);  //we read out a byte

      } while (Rx_byte_sync[0] != 0xFF);

    } break;

    //----Default----//

    default:

      response = RES_PARERR;  //default value is invalid parameter

  }

  SDCard_disable(0, 8);

  uint8_t Rx_byte_dummy[1] = { 0 };                  //Rx array for dummy
  SERCOM4_SPI_Master_SD_read(&Rx_byte_dummy[0], 1);  //we read out a byte

  return response;  //we return "all is well" or RES_OK in fatfs speak

}


/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(
  BYTE pdrv,    /* Physical drive nmuber to identify the drive */
  BYTE *buff,   /* Data buffer to store read data/uint8_t */
  LBA_t sector, /* Start sector in LBA/uint32_t */
  UINT count    /* Number of sectors to read/uint16_t */
) {

  DRESULT response = RES_ERROR;

  uint8_t result;

	/* convert to byte address */

	SDCard_enable(0, 8);

	if (count == 1)  {                                     //we read out one sector

		/* READ_SINGLE_BLOCK */
    result = SDCard_read_single_block(sector, buff);
	
  } else {                                                 //we read out multiple sectors

    result = SDCard_read_multi_block(sector, buff, count);

  }

	/* Idle */
	SDCard_disable(0, 8);

  uint8_t Rx_byte_dummy[1] = { 0 };                  //Rx array for dummy
  SERCOM4_SPI_Master_SD_read(&Rx_byte_dummy[0], 1);  //we read out a byte

  if(result == 0) response = RES_OK;

  return response;  //we return "all is well" or RES_OK in fatfs speak
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/



DRESULT disk_write(
  BYTE pdrv,        /* Physical drive nmuber to identify the drive */
  const BYTE *buff, /* Data to be written */
  LBA_t sector,     /* Start sector in LBA */
  UINT count        /* Number of sectors to write */
) {

  DRESULT response = RES_ERROR;

  uint8_t result;
	/* convert to byte address */

  //---Convert fatfs write ptr to local ptr---//


  //---Convert fatfs write ptr to local ptr---//

	SDCard_enable(0, 8);

	if (count == 1)  {                                                //we read out one sector

		/* READ_SINGLE_BLOCK */
    result = SDCard_write_single_block(sector, (uint8_t*)buff);     //We convert fatfs write ptr to local ptr
	
  } else {                                                          //we read out multiple sectors
    
    result = SDCard_write_multi_block(sector, (uint8_t*)buff, count);

  }

	/* Idle */
	SDCard_disable(0, 8);

  uint8_t Rx_byte_dummy[1] = { 0 };                  //Rx array for dummy
  SERCOM4_SPI_Master_SD_read(&Rx_byte_dummy[0], 1);  //we read out a byte

  if(result == 0) response = RES_OK;

  return response;  //we return "all is well" or RES_OK in fatfs speak
}


