/**
 * @file ultrasonic_hc-sr04.c
 * @brief Source file for HC-SR04 proximity sensor module library
 */

#include "ultrasonic_hc-sr04.h"

// Filewide scope variables to be used by any function.
// These exist so sense_distance can be called without passing input arguments.
static const struct device *port_fw;
static gpio_pin_t pin_trig_fw;
static gpio_pin_t pin_echo_fw;

// Variables accessable by ISR 
static volatile bool latch;
static volatile uint32_t start_time;
static volatile uint32_t stop_time;	
static volatile uint32_t cycles_spent;
static volatile uint32_t us_spent;
static volatile uint32_t distance;

// Private function prototypes
static void echo_cb(void);
static struct gpio_callback echo_cb_data;


// Ultrasonic echo ISR. Is called on rising and falling edge, hence the use of the latch variable.
static void echo_cb(void)
{
	if (!latch)
	{
		latch = 1;
		// Capture time stamp on rising edge
		start_time = k_cycle_get_32();
	} 
	else
	{
		// Capture time stamp on falling edge
		stop_time = k_cycle_get_32();
		latch = 0;
		// Compute proximity based on the echo pin pulse length (assumes no counter rollover)
		cycles_spent = stop_time - start_time;
		us_spent = k_cyc_to_us_floor32(cycles_spent);
	}
} /* echo_cb */

int32_t ultrasonic_init(const struct device *port, gpio_pin_t pin_trig, gpio_pin_t pin_echo)
{
    // Set filewide variables for sense_distance() to use
    port_fw = port;
	pin_trig_fw = pin_trig;
    pin_echo_fw = pin_echo;
	
    // Configure trigger and echo pins
    int err;
	err = gpio_pin_configure(port, pin_trig, GPIO_OUTPUT);
	err = gpio_pin_configure(port, pin_echo, GPIO_INPUT);
	err = gpio_pin_interrupt_configure(port, pin_echo, GPIO_INT_EDGE_BOTH);
	if (err)
	{
		return 666;
	}

	// Initialise callback and add callback functions
	gpio_init_callback(&echo_cb_data, (gpio_callback_handler_t)echo_cb, BIT(pin_echo));
	err = gpio_add_callback(port, &echo_cb_data);
	if (err)
	{
		return 667;
	}

	return 0;
} /* ultrasonic_init */


uint32_t sense_distance(void) 
{
	// Send 10 us trigger pulse, after which the echo_cb() ISR will run
	gpio_pin_set_raw(port_fw, pin_trig_fw, 0);
	k_sleep(K_USEC(2));
	gpio_pin_set_raw(port_fw, pin_trig_fw, 1);
	k_sleep(K_USEC(10));
	gpio_pin_set_raw(port_fw, pin_trig_fw, 0);
	k_sleep(K_USEC(2));

	// If device gives a bad reading, return last good value. 15000 chosen empirically
	if (us_spent > 15000)
	{
		return distance;
	}

	// Calculate distance in mm. 0.344 is the speed of sound in millimeters per microsecond
	distance = 0.344*us_spent/2;
	return distance;
} /* measure_distance */
