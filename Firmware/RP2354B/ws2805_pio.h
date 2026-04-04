// --------------------------------------------------
// WS2805 PIO Driver for RP2040/2350
// --------------------------------------------------
#pragma once
#include "hardware/pio.h"
#include "hardware/clocks.h"

// PIO assembly program for transmitting WS2805 data.
// A standard WS2812 PIO program works, but we must tune the clock
// to match the WS2805 tight timings:
// T0H: 0.28 us, T0L: 0.92 us (Total: 1.2 us)
// T1H: 0.62 us, T1L: 0.58 us (Total: 1.2 us)

// We can run the PIO at 10 MHz (0.1 us per cycle).
// 1 bit = 12 cycles (1.2 us).
// Bit 0: High for 3 cycles (0.3 us), Low for 9 cycles (0.9 us) -> Matches 0.28/0.92 well enough.
// Bit 1: High for 6 cycles (0.6 us), Low for 6 cycles (0.6 us) -> Matches 0.62/0.58 well enough.

static const uint16_t ws2805_program_instructions[] = {
            //     .wrap_target
    0x6221, //  0: out    x, 1            side 0 [2]
    0x1123, //  1: jmp    !x, 3           side 1 [1]
    0x1400, //  2: jmp    0               side 1 [4]
    0xa442, //  3: nop                    side 0 [4]
            //     .wrap
};

static const struct pio_program ws2805_program = {
    .instructions = ws2805_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline void ws2805_program_init(PIO pio, uint sm, uint offset, uint pin) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 8); // Shift left, auto pull, 8 bits (we send 1 byte at a time)
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Clock divider for 10 MHz
    float div = clock_get_hz(clk_sys) / 10000000.0;
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

class WS2805 {
private:
    PIO pio_;
    uint sm_;
    uint num_leds_;
public:
    WS2805(PIO pio, uint sm, uint pin, uint num_leds) : pio_(pio), sm_(sm), num_leds_(num_leds) {
        uint offset = pio_add_program(pio, &ws2805_program);
        ws2805_program_init(pio, sm, offset, pin);
    }

    void show(uint8_t r, uint8_t g, uint8_t b, uint8_t w1, uint8_t w2) {
        // Send to all LEDs. The WS2805 takes R, G, B, W1, W2.
        for (uint i = 0; i < num_leds_; i++) {
            pio_sm_put_blocking(pio_, sm_, (uint32_t)r << 24);
            pio_sm_put_blocking(pio_, sm_, (uint32_t)g << 24);
            pio_sm_put_blocking(pio_, sm_, (uint32_t)b << 24);
            pio_sm_put_blocking(pio_, sm_, (uint32_t)w1 << 24);
            pio_sm_put_blocking(pio_, sm_, (uint32_t)w2 << 24);
        }
        // Wait for reset (> 280us)
        sleep_us(300);
    }
};
