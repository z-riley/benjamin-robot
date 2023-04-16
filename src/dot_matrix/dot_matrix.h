/**
 * @file dot_matrix.h
 * @brief Header file for MAX7219 dot display with 8*8 LED matrix
 */

#ifndef MAX7219_H
#define MAX7219_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

/**
 * @brief Initialise SPI devices.
 *
 * Checks if SPI master and CS pins are ready.
 */
void spi_init(void);

/**
 * @brief Initialise MAX7219 dot matrix display.
 *
 * Set up configuration registers.
 */
void dot_matrix_init(void);

/**
 * @brief Write to MAX7219 dot matrix display.
 *
 * Binary representation of will displayed on digit register.
 *
 * @param add Register address.
 * @param data Data to write to address.
 */
void dot_matrix_write(uint8_t addr, uint8_t data);

#endif /* MAX7219_H */
