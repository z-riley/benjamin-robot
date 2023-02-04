/**
 * @file It's Benjamin!
 * @brief The Robot
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include "remote_service/remote.h"
#include "libs/ultrasonic_hc-sr04.h"

// Logging
#define LOG_MODULE_NAME Benjamin
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

// GPIO
#define RUN_STATUS_LED DK_LED1
#define CONN_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

// Motor Control                TODO: Instead get these values from devicetree
#define MOTOR_PWM_NS_MAX 2000
#define MOTOR_PWM_NS_OFF 1500
#define MOTOR_PWM_NS_MIN 1000

// Pin definitions
#define ULTRASONIC_TRIG_PIN 25
#define ULTRASONIC_ECHO_PIN 26

// Function prototypes
static struct bt_conn *current_conn;
void on_connected(struct bt_conn *conn, uint8_t err);
void on_disconnected(struct bt_conn *conn, uint8_t reason);
void on_data_received(struct bt_conn *conn, const uint8_t *const data, uint16_t len);
void update_motors(uint8_t dir_ascii);
void reset_motors(void);

// Timers
K_TIMER_DEFINE(motor_timeout, reset_motors, NULL);  

// Put thread definitions here?

// Initialise devices
static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct pwm_dt_spec motors_l = PWM_DT_SPEC_GET(DT_NODELABEL(motors_l));
static const struct pwm_dt_spec motors_r = PWM_DT_SPEC_GET(DT_NODELABEL(motors_r));
static const struct pwm_dt_spec motor_f = PWM_DT_SPEC_GET(DT_NODELABEL(motor_f));
static const uint32_t min_pulse_f = DT_PROP(DT_NODELABEL(motor_f), min_pulse);
static const uint32_t max_pulse_f = DT_PROP(DT_NODELABEL(motor_f), max_pulse);

enum directions{forwards, right, backwards, left}dir;

struct bt_conn_cb bluetooth_callbacks = {
	.connected 		= on_connected,
	.disconnected 	= on_disconnected,
};

struct bt_remote_service_cb remote_callbacks = {
    .data_received = on_data_received,
}; 


/* Callbacks */
void on_connected(struct bt_conn *conn, uint8_t err)
{
	if(err) {
		LOG_ERR("connection err: %d", err);
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
    int motor_err;
    uint32_t motors_l_pwm_ns;
    uint32_t motors_r_pwm_ns;
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
    motor_err = pwm_set_pulse_dt(&motors_l, motors_l_pwm_ns);
    if (motor_err < 0)
    {
        LOG_ERR("Error %d: failed to set pulse width of left motors", motor_err);
        return;
    }
    motor_err = pwm_set_pulse_dt(&motors_r, motors_r_pwm_ns);
    if (motor_err < 0) 
    {
        LOG_ERR("Error %d: failed to set pulse width of right motors", motor_err);
        return;
	}
    LOG_INF("Left motor set to %u us", motors_l_pwm_ns/1000);
    LOG_INF("Right motor set to %u us", motors_r_pwm_ns/1000);
    
    return;
} /* update_motors */


void reset_motors(void)
{
    int motor_err;
    motor_err = pwm_set_pulse_dt(&motors_l, PWM_USEC(1500));
     if (motor_err < 0) {
			LOG_ERR("Error %d: failed to reset left motors", motor_err);
			return;
		}
    motor_err = pwm_set_pulse_dt(&motors_r, PWM_USEC(1500));
    if (motor_err < 0) {
			LOG_ERR("Error %d: failed to reset right motors", motor_err);
			return;
		}
    LOG_INF("Motors turned off (1500 us)");
    return;
} /* reset_motors */


static void config_dk_leds(void)
{
    int err;
    err = dk_leds_init();
    if (err) {
        LOG_ERR("Couldn't init LEDS (err %d)", err);
    }
    return;
} /* config_dk_leds */


void ultrasonic_thread(void)
{
    uint32_t dist_mm; 
    int motor_err;
    uint32_t motor_f_pwm_ns = PWM_USEC(1500);
    int dir;

    for (;;)
    {
        if (current_conn)
        {
            // Move proximity sensor to next position
            motor_err = pwm_set_pulse_dt(&motor_f, motor_f_pwm_ns);
            if (motor_err < 0) 
            {
                LOG_ERR("Error %d: failed to set pulse width of front motor", motor_err);
                return;
            }
            LOG_INF("Front motor set to %u us", motor_f_pwm_ns/1000);

            // Take sensor reading
            dist_mm = sense_distance();
		    LOG_INF("Distance: %u mm", dist_mm);

            // Variable pulse widths        // TODO: MAKE THIS BIT NICER 
            if (dir == 0)
            {   
                if (motor_f_pwm_ns <= min_pulse_f) 
                {
            		dir = 1;
            		motor_f_pwm_ns = min_pulse_f;
            	} 
                else
                {
				motor_f_pwm_ns -= PWM_USEC(10);
		 	    }
            } 
            else
            {
            	motor_f_pwm_ns = motor_f_pwm_ns + PWM_USEC(10);
            	if (motor_f_pwm_ns >= max_pulse_f) 
                {
            		dir = 0;
            		motor_f_pwm_ns = max_pulse_f;
            	}
            }
        }
        k_sleep(K_MSEC(25));
    }
}


void main(void)
{
	int16_t ultrasonic_err;
    int err;
    int blink_status = 0;
	LOG_INF("Hello World! %s\n", CONFIG_BOARD);

    config_dk_leds();
    ultrasonic_err = ultrasonic_init(gpio_dev, ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN);
	if (!ultrasonic_err)
	{
		LOG_DBG("Ultrasonic sensor initialisation failed");
	}

    err = bluetooth_init(&bluetooth_callbacks, &remote_callbacks);
    if (err) {
        LOG_INF("Couldn't initialize Bluetooth. err: %d", err);
    }

    if (!device_is_ready(motors_l.dev)) {
		LOG_ERR("Error: PWM device %s is not ready\n", motors_l.dev->name);
		return;
	}
	if (!device_is_ready(motors_r.dev)) {
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
