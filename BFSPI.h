/*
 * Created: 19/03/2024 12:02:27
 * Author: BalazsFarkas
 * Project: SAMD21_SPI_SD_fatfs
 * Processor: SAMD21G18A
 * File: BFSPI.h
 * Header version: 1.0
 */ 

#ifndef BFSPI_H_
#define BFSPI_H_


#include "sam.h"
#include <Arduino.h>

//Function prototypes
void SERCOM4_SPI_Master_400KHZ_init(void);
void SERCOM4_SPI_Master_4MHZ_init(void);
void SERCOM4_SPI_Master_SD_write(uint8_t *tx_buffer_ptr, uint16_t len);
void SERCOM4_SPI_Master_SD_read(uint8_t *rx_buffer_ptr, uint16_t len);

#endif /*BFSPI*/
