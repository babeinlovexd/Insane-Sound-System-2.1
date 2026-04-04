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

// 10 MHz clock = 0.1 us per cycle.
// T0: 3 cycles high (0.3us), 9 cycles low (0.9us)
// T1: 6 cycles high (0.6us), 6 cycles low (0.6us)
// Total = 12 cycles = 1.2us
//
// Instruction breakdown (1 cycle each, plus delays):
// .wrap_target
// bitloop:
//   out x, 1       side 0 [2] ; pull bit into X, wait 2 cycles (total 3 cycles low from previous bit)
//   jmp !x do_zero side 1 [2] ; drive high, if X is 0 jump to do_zero (takes 1 cycle + 2 wait = 3 cycles high)
// do_one:
//   jmp bitloop    side 1 [2] ; X is 1, keep driving high (takes 1 cycle + 2 wait = 3 cycles high)
// do_zero:
//   nop            side 0 [2] ; drive low (takes 1 cycle + 2 wait = 3 cycles low)
// .wrap
// Wait, for a '1' we need 6 cycles high.
// In the above:
// start of bitloop: side is 0, delay 2 (cycles 0,1,2 of the new bit are low? NO! WS2805 bits START with HIGH!)
//
// Correct WS2805 logic:
// Every bit starts with HIGH.
// T0: 3 high, 9 low
// T1: 6 high, 6 low
//
// .wrap_target
// bitloop:
//   out x, 1       side 0 [1] ; (side 0 from end of last bit, lasts 2 cycles: this instr + wait 1)
//   jmp !x do_zero side 1 [2] ; Drive HIGH. Takes 1 instr + 2 wait = 3 cycles high.
// do_one:
//   jmp bitloop    side 1 [2] ; Still HIGH. Takes 1 instr + 2 wait = 3 cycles high. Total high = 6 cycles.
// do_zero:
//   nop            side 0 [3] ; Drive LOW. Takes 1 instr + 3 wait = 4 cycles low. (Total low for '1' = 2+4=6 cycles? Wait.)

static const uint16_t ws2805_program_instructions[] = {
            //     .wrap_target
    0x6121, //  0: out    x, 1            side 0 [1]
    0x1223, //  1: jmp    !x, 3           side 1 [2]
    0x1200, //  2: jmp    0               side 1 [2]
    0xa342, //  3: nop                    side 0 [3]
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
