# SAMD21_SPI_SD_fatfs
SD card driver on a SAMD21 using bare metal programming. Interfacing done with latest fatfs.bblablala

## General description
As mentioned in my project for the SAMD21 bootmaster controller for my STM32 bootloader, I haven’t been to happy with the Arduino-based solution I have presented there beforehand. Yes, it was functional, but at the same time, the code was sketchy at best, with some strange behaviour (“black magic“) that I could not get around, no matter how much I tired. Thus, it was only a matter of time before I have put my nose to the grindstone and come up with a way to replace the SD library of Arduino with something home-made. This project is the result of that work.

Of note, I had been using the source [SD card using SPI in STM32 » ControllersTech](https://controllerstech.com/sd-card-using-spi-in-stm32/) heavily while generating my own version of this project. I practically had to reverse engineer the Controllerstech driver file to get an understanding of what is going on. Unfortunately, there was no alternative source to figure out, how to set up the SD driver since the SD documentation wasn’t very good, nor was the Fatfs library website.

## Previous relevant projects
The following projects should be checked:

-	STM32_SPIDriver
-	SAMD21_BootMaster

## To read
It is somewhat recommended to go through the following two documents to have a grasp over what is going on, though I personally had a fair share of difficulties with them:

-	Fatfs library documentation FatFs - Generic FAT Filesystem Module ([elm-chan.org](http://elm-chan.org/fsw/ff/00index_e.html))
-	SD card documentation SD.pdf (https://www.sdcard.org/downloads/pls/)

As I mentioned above, I used chiefly reverse engineering methods on someone else’s code to find answers in the end, since these officials documents were lacking.

## Particularities
We will have 4 different layers within this project:

1) SPI layer where we set up and activate the SPI module within the SAMD21
2) SD card layer where we set up the functions to send commands and data to the memory card via the SPI layer mentioned above
3) The "diskio" layer which is the glue layer between the fatfs library and our SD card driver
4) High level where we call the fatfs functions

These layers should be merged seemlessly to allow a good flow of data, commands and information. Below I will take a closer look into them to show, what particularities are to be considered in each of them.

### Setting up SPI
Initializing the SPI on the SAMD21 is rather similar to how it is done on the STM32 with a few notable caveats:

1) Port configuration is for PMUX is organized into pairs of “even” and “odd” groups. This means that even if we have chosen the right PMUX pin, the even or the odd values must be updated, depending on if the pin number. (A good example for this is "PORT->Group[1].PMUX[10>>1].bit.PMUXE" which works, while "PORT->Group[1].PMUX[10>>1].bit.PMUXO" would not. Why? Because 10 is an even number, so we need to update the PMUXE part of the register, not he PMUXO.)
2) There are multiple registers that need a synch, namedly CTRLB and ENABLE. We can check the synch by checking the designated SYNCBUSY bit in the register.
3) Baud rate is calculated by ((fclk0)/(2 * fSPI)) – 1, where flck0 is the clock source of the SERCOM and fSPI is the desired baud rate in Hz. For 4 MHz with a standard 48 MHz fclk0, the baud will be (48MHz/(2 * 4MHz)) - 1 = 5.
4) SERCOM pads need to be specific inputs/outputs. We can choose, which pad is on which GPIO though throughout the configuration of the SERCOM. The pad assignment is done in the CTRLA register. (GPIO/Sercom is in the SAMD21 datasheet, section 7.)

Apart from these,  the SPI driver is very similar to its STM32 counterpart and thus won’t be discussed any more detail. We are going to keep the SPI driver blocking to simply the code though.

### Putting together the SD library

Admittedly, one of the most difficult things for me had been to make hands-or-tails on how the SD card is being controlled. As it goes, all SD cards come with two distinct control systems between which one can choose upon power up by pulling the card's CS pin HIGH or LOW. For microcontrollers, only the CS pin LOW version will work: this version will emulate the "standard" SD card control (not discussed here) using the SPI bus. The definition I find a but confusing though since, despite using the SPI bus as means of communicating, the SD card driver IS NOT an SPI-style driver: instead of having responses coming in immediatelly as bytes reach the slave, the communication will work along "tokens", "replies" and "commands" - certain predefined sections of data that will take time to be processed.

Tokens will be specific bytes depending on what the card is doing or expected to do. They preceed the data package that is being transferred on the bus. These tokens come on the bus sometime and somewhere, but NOT immiediatelly when they are demanded (think of it as communicating with other microcontroller using a a very busy. The micro will reply whenever it can, but not immediatelly). Tokens can indicate the start or the end of a data package, as well as show that the data sent to the card has not been accepted (i.e. there was a transmission error). There are all together 5 different tokes (see SD card datasheet 7.3.3 Control tokens section).

Replies come in the form of standard answer formats (usually coming in after a data package) which are then have to be captured and disected in order to acquire relevant information. Apart from receiving a full 512 byte data package as it is when we ask for the CSD and CIS values, we can have R1, R2, R3 and R7 as a response, each representing 1/2/3 or 7 bytes.

Controlling of the card is done using commands. These are bytes - codes of the command, where the code is generated by adding a HIGH MSB bit to the value...turning CMD10 or b1010 into byte 0x4A or b1001010 - followed by an argument and a CRC. For commands CMD0 and CMD8, the CRC must be accurate and valid! Afterwards, within SPI control mode, the CRC check will be disabled unless specifically turned back on. The CRC value to be sent with CMD0 is CRC 0x95) and for CMD8, it is CRC 0x87). Failing the CRC will not allow the card to work!

One nasty thing is that initialization of the card must always occur at 400 kHz or less, despite the fact that the SPI might be set for 10 MHz or even faster (SDHC card limit is at 25 MHz, though SPI becomes sketchy past 10 MHz). As it goes, within the driver, we MUST slow down the SPI bus during card initialization. After initialization, we can speed it back up though.

Another annoying thing that I have learnt through trial and error was that the bus isn't "empty" when the master isn't asking for data (thus reinforcing the idea that we aren't following a classic SPI communication profile). Instead, the bus is constantly full of trash, only going to "idle" (dataline 0xFF) after certain scenarios are met. This means that we MUST ALWAYS fish for the tokens on the bus, we can't just start taking data immediately after we have demanded it from the card. We will only capture noise!

Another-another annoying thing is the timing of the card. We need to know exactly, what type of answer we are expecting and then send over enough dummies by the master to properly vacate the SD card. We can't just cut the communication short: if we are to receive an R7 answer, we ought to clock for all 7 bytes. Similarly, it is recommended by the datasheet to send a dummy byte over after certain commands are sent (see 4.4 Clock control section).

Lastly, I need to be specific that we define the driver for only SDHC card types, which are up to 32 GB big and can be run up to 25 MHz. This means that we will always manipulate the memory area in blocks of 512 bytes, never less (as it could be do so with SDSD cards...but again, we ignore those).

To give a quick rundown on how we control the SD card:
1) We need to disable the card by pullling CS HIGH and then send over 10 bytes of dummy data. This is recommended by the SD datasheet.
2) We pull CS LOW and send CMD0. By doing so, we engage SPI control mode.
3) We send CMD8 over to activate SDHC control version (SDSD cards reject CMD8!)
4) We publish CMD55 (to select ACMD type commands) and ACMD41 until we receive in the R1 reply of these commands that the card is ready. From this spot and on, we don't care about the CRC

Once the card is ready, it will stay as such until it is power cycled.

Of note, the argument for all write and read commands (so, CMD17, CMD18, CMD24 and CMD25) is the sector address for SDHC, not the byte address. Thus, if we provide the commands the argument 0x8000, then 0x8001 will be the next value to update/read out. It must be kept in mind that sector addresses range from 0x0 to 0xF01D (0xF01d * 512 is 32 GB) on a 32 GB card.

On the topic of tokens, they are inserted in a rather self-explanatory way (see datasheet).

### Hammer out the diskio compatibility
Fatfs is a very commonly used library that provides a filesystem to an otherwise blank memory section. It is controlled by/called using its own functions (see FatFs module application interface section in the ff.h file) and interfaces towards the driver of the memory element through the diskio.h functions (disk_init/disk_status/disk_read/disk_write and disk_ioctl).

In order to glue any custom driver to fatfs, the custom driver must be working perfectly (!) alongside the diskio layer functions. What I mean is that that they need to demand the same variables as inputs, react to them in a way that the fatfs library can understand it and provide the desired outputs. It could be seen as a connector between two layers where the in/out flow of the diskio layer must match the driver’s in-out flow.

Observing the “disk control functions” in diskio.h, we can see that we have only two types of outputs from the diskio layer, namedly “DRESULT” and “DSTATUS” where both are 1 byte long error messages. The status messages can be 3 different values, while result enum can be 5. It is clearly indicated in the diskio.h, when these replies are to be generated (STA_NODISK status for instance when we have no disk detected). We will need to provide these outputs within our diskio functions.

Input-wise, we expect a byte called “pdrv” for all diskio functions. Now, this input merely selects between different types of memory elements (or multiple of them in case one master is driving multiple memories). Luckily, we can completely ignore anything related to “pdrv” since in our usecase we will have only one type of standard memory and no multiple slaves. As such, pdrv will be constant 0x0.

Other inputs are

- buff, a BYTE pointer to the array where we store the data we wish to send or extract from the memory element. Mind, this is a BYTE pointer, which is an unsigned char (see ff.h). It will behave both as input and output for the functions that are using it (for instance, the ioctl will dump the extracted SD card parameters in this buffer). Of note, for disk_write the input pointer is a constant, thus we can not directly step it: we will need to define a pointer within the function to point to this input pointer.
- LBA_t is the number of the sector and is a DWORD (uint32_t). This is the address we will be writing to/reading from. Mind, despite what the datasheet may suggest, this is the sector address and not the byte address. Similarly, when we go to the SD card driver implementation, this value will go directly into the argument of the SD card command without any modification (say, adding 9 bits of LSB to match sector address to byte address). All in all, it is confusing if one needs to give the byte address or the sector address to the SD card commands…
- count is the number of sectors (!) we wish to read/write
- cmd is the number of the command we send to the SD card

And now, to discuss what the 5 functions of the diskio layer are doing:

1) ”Disk_init” initializes the card. Since it only takes pdrv as input, we can pretty much handle the functions as a int (void) type function where the output will depend on the error message we encounter during the process. Also, we need not to forget to ramp up the speed of the SPI once init is done.
2) “Disk_status” function will be a dud, always giving back “RES_OK” as a response whenever it is being called by the fatfs (mind, we don’t know when and where the fatfs might use these functions, so best to build them anyway).
3) “disk_ioctl” will be the command element of the diskio layer. It takes in only 3 different commands (CTRL_SYNC, GET_SECTOR_COUNT, GET_SECTOR_SIZE) in our case, all other commands won’t be used and thus we don’t need to react to them in the diskio level. Why these three? Because in “diskio.h” it is clearly indicated that the rest we aren’t using due to our pre-selection of a specific card type (no GET_BLOCK_SIZE and no CTRL_TRIM).
GET_SECTOR_COUNT will give back the “c_size” bits that we extract from the CSD register of the SD card. GET_SECTOR_SIZE will give back always 512 as a reply since we are using SDHC. CTRL_SYNC will simply wait until the bus is idle.
4) “disk_read” and “disk_write” will read/write from the card. Of note, these functions are set to deal with multiple read and write scenarios, so our driver also needs to be able to do both.

In the end, I simply kept the framing provided by the diskio layer and then filled it up with custom SD card driver functions.

### Fatfs on highest level
Fatfs uses structs. We need to fill these structs up each time with the parameters related directly to the file we wish to use in order to have the appropriate behaviour/outcome from the fatfs library. Sometimes we have one struct, sometimes we have multiple, each of them having their own call function (f_open for FIL, f_stat for FILINFO). Once all information is extracted and plugged into the right sections, the fatfs library will handle the files for us using its own set of functions.

Mind, in thsi project, we aren’t using multiple sections of the fatfs library. With our basic activities, will use f_open (which calls “mount_volume”), f_close, f_stat, f_read and f_write and nothing else. This is useful to decide ahead of time since when we check the execution of these functions, we will also be able to also see, which diskio functions are being called by them (and with which parameters, if any). To be precise:

- we will call disk_init in f_open by proxy “mount_volume”
- we call disk_status in “mount_volume”
- we call disk_ioctl in “mount_volume” with the command GET_SECTOR_SIZE and some other places using CTRL_SYNC. The cmd that are not called, we will simply ignore during our implementation of the diskio cmd center (see “disk_ioctl” function in the diskio layer).
- disk_read is called in f_open, f_write
- disk_write is called at multiple places

We won’t be extracting RTC data, thus the “get_fattime” function in line 276 of “ff.cpp” timestamp section will need to be either removed or given the same values as for line 274.

### Additional notes
We have a myriad of while loops in the code which may or may not freeze the execution. For a robust implementation – which this one isn’t – we would need to ensure that these loops time out after a while. This timeout could be done by having an external counter count down while the loop is active and then forcing it to finish when it reaches zero, or we could just limit the number of loop executions we allow to occur. The reason why I ignored to do so is because in my current application any failure on the SD card will demand a complete reset of the micro anyway.

The project is primarily to integrate fatfs and then manipulate files on the SDcard. Nevertheless, behind #ifdef-s, I left the "crude" manipulation of the memory also available within the ino file. This crude version will directly write data to memory addresses and ignore any kind of file system that may or may not be on the card already. This can in some cases break the card to the point where it needs to be formatted before it can be used again with fatfs.

