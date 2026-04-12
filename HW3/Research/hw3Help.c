#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// I2C configuration
#define I2C_PORT       i2c0
#define I2C_SDA_PIN    4
#define I2C_SCL_PIN    5
#define I2C_BAUDRATE   100000

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
    int ret = i2c_write_blocking(I2C_PORT, MCP23008_ADDR, buf, 2, false);
    return ret == 2;
}

static bool mcp23008_read_reg(uint8_t reg, uint8_t *value) {
    // Write register address, then repeated-start into read
    int ret = i2c_write_blocking(I2C_PORT, MCP23008_ADDR, &reg, 1, true);
    if (ret != 1) {
        return false;
    }

    ret = i2c_read_blocking(I2C_PORT, MCP23008_ADDR, value, 1, false);
    return ret == 1;
}

static bool mcp23008_update_reg(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current;
    if (!mcp23008_read_reg(reg, &current)) {
        return false;
    }

    current = (current & ~mask) | (value & mask);
    return mcp23008_write_reg(reg, current);
}

static bool mcp23008_pin_mode(uint8_t pin, bool is_input) {
    if (pin > 7) return false;
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

    printf("Starting MCP23008 test...\n");

    // Example setup:
    // GP0 = output (LED)
    // GP1 = input with pull-up (button)
    if (!mcp23008_pin_mode(0, false)) {
        printf("Failed to set GP0 as output\n");
    }

    if (!mcp23008_pin_mode(1, true)) {
        printf("Failed to set GP1 as input\n");
    }

    if (!mcp23008_pullup(1, true)) {
        printf("Failed to enable pull-up on GP1\n");
    }

    while (true) {
        bool button_state = false;

        if (mcp23008_digital_read(1, &button_state)) {
            // If button is wired to ground, pressed = LOW because pull-up is enabled
            bool pressed = !button_state;

            if (!mcp23008_digital_write(0, pressed)) {
                printf("Failed to write GP0\n");
            }

            printf("GP1 raw=%d, pressed=%d\n", button_state, pressed);
        } else {
            printf("Failed to read GP1\n");
        }

        sleep_ms(100);
    }

    return 0;
}