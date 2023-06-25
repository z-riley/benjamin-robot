/**
 * @file main.c
 * @brief Benjamin The Robot
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include "remote_service/remote.h"
#include "libs/ultrasonic_hc-sr04.h"
#include "helpers.h"

// Logging
#define LOG_MODULE_NAME Benjamin_main
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

// GPIO
#define RUN_STATUS_LED DK_LED1
#define CONN_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

// Pin definitions
#define ULTRASONIC_TRIG_PIN 25
#define ULTRASONIC_ECHO_PIN 26

// OLED Display
#define MY_DISP_HOR_RES 128
#define MY_DISP_VER_RES 64

// Motors
#define MOTOR_TIMEOUT_MS 120

// Function prototypes
static struct bt_conn *current_conn;
static void on_connected(struct bt_conn *conn, uint8_t error);
static void on_disconnected(struct bt_conn *conn, uint8_t reason);
static void on_data_received(struct bt_conn *conn, const uint8_t *const data, uint16_t len);
static void update_motors(uint8_t dir_ascii);
static void reset_motors(struct k_timer *timer);
static void config_dk_leds(void);
static void i2c_init(void);
static void oled_init(void);

// Timers
K_TIMER_DEFINE(motor_timeout, reset_motors, NULL);  

// Initialise devices
static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct pwm_dt_spec motors_l = PWM_DT_SPEC_GET(DT_NODELABEL(motors_l));
static const struct pwm_dt_spec motors_r = PWM_DT_SPEC_GET(DT_NODELABEL(motors_r));
static const struct pwm_dt_spec motor_f = PWM_DT_SPEC_GET(DT_NODELABEL(motor_f));
static const uint32_t MIN_PULSE_F = DT_PROP(DT_NODELABEL(motor_f), min_pulse);
static const uint32_t MAX_PULSE_F = DT_PROP(DT_NODELABEL(motor_f), max_pulse);
static const struct device *oled_dev;

// Variable declarations
typedef enum
{
    NONE_E = 0,
    NORTH_E = 1,
    NORTHEAST_E = 2,
    EAST_E = 3,
    SOUTHEAST_E = 4,
    SOUTH_E = 5,
    SOUTHWEST_E = 6,
    WEST_E = 7,
    NORTHWEST_E = 8
} robot_dir_t;

struct bt_conn_cb bluetooth_callbacks = {
	.connected 		= on_connected,
	.disconnected 	= on_disconnected,
};

struct bt_remote_service_cb remote_callbacks = {
    .data_received = on_data_received,
}; 

/* Callbacks */
static void on_connected(struct bt_conn *conn, uint8_t error)
{
	if(error) {
		LOG_ERR("connection err: %d", error);
		return;
	}
	LOG_INF("Connected.");
	current_conn = bt_conn_ref(conn);
	dk_set_led_on(CONN_STATUS_LED);
} /* on_connected */

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason: %d)", reason);
	dk_set_led_off(CONN_STATUS_LED);
	if(current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
} /* on_disconnected */

static void on_data_received(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
    uint8_t temp_str[len+1];
    memcpy(temp_str, data, len);
    temp_str[len] = 0x00;

    LOG_DBG("Received data on conn %p. Len: %d", (void *)conn, len);;
    LOG_DBG("Data: %s", temp_str);

    update_motors(temp_str[0]); 

    // Start/reset timer. Call reset_motors if time elapses.
    k_timer_start(&motor_timeout, K_MSEC(MOTOR_TIMEOUT_MS), K_NO_WAIT);
    
} /* on_data_received */

static void update_motors(uint8_t dir_ascii)
{
    uint16_t error;
    robot_dir_t dir;
    uint32_t motors_l_pwm_ns = 1500;
    uint32_t motors_r_pwm_ns = 1500;
    const uint32_t ROBOT_SPEED = 350;       // Max 500
    
   
    dir = (uint8_t)(dir_ascii - '0');      // Black magic convert to numeric
    LOG_DBG("ASCII: %u", dir_ascii);
    LOG_INF("Int: %u", dir);

    /* Motor PWM->Speed Reference:
        Max foward = 2000 us
        Max reverse = 1000 us
        Stop = 1500 us. Dead band [1480-1520] us */

    switch(dir)
    {
        case NONE_E:
            motors_l_pwm_ns = PWM_USEC(1500);
            motors_r_pwm_ns = PWM_USEC(1500);
            break;
        case NORTH_E: 
            motors_l_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            break;
        case NORTHEAST_E:
            motors_l_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED/2);
            break;
        case EAST_E:
            motors_l_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            break;
        case SOUTHEAST_E:
            motors_l_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED/2);
            break;
        case SOUTH_E:
            motors_l_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            break;
        case SOUTHWEST_E:
            motors_l_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED/2);
            motors_r_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            break;
        case WEST_E:
            motors_l_pwm_ns = PWM_USEC(1500 - ROBOT_SPEED);
            motors_r_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            break;
        case NORTHWEST_E:
            motors_l_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED/2);
            motors_r_pwm_ns = PWM_USEC(1500 + ROBOT_SPEED);
            break;
        default:
            LOG_INF("Controller incorrectly configured. See robot_dir_t definition.");
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
    
} /* update_motors */

static void reset_motors(struct k_timer *timer)
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

} /* reset_motors */


static void config_dk_leds(void)
{
    int16_t error;
    error = dk_leds_init();
    if (error) {
        LOG_ERR("Couldn't init LEDS (error %d)", error);
    }

} /* config_dk_leds */

static void i2c_init(void)
{
    oled_dev = DEVICE_DT_GET(DT_NODELABEL(ssd1306));
    if(!device_is_ready(oled_dev)) 
    {
		LOG_ERR("SSD1306 display device not ready!\n");
	}
} /* i2c_init */

static void oled_init(void)
{
    // Create draw buffer
	static lv_disp_draw_buf_t draw_buf;
	static lv_color_t buf1[MY_DISP_HOR_RES * MY_DISP_VER_RES];
	lv_disp_draw_buf_init(&draw_buf, buf1, NULL, MY_DISP_VER_RES * MY_DISP_VER_RES);

    // Create text label
    lv_obj_t *hello_label;
    hello_label = lv_label_create(lv_scr_act());
	lv_label_set_text(hello_label, "Battery Voltage:");
	lv_obj_align(hello_label, LV_ALIGN_CENTER, 0, 24);

    // Create voltage label
    lv_obj_t *voltage_label;
    voltage_label = lv_label_create(lv_scr_act());
    char fake_voltage_status[] = "4148 mV";
    lv_label_set_text(voltage_label, fake_voltage_status);
    lv_obj_align(voltage_label, "LV_ALIGN_BOTTOM_MID", 32, 0);

    display_blanking_off(oled_dev);
    lv_task_handler();

} /* oled_init */

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
        if (NULL != current_conn)
        {               
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
            LOG_DBG("PWM: %lu   Scan: %lu", motor_f_pwm_ns/1000, scan_position);              // Motor PWM (1,000,000 to 2,000,000)
            
            if (scan_position != prev_scan_position)
            {
                // Placeholder. Write to screen
                //LOG_INF("Distance: %u mm, Position: %u, Distance bit field: "BYTE_TO_BINARY_PATTERN, dist_mm, scan_position, BYTE_TO_BINARY(dist_display));
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

    i2c_init();
    oled_init();

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

// Threads
K_THREAD_DEFINE(ultrasonic_thread_id, 1024, ultrasonic_thread, NULL, NULL, NULL, 4, 0, 0);
