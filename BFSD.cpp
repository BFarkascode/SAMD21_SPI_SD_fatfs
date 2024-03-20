/*
 * Created: 20/03/2024 09:46:27
 * Author: BalazsFarkas
 * Project: SAMD21_SPI_SD_fatfs
 * Processor: SAMD21G18A
 * File: BFSD.cpp
 * Program version: 1.0
 */ 


#include <C:\Users\BalazsFarkas(Lumiwor\Documents\Arduino\SD_card_custom_diskio_custom_integrated_fatfs\BFSD.h>


//1)SERCOM SPI init
uint8_t SDCard_init(void) {  //fatfs demands "DRSTATUS" as an output. DRSTATUS if a BYTE that is "0" for success, "1" for no init and "2" for no disk. Reset value is 0x1.

  /*

    Function initializes SD card.
    Gives back "1" if init was successfull. Gives back "0" in case of failure. (Needed as "stat" values for fatfs library)

	*/
  uint8_t dummy[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  uint8_t response_buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };  //this buffer stores the responses of each CMD
                                                         //Note: we always overwrite the last response with the next one
                                                         //Note: the very first byte is always going to be a dummy 0xFF for a response. This is NOT going to be the case for data readout though!
                                                         //0)Check if we have a card in there
  //if the CS is pulled HIGH, we have a card inserted
  //Note: on the Adalogger, the pin is pulled HIGH externally so this card detect technique does not work

  if (SDCard_detect(0, 8) != 1) {  //if we have a detect answer different than "1"

    Serial.println(" ");
    Serial.print("No card error...");
    Serial.println(" ");
    return 2;  //we return "2" and this indicate to fatfs that something has gone fucky (no card)
  }

  //1)SPI init at 400 kHz (see section 4.4)
  SERCOM4_SPI_Master_400KHZ_init();  //scope shows that the SPI is well calibrated at 400 kHZ, sending through the intended sequence of bits
  Serial.println(" ");

  //2)Wait for 74 cycles + 1 ms (see section 6.4)
  SDCard_disable(0, 8);                        //put CS HIGH and power up the card properly
  delay(1);                                    //wait for 1 ms to stabilize power
  SERCOM4_SPI_Master_SD_write(&dummy[0], 10);  //we send 10 bytes of dummy for at least 74 clock cycles

  //2)Set SPI mode
  SDCard_enable(0, 8);                              //pull CS LOW
  SERCOM4_SPI_Master_SD_write(&CMD0[0], 6);         //we send the idle command and put the system to card to SPI mode
                                                    //Note: card will stay in SPI mode until a full power cycle occurs
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 2);  //CMD0 response is R1	- 1 byte
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);        //after the last SD card transaction, a dummy of 8 clocks must be sent to the card. This applies to:
                                                    //1)After a command with no response
                                                    //2)After a response of a command
                                                    //3)After the end bit of the last read block
                                                    //4)After the CRC in a write block

  Serial.print("CMD0  R1: ");
  Serial.print("0x");
  Serial.print(response_buf[1], HEX);
  Serial.print(",");

  response_buf[1] = 0xFF;  //we reset the response buffer's R1 section
  Serial.println(" ");

  //3)Validate host

  SERCOM4_SPI_Master_SD_write(&CMD8[0], 6);         //CMD8
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 6);  //CMD8 response is R7	- 5 bytes
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);

  Serial.print("CMD8  R7: ");
  for (int i = 1; i < 6; i++) {

    Serial.print("0x");
    Serial.print(response_buf[i], HEX);
    Serial.print(",");
  }
  Serial.println(" ");

  //4)Initialize card
  response_buf[1] = 0xFF;  //we reset the response buffer's R1 section, just in case...

  uint8_t init_timeout = 0;

  while ((response_buf[1] & (1 >> 0)) == (1 >> 0)) {  //"in idle state" bit in the R1 response must be LOW to proceed
                                                      //Note: R1 is stored as the second element of the buffer since the first is a "0xFF" dummy

    if (init_timeout > 10) {  //if after 10 attemps we still haven't managed to get the idle bit sorted

      Serial.println(" ");
      Serial.print("Initialization error...");
      Serial.println(" ");
      return 1;  //we return "1" and this indicate to fatfs that something has gone fucky (init failed)
    }

    SERCOM4_SPI_Master_SD_write(&CMD55[0], 6);        //CMD55		-	switch to app specific command
    SERCOM4_SPI_Master_SD_read(&response_buf[0], 2);  //CMD55 response is R1 - 1 bytes
    SERCOM4_SPI_Master_SD_write(&dummy[0], 1);
    SERCOM4_SPI_Master_SD_write(&ACMD41[0], 6);       //ACMD41	-	we send HCS HIGH to the card
    SERCOM4_SPI_Master_SD_read(&response_buf[0], 2);  //ACMD41 response is R1 - 1 byte
    SERCOM4_SPI_Master_SD_write(&dummy[0], 1);
    init_timeout++;
  }

  SERCOM4_SPI_Master_SD_write(&CMD58[0], 6);        //CMD58 - check CCS value of card
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 6);  //CMD58 response is R3, which is 5 bytes
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);

  Serial.print("CMD58 R3: ");
  for (int i = 1; i < 6; i++) {

    Serial.print("0x");
    Serial.print(response_buf[i], HEX);
    Serial.print(",");
  }

  Serial.print("    ->    Card initialized as...");

  if ((response_buf[2] & (1 << 6)) == (1 << 6)) {  //we check bit 30 in the R3 response. If it is HIGH, we have SDHC, if not, SDSC.

    Serial.println("SDHC ");

    //SDHC card can run up to 50 MHz. Block size is fixed as 512 bytes. Card size is maximum 32 GB.
  } else {

    Serial.println("SDSD ");
    //SDSC card can run up to 25 MHz. Block size is variable using CMD16. Card size is maximum 2 GB.
  }

  uint8_t csd_read_buf[19];                          //block size is 19 bytes
  SERCOM4_SPI_Master_SD_write(&CMD9[0], 6);          //CMD10		-	ask for ID
  SERCOM4_SPI_Master_SD_read(&csd_read_buf[0], 2);   //CMD10 response is R1 in SPI mode	-		we overwrite this response
  SERCOM4_SPI_Master_SD_read(&csd_read_buf[0], 19);  //CSD register readout				-		CID will be read out as simple readout block command: 1 byte start token, 16 bytes of data plus 2 bytes of CRC
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);

  Serial.print("SDCard capacity is: ");

  uint16_t capacity = (((uint16_t)(csd_read_buf[9] << 8)) | csd_read_buf[10]) / 2;
  Serial.print(capacity);
  Serial.println(" MB");

  return 0;
}


//2)Detect SD card
uint8_t SDCard_detect(uint8_t gpio_port, uint8_t gpio_number) {

  /*

		Function to detect SD card by sampling CS line.

	*/

  //  Serial.print("Pin cfg before: ");
  //  Serial.println(PORT->Group[gpio_port].PINCFG[gpio_number].bit.PULLEN);
  //  Serial.println(PORT->Group[gpio_port].DIR.reg &= ~(1<<gpio_number));
  //  Serial.println(PORT->Group[gpio_port].PINCFG[gpio_number].bit.INEN);

  PORT->Group[gpio_port].PINCFG[gpio_number].bit.PULLEN = 0x0;  //remove pullup from CS pin - if any
  PORT->Group[gpio_port].DIR.reg &= ~(1 << gpio_number);        //set CS pin as input
  PORT->Group[gpio_port].PINCFG[gpio_number].bit.INEN = 0x1;    //enable input sampling

  //  Serial.print("Pin cfg after: ");
  //  Serial.println(PORT->Group[gpio_port].PINCFG[gpio_number].bit.PULLEN);
  //  Serial.println(PORT->Group[gpio_port].DIR.reg &= ~(1<<gpio_number));
  //  Serial.println(PORT->Group[gpio_port].PINCFG[gpio_number].bit.INEN);

  //  Serial.print("Pin input value: ");
  //  Serial.println(PORT->Group[gpio_port].IN.reg, BIN);

  if ((PORT->Group[gpio_port].IN.reg & (1 << gpio_number)) == (1 << gpio_number)) {  //we sample the input and if there is a card

    PORT->Group[gpio_port].PINCFG[gpio_number].bit.INEN &= ~0x1;  //disable input sampling
    return 1;

  } else {  //if there is no card

    return 0;
  }
}

//3)Enable SD card CS
void SDCard_enable(uint8_t gpio_port, uint8_t gpio_number) {

  /*

		Function to enable card CS

	*/

  PORT->Group[gpio_port].DIR.reg |= (1 << gpio_number);         //set CS pin as output
  PORT->Group[gpio_port].PINCFG[gpio_number].bit.PULLEN = 0x1;  //pullup on CS pin
  PORT->Group[gpio_port].OUTCLR.reg |= (1 << gpio_number);      //drive CS pin LOW
}


//4)Disable SD card CS
void SDCard_disable(uint8_t gpio_port, uint8_t gpio_number) {

  /*

		Function to disable card CS

	*/

  PORT->Group[gpio_port].DIR.reg |= (1 << gpio_number);         //set CS pin as output
  PORT->Group[gpio_port].PINCFG[gpio_number].bit.PULLEN = 0x1;  //pullup on CS pin
  PORT->Group[gpio_port].OUTSET.reg |= (1 << gpio_number);      //drive CS pin HIGH
}

//5)Read SD card CID register
void SDCard_get_ID(void) {

  /*

		Function to read out card ID

	*/
  uint8_t dummy[1] = { 0xFF };
  uint8_t id_read_buf[19];                          //block size is 19 bytes

  SDCard_enable(0, 8);

  SERCOM4_SPI_Master_SD_write(&CMD10[0], 6);        //CMD10		-	ask for ID
  SERCOM4_SPI_Master_SD_read(&id_read_buf[0], 2);   //CMD10 response is R1 in SPI mode	-		we overwrite this response

  if (id_read_buf[1] != 0x0) {
    Serial.println(" ");
    Serial.print("CMD10 error code: ");
    Serial.print("0x");
    Serial.println(id_read_buf[1], HEX);
    Serial.println(" ");
    return;
  }

  SERCOM4_SPI_Master_SD_read(&id_read_buf[0], 19);  //CID register readout				-		CID will be read out as simple readout block command: 1 byte start token, 16 bytes of data plus 2 bytes of CRC
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);

  Serial.print("SDCard ID: ");
  for (int i = 0; i < 17; i++) {  //we write out the start token, but not the crc

    Serial.print(id_read_buf[i], HEX);
  }
  Serial.println(" ");

  SDCard_disable(0, 8);

}

//6)Change SPI speed
void SDCard_SPI_speed_change(void) {

  /*

		Change the speed of the SPI bus from 400 kHz to something higher.
    Currently SPI speed is set to be 4 MHz.

	*/
  Serial.println(" ");
  SERCOM4_SPI_Master_4MHZ_init();
  Serial.println("SPI speed changed to 4 MHz...");
  Serial.println(" ");
}

//7)Single block read
uint8_t SDCard_read_single_block(uint32_t read_sector_addr, uint8_t* read_buf_ptr) {

  /*

		Function to read out a single block of data from the SDCard.
    If the address is not aligned to the block boundary, the readout will go untill he end of the block and then return invalid data as output in SD mode. In SPI, it is recommended to align the address.
    Note: in SDHC, the block size is 512 bytes. This the address of the readout must align to the boundaries of the memory sections. As a matter of fact, the LSB below bit 9 will be discarded.
    Note: memory block address ranges from 0x0 to 0xF01D

	*/

  //-------Command definition------//

  uint8_t CMD17[6] = { 0x51, (uint8_t)(read_sector_addr >> 24), (uint8_t)(read_sector_addr >> 16), (uint8_t)(read_sector_addr >> 8), (uint8_t)(read_sector_addr), 0xff };
  //Note: we need to convert the block address into byte address for the read/write command


  //-------Command definition------//

  //-------Com tokens------//

  uint8_t dummy[1] = { 0xFF };

  //-------Com tokens------//

  //-------Buffers------//

  uint8_t response_buf[2];  //response buffer to discard the response

  //-------Buffers------//

  //-------Check bus availability------//

  SD_wait_for_bus_idle();

  //-------Check bus availability------//

  //-------Send command, capture command reply------//

  SERCOM4_SPI_Master_SD_write(&CMD17[0], 6);        //CMD17		-	read a block of data
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 2);  //CMD17 response is R1 in SPI mode	-		we read out, then discard the response

  if (response_buf[1] != 0x0) {

    Serial.println(" ");
    Serial.print("CMD17 error code: ");
    Serial.print("0x");
    Serial.println(response_buf[1], HEX);
    Serial.println(" ");
    return 1;
  }

  //-------Send command, capture command reply------//

  //-------Wait for data start token on bus------//

  while (dummy[0] != 0xFE) {  //it seems that the SPI sends over a fuckton of trash before it sends the data package, probably due to the file system shenenigans, or that it signals on the data out line that it is "not busy"
                              //this means that we need to poll for the start bit of the data token to actually fish out the data package

    SERCOM4_SPI_Master_SD_read(&dummy[0], 1);

    if (((dummy[0] & (1 << 0)) == (1 << 0)) & (dummy[0] < 16)) {  //if we have a data error code on the bus

      Serial.println(" ");
      Serial.print("Data error code: ");
      Serial.print("0x");
      Serial.println(dummy[0], HEX);
      Serial.println(" ");
      return 1;

    } else {

      //do nothing
    }
  }

  dummy[0] = { 0xFF };  //we reset our dummy

  //-------Wait for data start token on bus------//

  //-------Capture data package and CRC------//

  SERCOM4_SPI_Master_SD_read(read_buf_ptr, 512);  //512 bytes
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);      //CRC byte 1
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);      //CRC byte 2
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);      //after the last SD card transaction, a dummy of 8 clocks must be sent to the card

  //-------Capture data package and CRC------//
  return 0;
}


//8)Single block write
uint8_t SDCard_write_single_block(uint32_t write_sector_addr, uint8_t* write_buf_ptr) {

  /*

		Function to read out a single block of data from the SDCard.
    If the address is not aligned to the block boundary, the readout will go untill he end of the block and then return invalid data as output in SD mode. In SPI, it is recommended to align the address.
    Note: in SDHC, the block size is 512 bytes. This the address of the readout must align to the boundaries of the memory sections.

	*/

  //-------Command definition------//

  //  uint8_t CMD24[6]	= {0x58, (uint8_t) (write_sector_addr>>15), (uint8_t) (write_sector_addr>>7), (uint8_t) (write_sector_addr<<1), (uint8_t) 0x0, 0xff};						//check card ccs value - it is bit 30 in the R3 response
  uint8_t CMD24[6] = { 0x58, (uint8_t)(write_sector_addr >> 24), (uint8_t)(write_sector_addr >> 16), (uint8_t)(write_sector_addr >> 8), (uint8_t)(write_sector_addr), 0xff };

  //-------Command definition------//

  //-------Com tokens------//

  uint8_t Start_token[1] = { 0xFE };
  uint8_t dummy[1] = { 0xFF };  //this dummy is used to send a blank byte over OR to poll for a particular byte on the bus

  //-------Com tokens------//

  //-------Buffers------//

  uint8_t response_buf[2];  //response buffer to discard the response

  //-------Buffers------//

  //-------Check bus availability------//

  SD_wait_for_bus_idle();

  //-------Check bus availability------//

  //-------Send command, capture command reply------//
  SERCOM4_SPI_Master_SD_write(&CMD24[0], 6);        //CMD24		-	write a block of data
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 2);  //CMD24 response is R1 in SPI mode	-		we read out, then discard the response
  if (response_buf[1] != 0x0) {

    Serial.println(" ");
    Serial.print("CMD24 error code: ");
    Serial.print("0x");
    Serial.println(response_buf[1], HEX);
    Serial.println(" ");
    return 1;
  }

  //-------Send command, capture command reply------//

  //-------Send start token, data package, CRC, capture data response------//

  SERCOM4_SPI_Master_SD_write(&Start_token[0], 1);  //send start token
  SERCOM4_SPI_Master_SD_write(write_buf_ptr, 512);  //512 bytes of data
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);        //CRC byte 1
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);        //CRC byte 2
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 1);  //read the data response
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);        //after the last SD card transaction, a dummy of 8 clocks must be sent to the card

  if ((response_buf[0] & (1 << 3)) == (1 << 3)) {  //if bit 3 is HIGH, we have an error

    Serial.println(" ");
    Serial.print("Data response error code: ");
    Serial.print("0x");
    Serial.println(response_buf[0], HEX);
    Serial.println(" ");
    return 1;
  }

  //-------Send start token, data package, CRC, capture data response------//

  //-------Wait until card is not busy------//

  Serial.println(dummy[0]);
  dummy[0] = 0x0;            //we reset the dummy
  while (dummy[0] == 0x0) {  //Note: after the data block has been transferred, the data response token will be followed by a "busy" signal until the card is properly programmed. The busy signal is the line being pulled LOW

    SERCOM4_SPI_Master_SD_read(&dummy[0], 1);

  }

  Serial.println(dummy[0]);

  //-------Wait until card is not busy------//

  return 0;
}

//9)Multi block read
uint8_t SDCard_read_multi_block(uint32_t read_sector_addr, uint8_t* read_buf_ptr, uint16_t Rx_sector_cnt) {

  /*

		Multi readout

	*/

  //-------Command definition------//

  //  uint8_t CMD18[6]	= {0x52,  (uint8_t) (read_sector_addr>>15), (uint8_t) (read_sector_addr>>7), (uint8_t) (read_sector_addr<<1), (uint8_t) 0x0, 0xff};
  //Note: we need to convert the block address into byte address for the read/write command

  uint8_t CMD18[6] = { 0x52, (uint8_t)(read_sector_addr >> 24), (uint8_t)(read_sector_addr >> 16), (uint8_t)(read_sector_addr >> 8), (uint8_t)(read_sector_addr), 0xff };

  //-------Command definition------//

  //-------Com tokens------//

  uint8_t dummy[1] = { 0xFF };

  //-------Com tokens------//

  //-------Buffers------//

  uint8_t response_buf[2];  //response buffer to discard the response

  //-------Buffers------//

  //-------Check bus availability------//

  SD_wait_for_bus_idle();

  //-------Check bus availability------//

  //-------Send command, capture command reply------//

  SERCOM4_SPI_Master_SD_write(&CMD18[0], 6);        //CMD18		-	read multiple blocks of data
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 2);  //CMD18 response is R1 in SPI mode	-		we read out, then discard the response

  if (response_buf[1] != 0x0) {

    Serial.println(" ");
    Serial.print("CMD18 error code: ");
    Serial.print("0x");
    Serial.println(response_buf[1], HEX);
    Serial.println(" ");
    return 1;
  }

  //-------Send command, capture command reply------//


  //-------Capture data packages and CRC------//

  while (Rx_sector_cnt) {

    //-------Wait for data start token on bus------//
    //Note: each data block comes with a start token

    //  Serial.print("Sector count: ");
    //  Serial.println(Rx_sector_cnt);

    while (dummy[0] != 0xFE) {

      SERCOM4_SPI_Master_SD_read(&dummy[0], 1);

      if (((dummy[0] & (1 << 0)) == (1 << 0)) & (dummy[0] < 16)) {  //if we have a data error code on the bus (see datasheet for token costruction)
                                                                    //if there is data error, there won't be any data sent over, just the error token

        Serial.println(" ");
        Serial.print("Data error code: ");
        Serial.print("0x");
        Serial.println(dummy[0], HEX);
        Serial.println(" ");
        return 1;

      } else {

        //do nothing
      }
    }

    dummy[0] = 0xFF;  //we reset our dummy

    //-------Wait for data start token on bus------//

    SERCOM4_SPI_Master_SD_read(read_buf_ptr, 512);  //512 bytes
    SERCOM4_SPI_Master_SD_write(&dummy[0], 1);      //CRC byte 1
    SERCOM4_SPI_Master_SD_write(&dummy[0], 1);      //CRC byte 2
    read_buf_ptr += 512;                            //we step the pointer
    Rx_sector_cnt--;                                //we decrease sector count
  }

  //-------Capture data packages and CRC------//

  //-------Send STOP command, capture command reply------//

  SERCOM4_SPI_Master_SD_write(&CMD12[0], 6);        //CMD12		-	stop Tx
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 2);  //CMD12 response is R1 in SPI mode	-		we read out, then discard the response

  if (response_buf[1] != 0x0) {

    Serial.println(" ");
    Serial.print("CMD12 error code: ");
    Serial.print("0x");
    Serial.println(response_buf[1], HEX);
    Serial.println(" ");
    return 1;
  }

  //-------Send STOP command, capture command reply------//

  //-------Wait until card is not busy------//

  dummy[0] = 0x0;            //we reset the dummy
  while (dummy[0] == 0x0) {  //Note: after the data block has been transferred, the data response token will be followed by a "busy" signal until the card is properly programmed. The busy signal is the line being pulled LOW

    SERCOM4_SPI_Master_SD_read(&dummy[0], 1);
  }

  //-------Wait until card is not busy------//

  return 0;

}


//10)Multi block write
uint8_t SDCard_write_multi_block(uint32_t write_sector_addr, uint8_t* write_buf_ptr, uint16_t Tx_sector_cnt) {

  /*

		Multi write

	*/

  //-------Command definition------//

  uint8_t CMD25[6] = { 0x59, (uint8_t)(write_sector_addr >> 24), (uint8_t)(write_sector_addr >> 16), (uint8_t)(write_sector_addr >> 8), (uint8_t)(write_sector_addr), 0xff };  //check card ccs value - it is bit 30 in the R3 response

  //-------Command definition------//

  //-------Com tokens------//

  uint8_t Start_token_multi_write[1] = { 0xFC };
  uint8_t Stop_token_multi_write[1] = { 0xFD };
  uint8_t dummy[1] = { 0xFF };  //this dummy is used to send a blank byte over OR to poll for a particular byte on the bus
  uint8_t data_resp[1] = { 0x0 };  //this dummy is used to send a blank byte over OR to poll for a particular byte on the bus

  //-------Com tokens------//

  //-------Buffers------//

  uint8_t response_buf[2];  //response buffer to discard the response

  //-------Buffers------//

  //-------Check bus availability------//

  SD_wait_for_bus_idle();

  //-------Check bus availability------//

  //-------Send command, capture command reply------//

  SERCOM4_SPI_Master_SD_write(&CMD25[0], 6);        //CMD25		-	write a block of data
  SERCOM4_SPI_Master_SD_read(&response_buf[0], 2);  //CMD25 response is R1 in SPI mode	-		we read out, then discard the response

  if (response_buf[1] != 0x0) {

    Serial.println(" ");
    Serial.print("CMD25 error code: ");
    Serial.print("0x");
    Serial.println(response_buf[1], HEX);
    Serial.println(" ");
    return 1;
  }

  //-------Send command, capture command reply------//

  //-------Send data packages and CRC------//

  while (Tx_sector_cnt) {

    SERCOM4_SPI_Master_SD_write(&Start_token_multi_write[0], 1);  //send start token
    SERCOM4_SPI_Master_SD_write(write_buf_ptr, 512);              //512 bytes
    SERCOM4_SPI_Master_SD_write(&dummy[0], 1);                     //CRC byte 1
    SERCOM4_SPI_Master_SD_write(&dummy[0], 1);                    //CRC byte 2
    SERCOM4_SPI_Master_SD_read(&data_resp[0], 1);                 //each data block has a data response token which is 0xE5 for OK, 0xEB for CRC error and 0xED to write error

    if (data_resp[0] != 0xE5) {

      Serial.println(" ");
      Serial.print("Tx error code: ");
      Serial.print("0x");
      Serial.println(data_resp[0], HEX);
      Serial.println(" ");
      SERCOM4_SPI_Master_SD_write(&CMD12[0], 6);                  //CMD12		-	stop Tx
      return 1;

    }

    write_buf_ptr += 512;                                        //we step the pointer
    Tx_sector_cnt--;                                             //we decrease sector count

    SD_wait_for_bus_idle();

  }

  //-------Capture data packages and CRC------//

  //-------Send STOP token, capture command reply------//

  SERCOM4_SPI_Master_SD_write(&Stop_token_multi_write[0], 1);  //send stop token

  //-------Send STOP command, capture command reply------//

  //-------Wait until card is not busy------//

  dummy[0] = 0x0;            //we reset the dummy
  while (dummy[0] == 0x0) {  //Note: after the data block has been transferred, the data response token will be followed by a "busy" signal until the card is properly programmed. The busy signal is the line being pulled LOW

    SERCOM4_SPI_Master_SD_read(&dummy[0], 1);
  }

  //-------Wait until card is not busy------//
  return 0;
}


//10)SD wait bus idle
void SD_wait_for_bus_idle(void) {

  uint8_t dummy[1] = { 0x0 };
  while (dummy[0] != 0xFF) {

    SERCOM4_SPI_Master_SD_read(&dummy[0], 1);
  };
}



//11)Send card status
void SDCard_get_status(void) {

  uint8_t response_buf[3];													          //response buffer to discard the response
  uint8_t dummy[1] = {0xFF};

  SERCOM4_SPI_Master_SD_write(&CMD13[0], 6);									//CMD24		-	write a block of data

  SERCOM4_SPI_Master_SD_write(&CMD13[0], 6);									//CMD13 - get status
	SERCOM4_SPI_Master_SD_read(&response_buf[0], 3);						//CMD13 response is R2, which is 2 bytes
  SERCOM4_SPI_Master_SD_write(&dummy[0], 1);

  Serial.print("CMD13 R2: ");
  for(int i = 1; i < 3; i++) {

    Serial.print("0x");
    Serial.print(response_buf[i], HEX);
    Serial.print(",");

  }

}
