#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "ssd1306.h"
#include "font.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

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

//mouse mode button pin
#define MODE_BUTTON_PIN 2

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

volatile int16_t imu_ax = 0;
volatile int16_t imu_ay = 0;
volatile bool remote_mode = false;

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

//mode button for mouse mode toggle
void button_task(void)
{
    static bool last_button = true;
    static absolute_time_t last_press_time;

    bool button_now = gpio_get(MODE_BUTTON_PIN); // true = not pressed, false = pressed

    if (last_button == true && button_now == false) {
        if (absolute_time_diff_us(last_press_time, get_absolute_time()) > 250000) {
            remote_mode = !remote_mode;
            last_press_time = get_absolute_time();
        }
    }

    last_button = button_now;
}

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void hid_task(void);

/*------------- MAIN -------------*/
int main()
{
    // delay to allow user to open putty or other serial monitor after reset
    //sleep_ms(5000); 
    board_init();

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }
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

    gpio_init(MODE_BUTTON_PIN);
    gpio_set_dir(MODE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(MODE_BUTTON_PIN);
    
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

    //  // Check communication with the IMU by reading the WHO_AM_I register, which should return 0x68 or 0x98 depending on the IMU model
    // uint8_t whoami;
    // imu_read_register(WHO_AM_I, &whoami);
    // printf("WHO_AM_I = 0x%02X\n", whoami); // My chip returns  0x98
    // if ((whoami != 0x68) && (whoami != 0x98)) 
    // {
    //     while(1)
    //     {
    //         gpio_put(LED_PIN, 1);
    //         printf("IMU not detected! Check connections.\n");
    //         sleep_ms(1000);
    //     }
    // }
    // Initialize the IMU
    imu_init();

    while (true) {
        tud_task(); // tinyusb device task
        led_blinking_task();
        hid_task();
        button_task();

        int16_t ax, ay, az;
        int16_t gx, gy, gz;
        int16_t temp;

        imu_read_all(&ax, &ay, &az, &temp, &gx, &gy, &gz);

        imu_ax = ay;
        imu_ay = ax;

        printf("AX:%d AY:%d AZ:%d GX:%d GY:%d GZ:%d TEMP:%d\n",
            ax, ay, az, gx, gy, gz, temp);

        // ssd1306_clear();

        // drawString(0, 0, "Mouse");
        // // //toggle this section if you want to turn on the count.
        // // sprintf(buffer, "Count: %d", count);
        // // drawString(0, 12, buffer);
        
        // // Convert raw accelerometer readings to g's (assuming +/-2g range and 16-bit signed values)
        // float ax_g = ax * 0.000061f;
        // float ay_g = ay * 0.000061f;
        // // Draw a line representing the acceleration vector on the display
        // int x2 = CENTER_X - (int)(ax_g * 20.0f);
        // int y2 = CENTER_Y + (int)(ay_g * 20.0f);
        // ssd1306_drawLine(CENTER_X, CENTER_Y,
        //                 x2, y2,
        //                 1);


        // // Check if it's time to toggle the LED for the heartbeat
        // absolute_time_t current_time = get_absolute_time();
        // // Toggle the LED state if 500ms has passed
        // if (absolute_time_diff_us(last_blink_time, current_time) >= 500000) {
        //     led_state = !led_state;
        //     gpio_put(LED_PIN, led_state);
        //     last_blink_time = current_time;
        //     if (led_state) {
        //         count++;
        //     } 
        // }
        // // Update the display pixel with the LED state
        // if (led_state) {
        //     ssd1306_drawPixel(0, 24, 1);
        // } else {
        //     ssd1306_drawPixel(0, 24, 0);
        // }
        // // Update the display with the new content
        // ssd1306_update();
        // // Print the count to the console and send a message over UART
        // printf("Display updated\n");
        // uart_puts(UART_ID, "Screen updated\n");

        // sleep_ms(10); // 100 Hz



    }
}


//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
  // skip if hid is not ready yet
  if ( !tud_hid_ready() ) return;

  switch(report_id)
  {
    case REPORT_ID_KEYBOARD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_keyboard_key = false;

      if ( btn )
      {
        uint8_t keycode[6] = { 0 };
        keycode[0] = HID_KEY_A;

        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        has_keyboard_key = true;
      }else
      {
        // send empty key report if previously has key pressed
        if (has_keyboard_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        has_keyboard_key = false;
      }
    }
    break;

    case REPORT_ID_MOUSE:
{
    int8_t mouse_x = 0;
    int8_t mouse_y = 0;

    if (remote_mode) {
        static int step = 0;

        if (step < 25) {
            mouse_x = 2;
            mouse_y = 0;
        } else if (step < 50) {
            mouse_x = 0;
            mouse_y = 2;
        } else if (step < 75) {
            mouse_x = -2;
            mouse_y = 0;
        } else {
            mouse_x = 0;
            mouse_y = -2;
        }

        step++;
        if (step >= 100) {
            step = 0;
        }
    } else {
        if (abs(imu_ax) > 2000) {
            mouse_x = imu_ax / 2000;
        }

        if (abs(imu_ay) > 2000) {
            mouse_y = imu_ay / 2000;
        }

        if (mouse_x > 20) mouse_x = 20;
        if (mouse_x < -20) mouse_x = -20;
        if (mouse_y > 20) mouse_y = 20;
        if (mouse_y < -20) mouse_y = -20;
    }

    tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, mouse_x, mouse_y, 0, 0);
}
break;

    case REPORT_ID_CONSUMER_CONTROL:
    {
      // use to avoid send multiple consecutive zero report
      static bool has_consumer_key = false;

      if ( btn )
      {
        // volume down
        uint16_t volume_down = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2);
        has_consumer_key = true;
      }else
      {
        // send empty key report (release key) if previously has key pressed
        uint16_t empty_key = 0;
        if (has_consumer_key) tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
        has_consumer_key = false;
      }
    }
    break;

    case REPORT_ID_GAMEPAD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_gamepad_key = false;

      hid_gamepad_report_t report =
      {
        .x   = 0, .y = 0, .z = 0, .rz = 0, .rx = 0, .ry = 0,
        .hat = 0, .buttons = 0
      };

      if ( btn )
      {
        report.hat = GAMEPAD_HAT_UP;
        report.buttons = GAMEPAD_BUTTON_A;
        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

        has_gamepad_key = true;
      }else
      {
        report.hat = GAMEPAD_HAT_CENTERED;
        report.buttons = 0;
        if (has_gamepad_key) tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        has_gamepad_key = false;
      }
    }
    break;

    default: break;
  }
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();

  // Remote wakeup
  if ( tud_suspended() && btn )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }else
  {
    // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
    send_hid_report(REPORT_ID_MOUSE, btn);
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  uint8_t next_report_id = report[0] + 1u;

  if (next_report_id < REPORT_ID_COUNT)
  {
    send_hid_report(next_report_id, board_button_read());
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}


/*
 * The MIT License (MIT) - Moved to bottom.
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */