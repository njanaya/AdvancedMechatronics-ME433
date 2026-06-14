#include <stdio.h>
#include <stdlib.h>
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

// Display parameters
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define CENTER_X 64
#define CENTER_Y 16
#define VECTOR_SCALE 20.0f

//IMU Defines
#define IMU_ADDR 0x68
// config registers
#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define PWR_MGMT_1 0x6B
#define PWR_MGMT_2 0x6C
// sensor data registers:
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H   0x41
#define TEMP_OUT_L   0x42
#define GYRO_XOUT_H  0x43
#define GYRO_XOUT_L  0x44
#define GYRO_YOUT_H  0x45
#define GYRO_YOUT_L  0x46
#define GYRO_ZOUT_H  0x47
#define GYRO_ZOUT_L  0x48
#define WHO_AM_I     0x75

// OLED Functions
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

void ssd1306_drawLine(int x0, int y0, int x1, int y1, int color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;

    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;

    int err = dx + dy;
    int e2;

    while (1)
    {
        ssd1306_drawPixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

// IMU Functions
void imu_write_register(uint8_t reg, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;

    i2c_write_blocking(I2C_PORT, IMU_ADDR, buf, 2, false);
}

void imu_read_register(uint8_t reg, uint8_t *value)
{
    i2c_write_blocking(I2C_PORT, IMU_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, IMU_ADDR, value, 1, false);
}

void imu_read_all(int16_t *ax,
                  int16_t *ay,
                  int16_t *az,
                  int16_t *temp,
                  int16_t *gx,
                  int16_t *gy,
                  int16_t *gz)
{
    uint8_t reg = ACCEL_XOUT_H;
    uint8_t data[14];

    // Tell the IMU where to start reading
    i2c_write_blocking(I2C_PORT, IMU_ADDR, &reg, 1, true);

    // Read 14 bytes
    i2c_read_blocking(I2C_PORT, IMU_ADDR, data, 14, false);

    // Reassemble signed 16-bit values
    *ax   = (int16_t)((data[0]  << 8) | data[1]);
    *ay   = (int16_t)((data[2]  << 8) | data[3]);
    *az   = (int16_t)((data[4]  << 8) | data[5]);

    *temp = (int16_t)((data[6]  << 8) | data[7]);

    *gx   = (int16_t)((data[8]  << 8) | data[9]);
    *gy   = (int16_t)((data[10] << 8) | data[11]);
    *gz   = (int16_t)((data[12] << 8) | data[13]);
}

// init IMU
void imu_init()
{
    imu_write_register(PWR_MGMT_1, 0x00);   // wake up
    imu_write_register(ACCEL_CONFIG, 0x00); // +/-2g
    imu_write_register(GYRO_CONFIG, 0x18);  // +/-2000 dps
}

int main()
{
    stdio_init_all();
    // delay to allow user to open putty or other serial monitor after reset
    sleep_ms(5000); 
   
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

     // Check communication with the IMU by reading the WHO_AM_I register, which should return 0x68 or 0x98 depending on the IMU model
    uint8_t whoami;
    imu_read_register(WHO_AM_I, &whoami);
    printf("WHO_AM_I = 0x%02X\n", whoami); // My chip returns  0x98
    if ((whoami != 0x68) && (whoami != 0x98)) 
    {
        while(1)
        {
            gpio_put(LED_PIN, 1);
            printf("IMU not detected! Check connections.\n");
            sleep_ms(1000);
        }
    }
    // Initialize the IMU
    imu_init();

    while (true) {
        int16_t ax, ay, az;
        int16_t gx, gy, gz;
        int16_t temp;

        imu_read_all(&ax, &ay, &az, &temp, &gx, &gy, &gz);

        printf("AX:%d AY:%d AZ:%d GX:%d GY:%d GZ:%d TEMP:%d\n",
            ax, ay, az, gx, gy, gz, temp);

        ssd1306_clear();

        drawString(0, 0, "IMU");
        // //toggle this section if you want to turn on the count.
        // sprintf(buffer, "Count: %d", count);
        // drawString(0, 12, buffer);
        
        // Convert raw accelerometer readings to g's (assuming +/-2g range and 16-bit signed values)
        float ax_g = ax * 0.000061f;
        float ay_g = ay * 0.000061f;
        // Draw a line representing the acceleration vector on the display
        int x2 = CENTER_X - (int)(ax_g * 20.0f);
        int y2 = CENTER_Y + (int)(ay_g * 20.0f);
        ssd1306_drawLine(CENTER_X, CENTER_Y,
                        x2, y2,
                        1);


        // Check if it's time to toggle the LED for the heartbeat
        absolute_time_t current_time = get_absolute_time();
        // Toggle the LED state if 500ms has passed
        if (absolute_time_diff_us(last_blink_time, current_time) >= 500000) {
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            last_blink_time = current_time;
            if (led_state) {
                count++;
            } 
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

        sleep_ms(10); // 100 Hz
    }
}
