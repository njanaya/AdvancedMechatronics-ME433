#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

// I2C configuration
#define I2C_PORT       i2c0
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

// MCP23008 address: 0x20 + A2/A1/A0
#define MCP23008_ADDR  0x20

// MCP23008 register map
#define MCP_IODIR   0x00   // 1 = input, 0 = output
#define MCP_IPOL    0x01
#define MCP_GPINTEN 0x02
#define MCP_DEFVAL  0x03
#define MCP_INTCON  0x04
#define MCP_IOCON   0x05
#define MCP_GPPU    0x06   // pull-up enable
#define MCP_INTF    0x07
#define MCP_INTCAP  0x08
#define MCP_GPIO    0x09   // read pin states / write outputs
#define MCP_OLAT    0x0A   // output latch

static bool mcp23008_write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    printf("Writing to register 0x%02X: 0x%02X\n", reg, value);
    int ret = i2c_write_blocking(I2C_PORT, MCP23008_ADDR, buf, 2, false);
    return ret == 2;
}

static bool mcp23008_read_reg(uint8_t reg, uint8_t *value) {
    // Write register address, then repeated-start into read
    printf("write blocking\n");
    int ret = i2c_write_blocking(I2C_PORT, MCP23008_ADDR, &reg, 1, true);
    if (ret != 1) {
        printf("write failed ret = %d\n", ret);
        return false;
    }
    printf("read blocking\n");
    ret = i2c_read_blocking(I2C_PORT, MCP23008_ADDR, value, 1, false);
    return ret == 1;
}

static bool mcp23008_update_reg(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current;
    printf("start register update\n");
    if (!mcp23008_read_reg(reg, &current)) {
        printf("Failed to read register 0x%02X\n", reg);
        return false;
    }
    printf("read current value: 0x%02X\n", current);
    current = (current & ~mask) | (value & mask);
    printf("set new value: 0x%02X\n", current);
    return mcp23008_write_reg(reg, current);
}

static bool mcp23008_pin_mode(uint8_t pin, bool is_input) {
    if (pin > 7) return false;
    printf("entered pin mode\n");
    uint8_t mask = (1u << pin);
    return mcp23008_update_reg(MCP_IODIR, mask, is_input ? mask : 0);
}

static bool mcp23008_digital_write(uint8_t pin, bool high) {
    if (pin > 7) return false;
    uint8_t mask = (1u << pin);
    // Writing OLAT is usually a clean way to control outputs
    return mcp23008_update_reg(MCP_OLAT, mask, high ? mask : 0);
}

static bool mcp23008_digital_read(uint8_t pin, bool *state) {
    if (pin > 7 || state == NULL) return false;

    uint8_t gpio_val;
    if (!mcp23008_read_reg(MCP_GPIO, &gpio_val)) {
        return false;
    }

    *state = (gpio_val & (1u << pin)) != 0;
    return true;
}

static bool mcp23008_pullup(uint8_t pin, bool enable) {
    if (pin > 7) return false;
    uint8_t mask = (1u << pin);
    return mcp23008_update_reg(MCP_GPPU, mask, enable ? mask : 0);
}

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // Init I2C
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
    
    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    uart_puts(UART_ID, " Hello, UART!\n");
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    printf("Starting MCP23008 test...\n");

    int i = 1;
    while (i <= 10) {
        printf("Countdown: %d\n", i);
        i++;
        sleep_ms(1000);
    }

    // while (true) {
    //     printf("Hello, joe!\n");
    //     sleep_ms(1000);
    // }

    // Example setup:
    // GP7 = output (LED)
    // GP0 = input with pull-up (button)


    printf("set GP0 as input\n");
    if (!mcp23008_pin_mode(0, true)) {
        printf("Failed to set GP0 as input\n");
    }

    printf("set GP0 as pull-up\n");
    if (!mcp23008_pullup(0, true)) {
        printf("Failed to enable pull-up on GP0\n");
    }

    printf("set GP7 as output\n");
    if (!mcp23008_pin_mode(7, false)) {
        printf("Failed to set GP7 as output\n");
    }
    
    printf("Starting MCP23008 test...\n");

    while (true) {
        bool button_state = false;

        if (mcp23008_digital_read(0, &button_state)) {
            // If button is wired to ground, pressed = LOW because pull-up is enabled
            bool pressed = !button_state;

            printf("Button state: %d\n", pressed);
            if (!mcp23008_digital_write(7, pressed)) {
                printf("Failed to write GP7\n");
            }

            printf("GP0 raw=%d, pressed=%d\n", button_state, pressed);
        } else {
            printf("Failed to read GP0\n");
        }

        sleep_ms(100);
    }

    return 0;
}