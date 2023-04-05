/**
 * @file main.c
 * @brief Benjamin The Robot
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/spi.h>
#include "remote_service/remote.h"
#include "libs/ultrasonic_hc-sr04.h"
#include "helpers.h"

// Logging
#define LOG_MODULE_NAME Benjamin
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

// GPIO
#define RUN_STATUS_LED DK_LED1
#define CONN_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

// Pin definitions
#define ULTRASONIC_TRIG_PIN 25
#define ULTRASONIC_ECHO_PIN 26

// SPI
#define MY_SPI_MASTER DT_NODELABEL(my_spi_master)

// Function prototypes
static struct bt_conn *current_conn;
void on_connected(struct bt_conn *conn, uint8_t error);
void on_disconnected(struct bt_conn *conn, uint8_t reason);
void on_data_received(struct bt_conn *conn, const uint8_t *const data, uint16_t len);
void update_motors(uint8_t dir_ascii);
void reset_motors(struct k_timer *timer);
void dot_matrix_init();
void dot_matrix_write(uint8_t addr, uint8_t data);

// Timers
K_TIMER_DEFINE(motor_timeout, reset_motors, NULL);  

// Initialise devices
static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct pwm_dt_spec motors_l = PWM_DT_SPEC_GET(DT_NODELABEL(motors_l));
static const struct pwm_dt_spec motors_r = PWM_DT_SPEC_GET(DT_NODELABEL(motors_r));
static const struct pwm_dt_spec motor_f = PWM_DT_SPEC_GET(DT_NODELABEL(motor_f));
static const uint32_t MIN_PULSE_F = DT_PROP(DT_NODELABEL(motor_f), min_pulse);
static const uint32_t MAX_PULSE_F = DT_PROP(DT_NODELABEL(motor_f), max_pulse);
const struct device *spi_dev;

// Global variable declarations
enum directions{forwards, right, backwards, left}dir;
static bool b_dot_matrix_ok = 0;


struct spi_cs_control dot_matrix_cs = {
	.gpio = SPI_CS_GPIOS_DT_SPEC_GET(DT_NODELABEL(dot_matrix)),
	.delay = 0,
};


static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPHA |  SPI_MODE_CPOL, 
	.frequency = 4000000,
	.slave = 0,
	.cs = &dot_matrix_cs,
};


struct bt_conn_cb bluetooth_callbacks = {
	.connected 		= on_connected,
	.disconnected 	= on_disconnected,
};


struct bt_remote_service_cb remote_callbacks = {
    .data_received = on_data_received,
}; 


/* Callbacks */
void on_connected(struct bt_conn *conn, uint8_t error)
{
	if(error) {
		LOG_ERR("connection err: %d", error);
		return;
	}
	LOG_INF("Connected.");
	current_conn = bt_conn_ref(conn);
	dk_set_led_on(CONN_STATUS_LED);
} /* on_connected */


void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason: %d)", reason);
	dk_set_led_off(CONN_STATUS_LED);
	if(current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
} /* on_disconnected */


void on_data_received(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
    uint8_t temp_str[len+1];
    memcpy(temp_str, data, len);
    temp_str[len] = 0x00;

    LOG_INF("Received data on conn %p. Len: %d", (void *)conn, len);;
    LOG_INF("Data: %s", temp_str);

    update_motors(temp_str[0]); 

    // Start/reset timer. Call reset_motors if time elapses.
    // Data sent from controller every 50 ms. 70 ms found to be good timeout.
    k_timer_start(&motor_timeout, K_MSEC(70), K_NO_WAIT);
    
} /* on_data_received */


void update_motors(uint8_t dir_ascii)
{
    uint16_t error;
    uint32_t motors_l_pwm_ns = 1500;
    uint32_t motors_r_pwm_ns = 1500;
    const uint32_t ROBOT_SPEED = 400;       // Max 500
    
   
    uint8_t dir = dir_ascii - '0';          // Convert to uint8
    LOG_DBG("ASCII: %u", dir_ascii);
    LOG_DBG("Int: %u", dir);

    /* Motor PWM->Speed Reference:
    Max foward = 2000 us
    Max reverse = 1000 us
    Stop = 1500 us. Dead band [1480-1520] us
    */

    switch(dir)
    {
        case forwards: 
            motors_l_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);

            break;
        case right:
            motors_l_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            break;
        case backwards:
            motors_l_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            break;
        case left:
            motors_l_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            break;
        default:
            LOG_INF("Controller incorrectly configured. Set forwards = 0, right = 1, backwards = 2, left = 3.");
            break;    
    }   

    // Set motor speeds
    error = pwm_set_pulse_dt(&motors_l, motors_l_pwm_ns);
    if (error < 0)
    {
        LOG_ERR("Error %d: failed to set pulse width of left motors", error);
        return;
    }
    error = pwm_set_pulse_dt(&motors_r, motors_r_pwm_ns);
    if (error < 0)
    {
        LOG_ERR("Error %d: failed to set pulse width of right motors", error);
        return;
	}
    LOG_INF("Left motor set to %u us", motors_l_pwm_ns/1000);
    LOG_INF("Right motor set to %u us", motors_r_pwm_ns/1000);
    
    return;
} /* update_motors */


void reset_motors(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    uint16_t error;
    error = pwm_set_pulse_dt(&motors_l, PWM_USEC(1500));
     if (error < 0) {
			LOG_ERR("Error %d: failed to reset left motors", error);
			return;
		}
    error = pwm_set_pulse_dt(&motors_r, PWM_USEC(1500));
    if (error < 0) {
			LOG_ERR("Error %d: failed to reset right motors", error);
			return;
		}
    LOG_INF("Motors turned off (1500 us)");
    return;
} /* reset_motors */


static void config_dk_leds(void)
{
    int16_t error;
    error = dk_leds_init();
    if (error) {
        LOG_ERR("Couldn't init LEDS (error %d)", error);
    }
    return;
} /* config_dk_leds */


static void spi_init(void)
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
    return;    
}


void ultrasonic_thread(void)
{
    uint32_t dist_mm; 
    int16_t motor_err;
    uint32_t motor_f_pwm_ns = PWM_USEC(1500);
    bool b_dir = 0;
    uint32_t scan_position = 0;
    uint32_t prev_scan_position = 0;
    uint8_t dist_display= 0;
    
    for (;;)
    {
        if (1) //if (current_conn) USUAL
        {   
            // Initiliase dot matrix 
            if (!b_dot_matrix_ok)
            {
                dot_matrix_init();
                b_dot_matrix_ok = 1;
            }
            
            // Move proximity sensor to next position
            motor_err = pwm_set_pulse_dt(&motor_f, motor_f_pwm_ns);
            if (motor_err < 0) 
            {
                LOG_ERR("Error %d: failed to set pulse width of front motor", motor_err);
                return;
            }
            LOG_DBG("Front motor set to %u us", motor_f_pwm_ns/1000);

            // Take sensor reading
            dist_mm = sense_distance();
		    
            // Map servo position to 1-8 value 
            scan_position = map(motor_f_pwm_ns, 1000000, 2000000, 1, 8);
            //LOG_INF("PWM: %lu   Scan: %lu", motor_f_pwm_ns/1000, scan_position);              // Motor PWM (1,000,000 to 2,000,000)
            
            if (scan_position != prev_scan_position)
            {
                // Map distance value to respective bit field 
                dist_display = 0xFF << map(dist_mm, 0, 500, 0, 8);
                LOG_INF("Distance: %u mm, Position: %u, Distance bit field: "BYTE_TO_BINARY_PATTERN, dist_mm, scan_position, BYTE_TO_BINARY(dist_display));
                dot_matrix_write(scan_position, dist_display);
            }
            prev_scan_position = scan_position;
   
            // Update servo position values        // TODO: Tidy this up
            if (b_dir == 0)
            {
                if (motor_f_pwm_ns <= MIN_PULSE_F) 
                {
            		motor_f_pwm_ns = MIN_PULSE_F;
                    b_dir = 1;
            	} 
                else
                {
				    motor_f_pwm_ns -= PWM_USEC(10);                 // 30 is a nice speed
		 	    }
            } 
            else
            {
            	motor_f_pwm_ns = motor_f_pwm_ns + PWM_USEC(10);     // 30 is a nice speed
            	if (motor_f_pwm_ns >= MAX_PULSE_F) 
                {
            		motor_f_pwm_ns = MAX_PULSE_F;
                    b_dir = 0;
            	}
            }

        }
        k_sleep(K_MSEC(40));    // 50 ms is a proven value
    }
}


void dot_matrix_init()
{
    dot_matrix_write(0x0C, 0x01);   // Shutdown register
    dot_matrix_write(0x0A, 0x00);   // Intensity register
    dot_matrix_write(0x0B, 0x07);   // Scan limit register
    dot_matrix_write(0x09, 0x00);   // Decode-mode register
}


void dot_matrix_write(uint8_t addr, uint8_t data)
{
	static uint8_t tx_buffer[2];
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


void main(void)
{
    int16_t error;
    int16_t blink_status = 0;
	LOG_INF("Hello World! %s\n", CONFIG_BOARD);

    config_dk_leds();
    error = ultrasonic_init(gpio_dev, ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN);
	if (!error)
	{
		LOG_DBG("Ultrasonic sensor initialisation failed");
	}

    spi_init();

    error = bluetooth_init(&bluetooth_callbacks, &remote_callbacks);
    if (error) 
    {
        LOG_INF("Couldn't initialize Bluetooth. error %d", error);
    }

    if (!device_is_ready(motors_l.dev))
    {
		LOG_ERR("Error: PWM device %s is not ready\n", motors_l.dev->name);
		return;
	}
	if (!device_is_ready(motors_r.dev))
    {
		LOG_ERR("Error: PWM device %s is not ready\n", motors_r.dev->name);
		return;
	}

    LOG_INF("Running...");
    for (;;) {

        dk_set_led(RUN_STATUS_LED, (blink_status++)%2);
        k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));

    }
} /* main */


K_THREAD_DEFINE(ultrasonic_thread_id, 1024, ultrasonic_thread, NULL, NULL, NULL, 4, 0, 0);
