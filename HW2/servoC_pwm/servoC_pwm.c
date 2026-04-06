#include "pico/stdlib.h"
#include "hardware/pwm.h"

// Servo constants
const uint SERVO_PIN = 15;
const float FREQUENCY = 50.0f;

// Set servo angle (0 to 180 degrees)
void set_servo_angle(uint gpio, float angle) {
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    
    // Standard servo: 1ms (1000us) = 0 deg, 2ms (2000us) = 180 deg
    // Formula: pulse_width = 1000 + (angle / 180) * 1000
    uint32_t pulse_width_us = 1000 + (uint32_t)(angle * 1000.0f / 180.0f);
    
    // duty = (pulse_width / period) * wrap_value
    // At 50Hz, period = 20,000us. Default wrap is 65535.
    uint32_t duty = (pulse_width_us * 65535) / 20000;
    pwm_set_gpio_level(gpio, duty);
}

int main() {
    stdio_init_all();

    // Set up PWM on the chosen GPIO
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(SERVO_PIN);

    // Set clock divider to get 50Hz (approx 125MHz / (64 * 39062.5) = 50Hz)
    pwm_set_clkdiv(slice_num, 64.0f);
    pwm_set_wrap(slice_num, 39062); // 20ms period
    pwm_set_enabled(slice_num, true);

    while (true) {
        // Sweep from 0 to 180 degrees
        for (int i = 0; i <= 180; i += 1) {
            set_servo_angle(SERVO_PIN, (float)i);
            sleep_ms(10);
        }
        for (int i = 180; i >= 0; i -= 1) {
            set_servo_angle(SERVO_PIN, (float)i);
            sleep_ms(10);
        }
    }
}