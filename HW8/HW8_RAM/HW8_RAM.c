#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include <math.h>

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define DAC_CS   17 //DAC CS
#define PIN_SCK  18
#define PIN_MOSI 19
#define RAM_CS 20   // RAM CS

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart1
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// RAM Addressing
#define RAM_WRITE 0x02
#define RAM_READ  0x03
#define RAM_RDSR  0x05
#define RAM_WRSR  0x01
#define RAM_SEQ   0x40

// samples for DAC from RAM
#define NSAMPLES 1000
#define PI 3.14159265358979323846

// Helper functions for controlling the chip select lines. These add a few NOPs to ensure timing is correct
static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop");
}

// Function to write a value to the DAC. The MCP4912 expects a 16-bit command, with the 10-bit value in the lower bits
void dac_write(uint8_t channel, uint16_t value) {
    value &= 0x03FF; // 10-bit limit

    uint16_t command = 0;
    command |= (channel & 0x01) << 15; // 0 = A, 1 = B
    command |= 0 << 14;                // unbuffered VREF
    command |= 1 << 13;                // 1x gain
    command |= 1 << 12;                // active mode
    command |= value << 2;             // MCP4912 data bits

    uint8_t data[2];
    data[0] = command >> 8;
    data[1] = command & 0xFF;

    cs_select(DAC_CS);
    spi_write_blocking(SPI_PORT, data, 2);
    cs_deselect(DAC_CS);
}

// Function to initialise the RAM. We need to set it to sequential mode for our read/write functions to work correctly
void ram_init(void) {
    uint8_t buf[2];

    buf[0] = RAM_WRSR;
    buf[1] = RAM_SEQ;

    gpio_put(RAM_CS, 0);
    spi_write_blocking(SPI_PORT, buf, 2);
    gpio_put(RAM_CS, 1);
}
// Function to write data to the RAM. We send a 3-byte header with the command and address, followed by the data bytes
void ram_write(uint16_t address, uint8_t *data, int length) {
    uint8_t header[3];

    header[0] = RAM_WRITE;
    header[1] = (address >> 8) & 0xFF;
    header[2] = address & 0xFF;

    gpio_put(RAM_CS, 0);
    spi_write_blocking(SPI_PORT, header, 3);
    spi_write_blocking(SPI_PORT, data, length);
    gpio_put(RAM_CS, 1);
}

// Function to read data from the RAM. We send a 3-byte header with the command and address, then read back the data bytes
void ram_read(uint16_t address, uint8_t *data, int length) {
    uint8_t header[3];

    header[0] = RAM_READ;
    header[1] = (address >> 8) & 0xFF;
    header[2] = address & 0xFF;

    gpio_put(RAM_CS, 0);
    spi_write_blocking(SPI_PORT, header, 3);
    spi_read_blocking(SPI_PORT, 0, data, length);
    gpio_put(RAM_CS, 1);
}

// Function to generate a sine wave table and write it to the RAM. The sine wave values are converted to 10-bit DAC values and formatted as 16-bit commands for the MCP4912
void make_sine_table(void) {
    uint8_t data[NSAMPLES * 2];

    for (int i = 0; i < NSAMPLES; i++) {
        float angle = 2.0f * PI * i / NSAMPLES;
        float s = 0.5f + 0.5f * sinf(angle);   // 0 to 1
        uint16_t level = (uint16_t)(s * 1023); // 10-bit DAC value

        uint16_t dac_cmd = 0;
        dac_cmd |= 0b0111000000000000;         // DAC config bits
        dac_cmd |= (level << 2);               // MCP4912 uses bits 11:2

        data[2*i]     = (dac_cmd >> 8) & 0xFF;
        data[2*i + 1] = dac_cmd & 0xFF;
    }

    ram_write(0x0000, data, NSAMPLES * 2);
}

int main()
{
    stdio_init_all();

    // SPI initialisation. This example will use SPI at 1MHz.
    spi_init(SPI_PORT, 1000*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(DAC_CS);
    gpio_set_dir(DAC_CS, GPIO_OUT);
    gpio_put(DAC_CS, 1); // Deselect the DAC

    gpio_init(RAM_CS);
    gpio_set_dir(RAM_CS, GPIO_OUT);
    gpio_put(RAM_CS, 1);   // deselect RAM
    // For more examples of SPI use see https://github.com/raspberrypi/pico-examples/tree/master/spi

    ram_init();
    make_sine_table();

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

    int i = 0;

    while (true) {

        uint8_t dac_data[2];

        for (int i = 0; i < NSAMPLES; i++) {
            ram_read(i * 2, dac_data, 2);

            gpio_put(DAC_CS, 0);
            spi_write_blocking(SPI_PORT, dac_data, 2);
            gpio_put(DAC_CS, 1);

            sleep_us(333); // 333 μs delay for ~3 Hz output frequency
           // sleep_ms(1); // 1 ms delay for ~1 kHz output frequency
        }

        // // Generate a 2 Hz sine wave and a 1 Hz triangle wave, and output them to the DAC. The values are calculated on the fly here, but could also be pre-calculated and stored in the RAM if desired
        // uint16_t sine_val = 512 + 511 * sin(2.0 * M_PI * i / 100.0);

        // int tri_index = i % 200;
        // uint16_t tri_val;

        // if (tri_index < 100) {
        //     tri_val = tri_index * 1023 / 99;
        // } else {
        //     tri_val = (199 - tri_index) * 1023 / 99;
        // }

        // dac_write(0, sine_val); // 2 Hz sine on A
        // dac_write(1, tri_val);  // 1 Hz triangle on B

        // i++;
        // if (i >= 200) {
        //     i = 0;
        // }

        // sleep_ms(5);
    }
}
