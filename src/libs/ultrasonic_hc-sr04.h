/**
 * @file ultrasonic_hc-sr04.h
 * @brief Header file for HC-SR04 proximity sensor module library
 */

#ifndef HCSR04_H
#define HCSR04_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/**
 * @brief Initialise HC-SR04 sensor.
 *
 * Intialise trigger and echo pins. Configure an interrupt on the rising and
 * falling edge on the trigger pin.
 *
 * @param port Pointer to the device structure for the driver instance.
 * @param pin_trig Pin number of the trigger pin.
 * @param pin_echo Pin number of the echo pin.
 * @retval 0 if successful.
 */
int32_t ultrasonic_init(const struct device *port, gpio_pin_t pin_trig, gpio_pin_t pin_echo);


/**
 * @brief Measure proximity between HC-SR04 and nearest object in sensing range. 
 *
 * Configure trigger and echo pins. Set up an interrupt on the rising and
 * falling edge on the trigger pin.
 *
 * @param port Pointer to the device structure for the driver instance.
 * @param pin_trig Pin number connected to the echo
 * @return Proximity, in millimetres, of nearest object in sensing range.
 */
uint32_t sense_distance(void);

#endif /* HCSR04_H */
