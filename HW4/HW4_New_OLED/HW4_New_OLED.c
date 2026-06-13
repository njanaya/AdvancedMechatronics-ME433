#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "ssd1306.h"
#include "font.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA_PIN    8
#define I2C_SCL_PIN    9
#define I2C_BAUDRATE   100000

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart1
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 4
#define UART_RX_PIN 5

//LED Heartbeat pin
#define LED_PIN 15

//ADC Potentiometer pin
#define ADC_PIN 28
#define ADC_CHANNEL 2

static void drawChar(int x, int y, char c) {
    if (c < 0x20 || c > 0x7F) {
        c = '?';
    }

    for (int col = 0; col < 5; col++) {
        unsigned char bits = ASCII[c - 0x20][col];

        for (int row = 0; row < 8; row++) {
            if (bits & (1 << row)) {
                ssd1306_drawPixel(x + col, y + row, 1);
            } else {
                ssd1306_drawPixel(x + col, y + row, 0);
            }
        }
    }

    // one blank column for spacing
    for (int row = 0; row < 8; row++) {
        ssd1306_drawPixel(x + 5, y + row, 0);
    }
}

static void drawString(int x, int y, const char *s) {
    while (*s) {
        drawChar(x, y, *s);
        x += 6;  // 5 pixels wide + 1 pixel spacing

        // stop if we are off the display
        if (x > 122) {
            break;
        }

        s++;
    }
}

int main()
{
    stdio_init_all();

    //initialise the ADC for the potentiometer
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHANNEL);

    // I2C Initialisation. 
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    uart_puts(UART_ID, " Hello, UART!\n");
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

     // SSD1306 initialization
    ssd1306_setup();

    int count = 0;
    char buffer[32];
    // Set up a variable to track the last time the LED was toggled for the heartbeat
    absolute_time_t last_blink_time = get_absolute_time();
    // set up a variable to track the LED state for the heartbeat
    bool led_state = false;

    //Impliment Frames per sec. 
    float fps = 0.0;
    absolute_time_t last_frame_time = get_absolute_time();

    absolute_time_t current_frame_time = get_absolute_time();

    int64_t frame_time_us =
        absolute_time_diff_us(last_frame_time, current_frame_time);

    if (frame_time_us > 0) {
        fps = 1000000.0f / frame_time_us;
    }

    last_frame_time = current_frame_time;
    
    while (true) {
        ssd1306_clear();

        drawString(0, 0, "Hello Nick");

        current_frame_time = get_absolute_time();

        //toggle this section if you want to turn on the count.
        // sprintf(buffer, "Count: %d", count);
        // drawString(0, 12, buffer);

        //read the potentiometer value and display it on the screen
        adc_select_input(ADC_CHANNEL);
        uint16_t adc_raw = adc_read();
        float voltage = adc_raw * 3.3f / 4095.0f;
        sprintf(buffer, "ADC2: %.2f V", voltage);
        drawString(0, 12, buffer);

       // ssd1306_update();

        // commented out timmer that was blocking the screen update for LED heartbeat, you can uncomment if you want to see the LED blink on and off once per second, but it will block the screen update while it is running.
        // //blink on and off once per second
        // led_state = !led_state;
        // gpio_put(LED_PIN, led_state);
        // ssd1306_drawPixel(0, 24, 1);
        // ssd1306_update();
        // sleep_ms(500);
        // led_state = !led_state;
        // gpio_put(LED_PIN, led_state);
        // ssd1306_drawPixel(0, 24, 0);
        // ssd1306_update();
        // sleep_ms(500);

        sprintf(buffer, "FPS: %.1f", fps);
        drawString(4, 24, buffer);


        // now calculate FPS for the next frame
        current_frame_time = get_absolute_time();
        int64_t frame_time_us = absolute_time_diff_us(last_frame_time, current_frame_time);

        if (frame_time_us > 0) {
            fps = 1000000.0f / frame_time_us;
        }

        last_frame_time = current_frame_time;

        // Check if it's time to toggle the LED for the heartbeat
        absolute_time_t current_time = get_absolute_time();
        // Toggle the LED state if 500ms has passed
        if (absolute_time_diff_us(last_blink_time, current_time) >= 500000) {
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            last_blink_time = current_time;
        }
        // Update the display pixel with the LED state
        if (led_state) {
            ssd1306_drawPixel(0, 24, 1);
        } else {
            ssd1306_drawPixel(0, 24, 0);
        }
        // Update the display with the new content
        ssd1306_update();
        // Print the count to the console and send a message over UART
        printf("Display updated: %s\n", buffer);
        uart_puts(UART_ID, "Screen updated\n");
        count++;
    }
}
