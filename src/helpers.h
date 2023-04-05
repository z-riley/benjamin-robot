/**
 * @file helpers.h
 * @brief Header file for helpers file
 */

#ifndef HELPERS_H
#define HELPERS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

/* Display byte in binary. */
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 


/**
 * @brief Re-maps a number from one range to another.
 *
 * Constrains returned values within the specified range.
 *
 * @param x Input value.
 * @param in_min Lower bound of input value.
 * @param in_max Upper bound of the input value.
 * @param out_min Lower bound of the value's target range.
 * @param out_max Upper bound of the value's target range.
 * @returns The mapped value. Type uint32_t.
 */
uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);

#endif /* HELPERS_H */
