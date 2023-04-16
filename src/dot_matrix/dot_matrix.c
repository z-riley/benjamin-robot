/**
 * @file dot_matrix.c
 * @brief Source file for MAX7219 dot display with 8*8 LED matrix
 */

#include "dot_matrix.h"

// Logging
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

// SPI
#define MY_SPI_MASTER DT_NODELABEL(my_spi_master)
static const struct device *spi_dev;

const struct spi_cs_control dot_matrix_cs = {
	.gpio = SPI_CS_GPIOS_DT_SPEC_GET(DT_NODELABEL(dot_matrix)),
	.delay = 0,
};

const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPHA |  SPI_MODE_CPOL, 
	.frequency = 4000000,
	.slave = 0,
	.cs = &dot_matrix_cs,
};

void spi_init(void)
{
	
	spi_dev = DEVICE_DT_GET(MY_SPI_MASTER);
	if(!device_is_ready(spi_dev)) 
    {
		LOG_ERR("SPI master device not ready!\n");
	}
	if(!device_is_ready(dot_matrix_cs.gpio.port))
    {
		LOG_ERR("SPI master chip select device not ready!\n");
	}
} /* spi_init */

void dot_matrix_init(void)
{
    dot_matrix_write(0x0C, 0x01);   // Shutdown register
    dot_matrix_write(0x0A, 0x00);   // Intensity register
    dot_matrix_write(0x0B, 0x07);   // Scan limit register
    dot_matrix_write(0x09, 0x00);   // Decode-mode register
} /* dot_matrix_init */

void dot_matrix_write(uint8_t addr, uint8_t data)
{
	uint8_t tx_buffer[2];
	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

    tx_buffer[0] = addr;
    tx_buffer[1] = data;
    
    LOG_DBG("SPI TX: 0x%.2x, 0x%.2x\n", tx_buffer[0], tx_buffer[1]);
	
	// Start transaction
    int16_t error;
	error = spi_write(spi_dev, &spi_cfg, &tx);
    if(error != 0)
    {
		LOG_ERR("SPI write error: %i\n", error);
		return;
	}
	return;
} /* dot_matrix_write */