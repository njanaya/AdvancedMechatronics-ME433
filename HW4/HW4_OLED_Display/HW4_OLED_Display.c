#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "ssd1306.h"
#include "font.h"

// I2C defines
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

// UART defines
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

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

int main() {
    stdio_init_all();

    // I2C initialization at 400 kHz
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // UART initialization
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_puts(UART_ID, "UART started\n");

    // SSD1306 initialization
    ssd1306_setup();

    int count = 0;
    char buffer[32];

    while (true) {
        ssd1306_clear();

        drawString(0, 0, "Hello Nick");

        sprintf(buffer, "Count: %d", count);
        drawString(0, 16, buffer);

        ssd1306_update();

        printf("Display updated: %s\n", buffer);
        uart_puts(UART_ID, "Screen updated\n");

        count++;
        sleep_ms(1000);
    }
}