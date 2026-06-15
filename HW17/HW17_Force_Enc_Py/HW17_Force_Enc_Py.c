// HW17_Force_Enc_Py
// Reads AS5600 magnetic encoder over I2C and HX711 load cell amplifier by bit-banged GPIO.
// Streams CSV over USB serial for a Python graphics program.
//
// Wiring used here:
// AS5600 SDA -> GP8
// AS5600 SCL -> GP9
// HX711 SCK  -> GP2
// HX711 DT   -> GP3

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// ---------- I2C / AS5600 ----------
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9
#define I2C_BAUDRATE 100000
#define AS5600_ADDR 0x36

// ---------- HX711 ----------
#define HX711_SCK 2
#define HX711_DT  3
#define IIR_A 0.90f

// ---------- Timing ----------
#define SAMPLE_DELAY_MS 20   // about 50 Hz, limited by HX711 update rate

void as5600_init(void) {
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

uint16_t as5600_get_angle_raw(void) {
    uint8_t reg = 0x0E;   // ANGLE high byte register
    uint8_t buf[2];

    int write_ok = i2c_write_blocking(I2C_PORT, AS5600_ADDR, &reg, 1, true);
    if (write_ok < 0) {
        return 0;
    }

    int read_ok = i2c_read_blocking(I2C_PORT, AS5600_ADDR, buf, 2, false);
    if (read_ok < 0) {
        return 0;
    }

    return ((buf[0] & 0x0F) << 8) | buf[1];  // 12-bit value, 0 to 4095
}

float as5600_get_angle_deg(void) {
    uint16_t raw = as5600_get_angle_raw();
    return raw * 360.0f / 4096.0f;
}

void hx711_init(void) {
    gpio_init(HX711_SCK);
    gpio_set_dir(HX711_SCK, GPIO_OUT);
    gpio_put(HX711_SCK, 0);

    gpio_init(HX711_DT);
    gpio_set_dir(HX711_DT, GPIO_IN);
}

int32_t hx711_read(void) {
    uint32_t raw = 0;

    // Wait for HX711 data ready.
    // DT goes low when a conversion is ready.
    while (gpio_get(HX711_DT) == 1) {
        tight_loop_contents();
    }

    // Read 24 bits, MSB first.
    for (int i = 0; i < 24; i++) {
        gpio_put(HX711_SCK, 1);
        sleep_us(1);

        raw <<= 1;
        if (gpio_get(HX711_DT)) {
            raw++;
        }

        gpio_put(HX711_SCK, 0);
        sleep_us(1);
    }

    // 25th pulse sets gain = 128 for the next reading.
    gpio_put(HX711_SCK, 1);
    sleep_us(1);
    gpio_put(HX711_SCK, 0);
    sleep_us(1);

    // Sign extend 24-bit two's complement value.
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }

    return (int32_t)raw;
}

int main(void) {
    stdio_init_all();

    // Give USB serial time to connect after reset.
    sleep_ms(2000);

    as5600_init();
    hx711_init();

    float force_filtered = 0.0f;
    bool filter_initialized = false;

    // CSV header for Python.
    printf("time_ms,angle_deg,force_raw,force_filtered\n");

    while (true) {
        uint32_t time_ms = to_ms_since_boot(get_absolute_time());

        float angle_deg = as5600_get_angle_deg();
        int32_t force_raw = hx711_read();

        if (!filter_initialized) {
            force_filtered = (float)force_raw;
            filter_initialized = true;
        } else {
            force_filtered = IIR_A * force_filtered + (1.0f - IIR_A) * (float)force_raw;
        }

        printf("%lu,%.2f,%ld,%.2f\n",
               time_ms,
               angle_deg,
               force_raw,
               force_filtered);

        sleep_ms(SAMPLE_DELAY_MS);
    }
}
