// --------------------------------------------------
// Dual I2S Master Transmitter PIO (For Amp 1 & Amp 2)
// --------------------------------------------------
#pragma once
#include "hardware/pio.h"

// PIO assembly program for transmitting I2S audio (Dual Master Mode)
// Expected wiring: 
// SET pin 0: BCLK
// SET pin 1: LRCK
// OUT pin 0: DOUT1
// OUT pin 1: DOUT2
// This PIO generates BCLK and LRCK, outputting the same 32-bit audio frame 
// to TWO data out pins (Amp 1 and Amp 2) on every clock cycle.
// BCLK = 0, LRCK = 0 (0b00 = 0)
// BCLK = 1, LRCK = 0 (0b01 = 1)
// BCLK = 0, LRCK = 1 (0b10 = 2)
// BCLK = 1, LRCK = 1 (0b11 = 3)

static const uint16_t i2s_dual_master_program_instructions[] = {
            //     .wrap_target
            // --- LEFT CHANNEL (LRCK = 0) ---
    0xe03e, //  0: set    x, 30                      
            // loop_l:
    0xe000, //  1: set    pins, 0                    
    0x6002, //  2: out    pins, 2                    
    0xe001, //  3: set    pins, 1                    
    0x0041, //  4: jmp    x--, 1                     
    0xe000, //  5: set    pins, 0                    
    0x6002, //  6: out    pins, 2                    
    0xe001, //  7: set    pins, 1                    
            // --- RIGHT CHANNEL (LRCK = 1) ---
    0xe03e, //  8: set    x, 30                      
            // loop_r:
    0xe002, //  9: set    pins, 2                    
    0x6002, // 10: out    pins, 2                    
    0xe003, // 11: set    pins, 3                    
    0x0049, // 12: jmp    x--, 9                     
    0xe002, // 13: set    pins, 2                    
    0x6002, // 14: out    pins, 2                    
    0xe003, // 15: set    pins, 3                    
            //     .wrap
};

static const struct pio_program i2s_dual_master_program = {
    .instructions = i2s_dual_master_program_instructions,
    .length = 16,
    .origin = -1,
};

static inline void i2s_dual_master_program_init(PIO pio, uint sm, uint offset, uint data_pins_base, uint clock_pins_base) {
    pio_sm_config c = pio_get_default_sm_config();
    
    // Configure OUT pins for DOUT1 and DOUT2 (Must be contiguous in hardware for this to work natively)
    sm_config_set_out_pins(&c, data_pins_base, 2);
    // Configure SET pins for BCLK and LRCK (Must be contiguous)
    sm_config_set_set_pins(&c, clock_pins_base, 2);
    
    // Shift out to right, auto pull after 32 bits
    sm_config_set_out_shift(&c, true, true, 32);

    // Join FIFOs to double TX capacity
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Set clock divider for 44.1kHz (assuming 133MHz sys clock)
    // 44100 * 32 bits * 2 channels * 2 edges per bit = 5.644MHz BCLK edges
    float div = clock_get_hz(clk_sys) / (44100 * 64 * 2);
    sm_config_set_clkdiv(&c, div);

    // Initialize state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
