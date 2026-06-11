#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/adc.h"
#include "ssd1306.h"
#include "font.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9
#define I2C_BAUDRATE   100000

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart1
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 4
#define UART_RX_PIN 5

//AS5600 Magnetic Encoder
#define AS5600_ADDR 0x36

//ADC2 input from Servo Potentiometer
#define SRV_POT_ADC_PIN 28
#define SRV_POT_ADC_INPUT 2
#define BB_POT_ADC_PIN 26
#define BB_POT_ADC_INPUT 0
#define ADC_MAX 4095.0f
#define ADC_REF_VOLTAGE 3.3f

//Draws characters on OLED
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
//Creates string for the OLED
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
//AS5600 Read a Register
uint8_t as5600_read_reg(uint8_t reg)
{
    uint8_t data;

    i2c_write_blocking(
        I2C_PORT,
        AS5600_ADDR,
        &reg,
        1,
        true);     // repeated start

    i2c_read_blocking(
        I2C_PORT,
        AS5600_ADDR,
        &data,
        1,
        false);

    return data;
}
//Read the 12-bit Angle
uint16_t as5600_get_angle(void)
{
    uint8_t reg = 0x0E;
    uint8_t buf[2];

    i2c_write_blocking(
        I2C_PORT,
        AS5600_ADDR,
        &reg,
        1,
        true);

    i2c_read_blocking(
        I2C_PORT,
        AS5600_ADDR,
        buf,
        2,
        false);

    return ((buf[0] & 0x0F) << 8) | buf[1];
}
//Calculate Angle from register Read
float as5600_get_angle_deg(void)
{
    uint16_t angle = as5600_get_angle();
    return angle * 360.0f / 4096.0f;
}
//initialize the ADC
void adc_init_inputs(void)
{
    adc_init();
    adc_gpio_init(SRV_POT_ADC_PIN);
    adc_gpio_init(BB_POT_ADC_PIN);
}
//Read and convert ADC2
float read_adc2_voltage(void)
{
    adc_select_input(SRV_POT_ADC_INPUT);
    uint16_t raw = adc_read();
    return raw * ADC_REF_VOLTAGE / ADC_MAX;
}
//Read and convert ADC0
float read_adc0_voltage(void)
{
    adc_select_input(BB_POT_ADC_INPUT);
    uint16_t raw = adc_read();
    return raw * ADC_REF_VOLTAGE / ADC_MAX;
}
int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 100Khz.
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    uart_puts(UART_ID, " Hello, UART!\n");
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    // SSD1306 initialization
    ssd1306_setup();

    //ADC initialization
    adc_init_inputs();     // Initialize ADC2 and ADC0

    int count = 0;
    char buffer[32];

    while (true) {
        uint16_t raw = as5600_get_angle(); // read encoder raw
        float deg = raw * 360.0f / 4096.0f; // convert raw to angle

    float adc0 = read_adc0_voltage();
    float adc2 = read_adc2_voltage();

    printf("E_Angle: %.2f deg  ADC0: %.3f V  ADC2: %.3f V\n",
           deg,
           adc0,
           adc2);
        
        // Code to write to OLED
        ssd1306_clear();

        sprintf(buffer, "ADC0:%.3f V", adc0);
        drawString(0, 0, buffer);

        sprintf(buffer, "ADC2:%.3f V", adc2);
        drawString(0, 12, buffer);

        sprintf(buffer, "E_Angle: %.2f", deg);
        drawString(0, 24, buffer);

        ssd1306_update();

        printf("Display updated: %s\n", buffer);
        uart_puts(UART_ID, "Screen updated\n");

        count++;
        sleep_ms(1000);
    }
}
