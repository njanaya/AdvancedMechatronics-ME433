#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart1
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define HX711_SCK 2   // SCK GPIO2
#define HX711_DT  3   // DT GPIO3
#define MAX_SAMPLES 1000
#define IIR_A 0.90f

void hx711_init() {
    gpio_init(HX711_SCK);
    gpio_set_dir(HX711_SCK, GPIO_OUT);
    gpio_put(HX711_SCK, 0);

    gpio_init(HX711_DT);
    gpio_set_dir(HX711_DT, GPIO_IN);
}

int32_t hx711_read() {
    uint32_t raw = 0;

    // Wait until data is ready
    while (gpio_get(HX711_DT) == 1) {
        tight_loop_contents();
    }

    // Read 24 bits
    for (int i = 0; i < 24; i++) {
        gpio_put(HX711_SCK, 1);
        sleep_us(1);

        raw = raw << 1;
        if (gpio_get(HX711_DT)) {
            raw++;
        }

        gpio_put(HX711_SCK, 0);
        sleep_us(1);
    }

    // 25th pulse sets gain to 128 for next reading
    gpio_put(HX711_SCK, 1);
    sleep_us(1);
    gpio_put(HX711_SCK, 0);
    sleep_us(1);

    // Sign-extend 24-bit two's complement to 32-bit signed int
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }

    return (int32_t)raw;
}


int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
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
    
    //Init HX711 Force Sensor
    sleep_ms(2000);//wait as small amount of time for the chip to boot
    hx711_init();
    printf("HX711 Force Sensor Test\n");

    int num_samples = 0;

printf("Enter number of samples:\n");
scanf("%d", &num_samples);

if (num_samples > MAX_SAMPLES) {
    num_samples = MAX_SAMPLES;
}

int32_t raw_data[MAX_SAMPLES];
float filtered_data[MAX_SAMPLES];
uint32_t time_data[MAX_SAMPLES];

float filtered = 0.0f;

for (int i = 0; i < num_samples; i++) {
    int32_t raw = hx711_read();
    uint32_t time_ms = to_ms_since_boot(get_absolute_time());

    if (i == 0) {
        filtered = raw;
    } else {
        filtered = IIR_A * filtered + (1.0f - IIR_A) * raw;
    }

    raw_data[i] = raw;
    filtered_data[i] = filtered;
    time_data[i] = time_ms;
}

printf("time_ms,raw,filtered\n");

for (int i = 0; i < num_samples; i++) {
    printf("%lu,%ld,%.2f\n",
           time_data[i],
           raw_data[i],
           filtered_data[i]);
}
 
    while (true) {
        sleep_ms(1000);
    }
}
