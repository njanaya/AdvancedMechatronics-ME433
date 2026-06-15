#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/adc.h"
#include <math.h>
#include "hardware/pwm.h"
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

// DRV8833 H-Bridge
#define MOTOR_IN1 12   // GP12 -> AIN1
#define MOTOR_IN2 13   // GP13 -> AIN2

#define PWM_WRAP 999
#define PWM_CLKDIV 125.0f
#define MOTOR_DEADBAND 0.005f

// INA219 current sensor
#define INA219_ADDR 0x40
#define INA219_REG_SHUNT_VOLTAGE 0x01
#define INA219_REG_BUS_VOLTAGE   0x02
#define INA219_SHUNT_OHMS 0.1f   // change if your INA219 board uses a different shunt

// Current control parameters
#define MAX_TARGET_CURRENT_A 0.30f
#define CURRENT_KP 1.0f
#define CURRENT_KI 0.005f
#define CURRENT_INTEGRAL_LIMIT 0.25f
#define POSITION_KP 0.008f
#define CURRENT_DEADBAND_A 0.01f

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

//motor and H-Bridge Functions
// command ranges from -1.0 to +1.0
void motor_set(float command)
{
    if(command > 1.0f) command = 1.0f;
    if(command < -1.0f) command = -1.0f;

    // deadband -> brake
    if(fabsf(command) < MOTOR_DEADBAND)
    {
        pwm_set_gpio_level(MOTOR_IN1, PWM_WRAP);
        pwm_set_gpio_level(MOTOR_IN2, PWM_WRAP);
        return;
    }

    uint16_t duty = (uint16_t)(fabsf(command) * PWM_WRAP);

    if(command > 0)
    {
        pwm_set_gpio_level(MOTOR_IN1, duty);
        pwm_set_gpio_level(MOTOR_IN2, 0);
    }
    else
    {
        pwm_set_gpio_level(MOTOR_IN1, 0);
        pwm_set_gpio_level(MOTOR_IN2, duty);
    }
}

void motor_pwm_init(void)
{
    gpio_set_function(MOTOR_IN1, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR_IN2, GPIO_FUNC_PWM);

    uint slice1 = pwm_gpio_to_slice_num(MOTOR_IN1);
    uint slice2 = pwm_gpio_to_slice_num(MOTOR_IN2);

    pwm_set_clkdiv(slice1, PWM_CLKDIV);
    pwm_set_wrap(slice1, PWM_WRAP);
    pwm_set_enabled(slice1, true);

    if (slice2 != slice1) {
        pwm_set_clkdiv(slice2, PWM_CLKDIV);
        pwm_set_wrap(slice2, PWM_WRAP);
        pwm_set_enabled(slice2, true);
    }

    // default to brake
    pwm_set_gpio_level(MOTOR_IN1, PWM_WRAP);
    pwm_set_gpio_level(MOTOR_IN2, PWM_WRAP);
}

//INA219 functions
int16_t ina219_read_reg16(uint8_t reg)
{
    uint8_t buf[2];

    i2c_write_blocking(I2C_PORT, INA219_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, INA219_ADDR, buf, 2, false);

    return (int16_t)((buf[0] << 8) | buf[1]);
}

float ina219_read_shunt_voltage_V(void)
{
    int16_t raw = ina219_read_reg16(INA219_REG_SHUNT_VOLTAGE);

    // INA219 shunt voltage LSB = 10 uV
    return raw * 0.00001f;
}

float ina219_read_current_A(void)
{
    float shunt_voltage = ina219_read_shunt_voltage_V();
    return shunt_voltage / INA219_SHUNT_OHMS;
}

float ina219_read_bus_voltage_V(void)
{
    uint16_t raw = (uint16_t)ina219_read_reg16(INA219_REG_BUS_VOLTAGE);

    // Bus voltage is bits [15:3], LSB = 4 mV
    raw = raw >> 3;
    return raw * 0.004f;
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

    // Set up UART1
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_puts(UART_ID, "Hello, UART!\n");

    // SSD1306 initialization
    ssd1306_setup();
    int oled_count = 0;

    // ADC initialization
    adc_init_inputs();

    // Motor and H-Bridge initialization
    motor_pwm_init();
    float current_integral = 0.0f;
    char buffer[32];

    while (true) {
        uint16_t raw = as5600_get_angle();
        float deg = raw * 360.0f / 4096.0f;

        float adc0 = read_adc0_voltage();
        float adc2 = read_adc2_voltage();

        // Desired position from the breadboard pot
        float target_angle_deg = 270.0f - (adc0 / 3.3f) * 180.0f;

        // Position controller
        float angle_error_deg = target_angle_deg - deg;

        float target_current_A = POSITION_KP * angle_error_deg;

        // Limit current command
        if(target_current_A > MAX_TARGET_CURRENT_A)
            target_current_A = MAX_TARGET_CURRENT_A;

        if(target_current_A < -MAX_TARGET_CURRENT_A)
            target_current_A = -MAX_TARGET_CURRENT_A;

        float current_A = ina219_read_current_A();
        float bus_V = ina219_read_bus_voltage_V();

        float current_error_A = target_current_A - current_A;

        current_integral += current_error_A;

        if (current_integral > CURRENT_INTEGRAL_LIMIT) current_integral = CURRENT_INTEGRAL_LIMIT;
        if (current_integral < -CURRENT_INTEGRAL_LIMIT) current_integral = -CURRENT_INTEGRAL_LIMIT;

        float command = CURRENT_KP * current_error_A + CURRENT_KI * current_integral;

        if (fabsf(target_current_A) < CURRENT_DEADBAND_A) {
            current_integral = 0.0f;
            command = 0.0f;
        }

        motor_set(command);

        // printf("Target: %.3f A  Current: %.3f A  Error: %.3f A  Cmd: %.2f  Bus: %.2f V\n",
        //     target_current_A,
        //     current_A,
        //     current_error_A,
        //     command,
        //     bus_V);

        printf("%lu,%.2f,%.2f\n",
            to_ms_since_boot(get_absolute_time()),
            target_angle_deg,
            deg);

        // OLED display
        oled_count++;

        if (oled_count >= 10) {
            oled_count = 0;

            ssd1306_clear();

            sprintf(buffer, "T:%.2fA", target_current_A);
            drawString(0, 0, buffer);

            sprintf(buffer, "I:%.2fA", current_A);
            drawString(0, 12, buffer);

            sprintf(buffer, "A:%.0f T:%.0f", deg, target_angle_deg);
            drawString(0, 24, buffer);

            ssd1306_update();
        }

        sleep_ms(10); // remove/reduce later for control
    }
}