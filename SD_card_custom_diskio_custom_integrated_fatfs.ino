/*
The code below is to merge the custom SD card driver with the diskio functions.
Currently all diskio functions work.

*/

#include <Arduino.h>
#include <stdlib.h>
#include <C:\Users\BalazsFarkas(Lumiwor\Documents\Arduino\SD_card_custom_diskio_custom_integrated_fatfs\ff.h>
#include <C:\Users\BalazsFarkas(Lumiwor\Documents\Arduino\SD_card_custom_diskio_custom_integrated_fatfs\diskio.h>


FATFS fs;             //file system
FIL fil;              //file
FILINFO filinfo;
FRESULT fresult;
char buffer[1024];

UINT br, bw;

FATFS *pfs;
DWORD fre_clust;
uint32_t total, free_space;

int bufsize (char *buf)
{

  int i= 0;
  while(*buf++ != '\0') i++;
  return i;

}

void bufclear (void)                       //wipe buffer
{

  for (int i = 0; i < 1024; i++){

    buffer[i] = '\0';

  }

}


void setup() {
  // put your setup code here, to run once:

  // start serial port at 9600 bps:
  Serial.begin(9600);

  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB
  }

//------With FATFS-----///


  //--Mount--//
  fresult = f_mount(&fs, "", 0);                    //mount card
  if (fresult != FR_OK) Serial.println("Card mounting failed...");
  else ("Card mounted...");

  //--Capacity check--//
  f_getfree("", &fre_clust, &pfs);

  total = (uint32_t) ((pfs->n_fatent - 2) * pfs->csize * 0.5);
  Serial.print("SD card total size is: ");
  Serial.println(total);

  free_space = (uint32_t) (fre_clust * pfs->csize * 0.5);
  Serial.print("SD card free space is: ");
  Serial.println(free_space);

  //--Create file--//
  f_open(&fil, "file4.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
  strcpy(buffer, "This goes into the fourth file \n");
  f_write(&fil, buffer, bufsize(buffer), &bw);
  f_close(&fil);
  Serial.print("File is created...");
  bufclear();


//------With FATFS-----///

//------No FATFS-----///
#ifdef no_fatfs

  uint8_t stat;

  Serial.println(" ");
  stat =  disk_initialize (0x0);                   //diskio standard init function

  Serial.print("Disk init code: ");
  Serial.print(stat); 

  Serial.println(" ");

  stat = disk_status (0x0);                        //diskio standard status function
  Serial.print("Disk status code: ");

  Serial.print(stat); 

  Serial.println(" ");

  SDCard_get_ID();
#endif

//------No FATFS-----///

}

void loop() {
  // put your main code here, to run repeatedly:


#ifdef no_fatfs
  //-------Write to SD card----------//

  uint8_t write_buf[1024];
  uint8_t feedback;

  for(uint16_t i = 0; i < 256; i++) {

    write_buf[i] = i;
    write_buf[256+i] = 0;
    write_buf[512+i] = 0;
    write_buf[768+i] = i;

  }

  feedback = disk_write (0x0, &write_buf[0], 0x8000, 0x2);
  Serial.print("Write feedback: ");
  Serial.println(feedback);           //diskio standard write command

  //-------Write to SD card----------//

  //-------Read from SD card----------//

  uint8_t read_buf[1024];

  feedback = disk_read (0x0, &read_buf[0], 0x8000, 0x2);
  Serial.print("Read feedback: ");
  Serial.println(feedback);            //diskio standard read command

  Serial.println(" ");
  Serial.print("Data readout: ");
  for(uint16_t i = 0; i < 1024; i++) {

    Serial.print(read_buf[i], HEX);

  }

  Serial.println(" ");
#endif

  char buffer2[1024];

  for (int i = 0; i < 1024; i++){

    buffer2[i] = '0';

  }

  //We request the stats of the file and fill up the FILINFO struct
  f_stat ("file3.txt", &filinfo);					/* Get file status */
  Serial.print("File size: ");  
  Serial.println(filinfo.fsize);

  //We open the file and fill up the FIL struct
  f_open(&fil, "file3.txt", FA_READ);
  fresult = f_read(&fil, buffer2, filinfo.fsize, &br);
  f_close(&fil);

  for(int i = 0; i < filinfo.fsize; i++) {

    Serial.print(buffer2[i]);

  }

  bufclear();

  delay(2000);
  Serial.println(" ");

  //-------Read from SD card----------//

}
