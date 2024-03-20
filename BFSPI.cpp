/*
 * Created: 19/03/2024 12:02:27
 * Author: BalazsFarkas
 * Project: SAMD21_SPI_SD_fatfs
 * Processor: SAMD21G18A
 * File: BFSPI.cpp
 * Program version: 1.0
 */ 


#include <BFSPI.h>


//1)SERCOM SPI init at 4 MHz
void SERCOM4_SPI_Master_4MHZ_init(void) {

	//SPI init
	//1)Configure IO ports
	//Hardware SPI pins are on PB10, PB11 and PA12 on the Adalogger. It is SERCOM4.
	//SD card on Adalogger gas CS on D4. We don't use PAD1 of the SERCOM4

	PORT->Group[1].PMUX[10>>1].bit.PMUXE = PORT_PMUX_PMUXE_D_Val;		//choose SERCOM4 on pin, PAD2
	PORT->Group[1].PMUX[11>>1].bit.PMUXO = PORT_PMUX_PMUXO_D_Val;		//choose SERCOM4 on pin, PAD3
																		//Note: this pin is a PMUXO, not a PMUXE
	PORT->Group[0].PMUX[12>>1].bit.PMUXE = PORT_PMUX_PMUXE_D_Val;		//choose SERCOM4 on pin, PAD0
	PORT->Group[1].PINCFG[10].bit.PMUXEN = 1;							//enable mux on pin
	PORT->Group[1].PINCFG[11].bit.PMUXEN = 1;							//enable mux on pin
	PORT->Group[0].PINCFG[12].bit.PMUXEN = 1;							//enable mux on pin
  PORT->Group[0].OUT.reg |= (1<<12);       							//we put input definion to allow pullup on PA12
	PORT->Group[0].PINCFG[12].bit.PULLEN = 1;							//add pull-up on MISO


	//2)Clock sources
	//We are using DFLL48M on GCLK0
	//We have OSC8M without pre-scale

	PM->APBCMASK.bit.SERCOM4_ = 0x1;								//enable SERCOM4 APB

	//3)GCLK for SERCOM4
	//we assume SPI is clocked from GCLK0/DFLL48M

	GCLK->CLKCTRL.bit.CLKEN = 1,									//clock channel enabled
	GCLK->CLKCTRL.bit.GEN = (0u),									//choose generator, will be generator 0, so GCLK0
	GCLK->CLKCTRL.bit.ID = 0x18;									//choose channel: GCLK_SERCOM4_CORE

	while(GCLK->STATUS.bit.SYNCBUSY);								//we wait until synch is done
	
	//4)Set SERCOM4 up
	
	SERCOM4->SPI.CTRLA.bit.SWRST = 0x1;								//disable and reset SERCOM SPI
	while(SERCOM4->SPI.SYNCBUSY.bit.SWRST);							//synchronize reset pin

	SERCOM4->SPI.CTRLA.bit.MODE = 0x3;								//SPI is MASTER
	SERCOM4->SPI.CTRLA.bit.DOPO = 0x1;								//PAD[2] is DO, PAD[3] SCK, PAD[1] is host SS and client SS (if MSSEN = 1, otherwise any plain GPIO will do)
																	//Note: we aren't using SERCOM4 PAD[1] since it is not available on the Adalogger. PA8 is the CS for the SD card instead.
	SERCOM4->SPI.CTRLA.bit.DIPO = 0x0;								//PAD[0] is DI
	SERCOM4->SPI.CTRLA.bit.FORM = 0x0;								//simple SPI frame
	SERCOM4->SPI.CTRLA.bit.CPHA = 0x0;								//Rising edge samples, falling edge changes
	SERCOM4->SPI.CTRLA.bit.CPOL = 0x0;								//SCK is LOW when idle	
																	//SPI0 type is used
	SERCOM4->SPI.CTRLA.bit.DORD = 0x0;								//MSB first

	SERCOM4->SPI.CTRLB.bit.CHSIZE = 0x0;							//8 bit word size
	SERCOM4->SPI.CTRLB.bit.MSSEN = 0x0;								//no hardware control on SS
	SERCOM4->SPI.CTRLB.bit.RXEN = 0x1;								//receiver enabled
	while(SERCOM4->SPI.SYNCBUSY.bit.CTRLB);							//synchronize CTRLB register

	SERCOM4->SPI.BAUD.reg = 0x5;									//baud rate value extracted from Arduino definition
																	//Calculatiuons is ((fclk0)/(2 * fSPI)) - 1
																	//for 4 MHz, the baud will be (48MHz/(2 * 4MHz)) - 1 = 5
	SERCOM4->SPI.INTENSET.reg = 0x0;								//no interrupts are used
	
	SERCOM4->SPI.CTRLA.bit.ENABLE = 0x1;							//enable SERCOM SPI
	while(SERCOM4->SPI.SYNCBUSY.bit.ENABLE);						//synchronize enable pin

}


//2)SPI SD write
void SERCOM4_SPI_Master_SD_write(uint8_t *tx_buffer_ptr, uint16_t len) {

//1)We reset the flags

	uint32_t flag_reset = SERCOM4->SPI.DATA.reg;							//reset the RXC flag by reading the DATA register
	while(!(SERCOM4->SPI.INTFLAG.bit.DRE));									//until the DRE flag is not set - DRE set means the Tx buffer is ready to receive new data.


//2)We load the message into the SPI module

	while (len)
	{
		SERCOM4->SPI.DATA.bit.DATA = (volatile uint8_t) *tx_buffer_ptr++;						
		while(!(SERCOM4->SPI.INTFLAG.bit.DRE));
		flag_reset = SERCOM4->SPI.DATA.reg;
		len--;
	}

	while(!(SERCOM4->SPI.INTFLAG.bit.TXC));									//we wait until the last package has been sent

}


//3)SPI SD read
void SERCOM4_SPI_Master_SD_read(uint8_t *rx_buffer_ptr, uint16_t len) {

//1)We reset the flags

	uint32_t flag_reset = SERCOM4->SPI.DATA.reg;							//reset the RXC flag by reading the DATA register
	while(!(SERCOM4->SPI.INTFLAG.bit.DRE));									  //until the DRE flag is not set - DRE set means the Tx buffer is ready to receive new data.

//2)We load the message into the SPI module

	while (len)
	{	
		SERCOM4->SPI.DATA.bit.DATA = 0xFF;									//load dummy data
		while(!(SERCOM4->SPI.INTFLAG.bit.RXC));								//we wait until we have something in the Rx buffer
		*rx_buffer_ptr = SERCOM4->SPI.DATA.reg;
    rx_buffer_ptr++;
		len--;
	}

	//we don't need to wait here. If len is 0, all the expected bytes have been received
	
}


//4)SERCOM SPI init at 400 kHz
void SERCOM4_SPI_Master_400KHZ_init(void) {

	/*

	SPI clocked from GCLK0 with DFLL48M as source.
	SPI baud rate is 400 kHz.

	*/

	//SPI init
	//1)Configure IO ports

	PORT->Group[1].PMUX[10>>1].bit.PMUXE = PORT_PMUX_PMUXE_D_Val;		//choose SERCOM4 on pin, PAD2
	PORT->Group[1].PMUX[11>>1].bit.PMUXO = PORT_PMUX_PMUXO_D_Val;		//choose SERCOM4 on pin, PAD3
																		//Note: this pin is a PMUXO, not a PMUXE
	PORT->Group[0].PMUX[12>>1].bit.PMUXE = PORT_PMUX_PMUXE_D_Val;		//choose SERCOM4 on pin, PAD0
	PORT->Group[1].PINCFG[10].bit.PMUXEN = 1;							//enable mux on pin
	PORT->Group[1].PINCFG[11].bit.PMUXEN = 1;							//enable mux on pin
	PORT->Group[0].PINCFG[12].bit.PMUXEN = 1;							//enable mux on pin
  PORT->Group[0].OUT.reg |= (1<<12);       							//we put input definion to allow pullup on PA12
	PORT->Group[0].PINCFG[12].bit.PULLEN = 1;							//add pull-up on MISO

	//2)Clock sources
	//We are using DFLL48M on GCLK0
	//We have OSC8M without pre-scale

	PM->APBCMASK.bit.SERCOM4_ = 0x1;								//enable SERCOM4 APB

	//3)GCLK for SERCOM4
	//we assume SPI is clocked from GCLK0/DFLL48M

	GCLK->CLKCTRL.bit.CLKEN = 1,									//clock channel enabled
	GCLK->CLKCTRL.bit.GEN = (0u),									//choose generator, will be generator 0, so GCLK0
	GCLK->CLKCTRL.bit.ID = 0x18;									//choose channel: GCLK_SERCOM4_CORE

	while(GCLK->STATUS.bit.SYNCBUSY);								//we wait until synch is done
	
	//4)Set SERCOM4 up
	
	SERCOM4->SPI.CTRLA.bit.SWRST = 0x1;								//disable and reset SERCOM SPI
	while(SERCOM4->SPI.SYNCBUSY.bit.SWRST);							//synchronize reset pin

	SERCOM4->SPI.CTRLA.bit.MODE = 0x3;								//SPI is MASTER
	SERCOM4->SPI.CTRLA.bit.DOPO = 0x1;								//PAD[2] is DO, PAD[3] SCK, PAD[1] is host SS and client SS (if MSSEN = 1, otherwise any plain GPIO will do)
																	//Note: we aren't using SERCOM4 PAD[1] since it is not available on the Adalogger. PA8 is the CS for the SD card instead.
	SERCOM4->SPI.CTRLA.bit.DIPO = 0x0;								//PAD[0] is DI
	SERCOM4->SPI.CTRLA.bit.FORM = 0x0;								//simple SPI frame
	SERCOM4->SPI.CTRLA.bit.CPHA = 0x0;								//Rising edge samples, falling edge changes
	SERCOM4->SPI.CTRLA.bit.CPOL = 0x0;								//SCK is LOW when idle	
																	//SPI0 type is used
	SERCOM4->SPI.CTRLA.bit.DORD = 0x0;								//MSB first

	SERCOM4->SPI.CTRLB.bit.CHSIZE = 0x0;							//8 bit word size
	SERCOM4->SPI.CTRLB.bit.MSSEN = 0x0;								//no hardware control on SS
	SERCOM4->SPI.CTRLB.bit.RXEN = 0x1;								//receiver enabled
	while(SERCOM4->SPI.SYNCBUSY.bit.CTRLB);							//synchronize CTRLB register

	SERCOM4->SPI.BAUD.reg = 0x3B;									//baud rate value extracted from Arduino definition
																	//for 400 kHz, the baud will be (48MHz/(2 * 0.4MHz)) - 1 = 59
	SERCOM4->SPI.INTENSET.reg = 0x0;								//no interrupts are used
	
	SERCOM4->SPI.CTRLA.bit.ENABLE = 0x1;							//enable SERCOM SPI
	while(SERCOM4->SPI.SYNCBUSY.bit.ENABLE);						//synchronize enable pin

}
