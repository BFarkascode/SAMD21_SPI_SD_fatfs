# SAMD21_SPI_SD_fatfs
SD card driver on a SAMD21 using bare metal programming. Interfacing done with latest fatfs.

## General description
AS mentioned in my project for the SAMD21 bootmaster for the STM32 bootloader, I haven’t been to happy with the Arduino-based solution I have presented there. The code was sketchy at best, with strange behaviour (“black magic“) that I could not get around, no matter how much I tired. Thus, it was only a matter of time before I have decided to put my nose to the grindstone and come up with a way to replace the SD library of Arduino with something home-made. This project is the result of this work.

Of note, I had been using this source (SD card using SPI in STM32 » ControllersTech) heavily while generating my own version of this project. I practically had to reverse engineer its driver file since there was no alternative source to figure out, how to set up the SD driver and the SD documentation isn’t very good.

## Previous relevant projects
The following projects should be checked:

-	STM32_SPIDriver

## To read
It is highly recommended to go through the following two documents to have a grasp over what is going on (albeit I used both of them tangentially only and reverted to reverse engineering someone else’s code to find answers in the end, see above):

-	Fatfs library documentation FatFs - Generic FAT Filesystem Module (elm-chan.org)
-	SD card documentation SD.pdf (mit.edu)

## Particularities
We will have 4 different layers within this project:

1)SPI layer where we set up and activate the SPI module
2)SD card layer where we set up the functions to send commands and data to the memory card via the SPI layer mentioned above.
3)Diskio layer is the glue layer between the fatfs library and our SD card driver.
4)High level is where we call the fatfs functions directly

### Setting up SPI
Initializing the SPI on the SAMD21 is rather similar to how it is done on the STM32 with a few notable exemptions:
1)Port configuration is for PMUX is organized into pairs of “even” and “odd” groups. This means that even if we have chosen the right PMUX pin, then even or the odd values must be updated, depending on if the pin number is even or odd. (A good example for this would be 
PORT->Group[1].PMUX[10>>1].bit.PMUXE
which works, while
PORT->Group[1].PMUX[10>>1].bit.PMUXO
would not.
2)There are multiple registers that needs a synch, namedly CTRLB and ENABLE. We can check this by checking the designated SYNCBUSY bit in the register.
3)Baud rate is calculated by ((fclk0)/(2 * fSPI)) – 1. For 4 MHz, the baud will be (48MHz/(2 * 4MHz)) - 1 = 5.
4)SERCOM pads need to be specific inputs/outputs. We can choose, which pads is on which GPIO though. The pad assignment is done in the CTRLA register. GPIO/Sercom is in the SAMD21 datasheet, section 7.
Apart from these, as I mentioned, the SPI driver is very similar to its STM32 counterpart and thus won’t be discussed any more detail. We are going to keep the SPI driver blocking to simply the code though.

### Putting together the SD library

Slow speed for init, then we can go to 4 MHz or more with the SPI.
The SD card uses tokens for communication.
The SD card runs on commands.
SD card control mode SPI or standard.
SD card runs on tokens.

Byte or sector? It isn’t clear from the official datasheet for SDcards.
We update by the sector where one sector is 512 bytes long. No modification on this possible since we use SDHC.


### Hammer out the diskio compatibility
Fatfs is an existing, very commonly used library that provides a filesystem to an otherwise blank memory section. It is controlled by/called using its own functions (see FatFs module application interface section in the ff.h file) and interfaces towards the driver of the memory element through the diskio.h functions (disk init/status/read/write/ioctl). Here, we will look at the diskio level and see, what happens there.
In order to glue any custom driver to fatfs, the custom driver must be working perfectly (!) alongside the diskio level functions. What I mean is that that they need to demand the same variable inputs (and react to them in a way that the fatfs library can understand it!) and provide the same outputs. It could be though of as a connector between two layers where the in/out flow of the diskio layer must match the driver’s layer.
Observing the “disk control functions” in diskio.h, we can see that we have only two types of outputs from the diskio layer, namedly “DRESULT” and “DSTATUS” with  both are 1 byte long error messages. The status messages can be 3 different values, while result enum has 5. They are clearly indicated in the diskio.h, when these replies are to be generated (STA_NODISK status for instance when we have no disk detected). We will need to provide these outputs within the diskio functions.
Input-wise, we expect a byte called “pdrv” for all diskio functions. Now, this input merely selects between different types of memory elements (or multiple of them in case one master is driving multiple memories). Luckily, we can completely ignore anything related to “pdrv” since in our usecase we will have only one type of standard memory and no multiple slave configs. As such, pdrv will be constant 0x0. Other inputs are
-buff, a BYTE pointer to the array where we store the data we wish to send or extract from the memory element. Mind, this is a BYTE pointer, which is an unsigned char (see ff.h). It will behave both as input and outputs for the functions that are using it (for instance, the ioctl will dump the extracted SD card parameters in this buffer). Also, for disk_write the input is a constant, thus we can not directly step the pointer: we will need to define a pointer within the function to point to this input pointer.
-LBA_t is the number of the sector and is a DWORD (uint32_t). This is the address we will be writing to/reading from. Mind, despite what the datasheet may suggest, this is the sector address and not the byte address. Similarly, when we go to the SD card driver implementation, this value will go directly into the argument of the SD card command without any modification (say, adding 9 bits of LSB to match sector address to byte address). All in all, it is confusing if one needs to give the byte address or the sector address to the SD card commands…but I am telling right out front that it is the unmodified sector address that goes into it.
-count is the number of sectors (!) we wish to read/write
-cmd is the number of the command we send to the SD card (it will be only GET_SECTOR_COUNT, which is cmd “1” as seen in diskio.h)

And now, to discuss what the 5 functions of the diskio layer are doing:
1)”Disk_init” initializes the card. Since it only takes pdrv as input, we can pretty much handle the functions as a int (void) type function where the output will depend on the error message we encounter during the process. Also, we need not to forget to ramp up the speed of the SPI once init is done.
2)“Disk_status” function will be a dud, always giving back “RES_OK” as a response whenever it is being called by the fatfs (mind, we don’t know when and where the fatfs might use these functions, so best to build them anyway).
3)“disk_ioctl” will be the command element of the diskio layer. It takes in only 3 different commands (CTRL_SYNC, GET_SECTOR_COUNT, GET_SECTOR_SIZE) in our case, all other commands won’t be used and thus we don’t need to react to them in the diskio level. Why these three? Because in “diskio.h” it is clearly indicated that the rest we aren’t using due to our selection of card (no GET_BLOCK_SIZE and no CTRL_TRIM). Similarly, we assume SDHC cards and won’t make the code compatible with anything else. (Of note, in a practical sense we don’t seem to use GET_SECTOR_COUNT as a command, nevertheless, I generated all the 3 commands in the layer.)
GET_SECTOR_COUNT will give back the “c_size” bits that we extract from the CSD register of the SD card. GET_SECTOR_SIZE will give back always 512 as a reply since we are using SDHC. CTRL_SYNC will simply wait until the bus is idle.
4)“disk_read” and “disk_write” will read/write from the card. Of note, these functions are set to deal with multiple read and write scenarios, so our driver also needs to be able to do both. They are called often.
In the end, I simply kept the framing provided by the diskio layer and then filled it up with custom SD card driver functions.

### Fatfs on highest level
Fatfs uses structs. We need to fill the structs up each time with the parameters related directly to the file we wish to use in order to have the appropriate behaviour/outcome. Also, sometimes we have one struct, sometimes we have multiple, each of them having their own call function (f_open for FIL, f_stat for FILINFO). Once all information is extracted and plugged into the right sections, the fatfs library will handle the files for you using its own set of functions. Mind, we aren’t using huge sections of the fatfs library. 
With our basic activities, will use f_open (which calls “mount_volume”), f_close, f_stat, f_read and f_write and nothing else. This is useful to decide ahead of time since when we check the execution of these functions, we will be able to also see, which diskio functions are being called by them (and with which parameters, if any). To be precise:
-we will call disk_init in f_open by proxy “mount_volume”
-we call disk_status in “mount_volume”
-we call disk_ioctl in “mount_volume” with the command GET_SECTOR_SIZE and some other places using CTRL_SYNC. The cmd that are not called, we will simply ignore during our implementation of the diskio cmd center (see “disk_ioctl” function in the diskio layer).
-disk_read is called in f_open, f_write
-disk_write is called all the time
We won’t be extracting RTC data, thus the “get_fattime” function in line 276 of “ff.cpp” timestamp section will need to be either removed or given the same values as for line 274. That’s because we don’t have an RTC on our microcontroller.


### Additional notes
We have a myriad of while loops in the code which may or may not freeze the execution. For a robust implementation – which this one isn’t – we would need to ensure that these loops time out after a while. This timeout could be done by having an external counter count down while the loop is active and then forcing it to finish when it reaches zero, or we could just limit the number of loop executions we allow to occur. The reason why I ignored to do so is because in my current application any failure on the SD card will demand a complete reset of the micro since I am not doing anything else anyway.

