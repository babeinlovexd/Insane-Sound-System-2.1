// --------------------------------------------------
// I2S Slave Receiver PIO (For receiving audio from ESP32s)
// --------------------------------------------------
#pragma once
#include "hardware/pio.h"

// PIO assembly program for receiving I2S audio (Slave Mode)
// Expected wiring: 
// IN pin 0: DIN (Data)
// IN pin 1: BCLK (Bit Clock)
// IN pin 2: LRCK (Word Select)
// We wait for BCLK falling edge to sample DIN on the rising edge.

static const uint16_t i2s_slave_program_instructions[] = {
    //     .wrap_target
    // Wait for LRCK edge (e.g., transition from Left to Right channel)
    // For simplicity, we just sync to the BCLK and sample 32 bits per frame.
    // A robust slave requires syncing to LRCK first.

    // To make this fully relocatable across any absolute GPIO pin (since BCLK, LRCK, and DIN 
    // might not be contiguous), we use `wait x gpio Y` instructions and dynamically 
    // patch the GPIO pin numbers into the compiled assembly array before loading it.
    // Base opcodes: WAIT 0 GPIO 0 = 0x2000, WAIT 1 GPIO 0 = 0x2080
    
    // LRCK Sync (Wait for Left Channel to start, assuming LRCK=0 is Left)
    // We only want to sync to LRCK once per frame (or at start) to establish L/R alignment.
    // For simplicity, we sync to LRCK falling edge at the start of the 32-bit shift loop.
    // Wait LRCK High, Wait LRCK Low.
    0x2080, // [0] wait 1 gpio Y (LRCK)
    0x2000, // [1] wait 0 gpio Y (LRCK)
    
    // Set loop counter for 63 bits (since we want 64 total for L+R)
    0xe03f, // [2] set x, 63
    
    // loop:
    // BCLK Loop
    0x2000, // [3] wait 0 gpio X (BCLK)
    0x2080, // [4] wait 1 gpio X (BCLK)
    
    // Shift DIN into ISR
    0x4001, // [5] in pins, 1 (Uses in_base which we map to data_pin)
    
    0x0043  // [6] jmp x--, loop (Jump back to index 3)
};

static const struct pio_program i2s_slave_program = {
    .instructions = i2s_slave_program_instructions,
    .length = 7,
    .origin = -1,
};

static inline void i2s_slave_program_init(PIO pio, uint sm, uint offset, uint data_pin) {
    pio_sm_config c = pio_get_default_sm_config();
    
    // Configure IN base pin for the `in pins, 1` instruction
    sm_config_set_in_pins(&c, data_pin);
    
    // Shift to left, auto push after 64 bits (32L + 32R)
    // For standard I2S 24-bit padded to 32-bit frames.
    // Wait, PIO shift registers are 32-bit. We push 32 bits at a time.
    sm_config_set_in_shift(&c, false, true, 32);

    // Join FIFOs to double RX capacity
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    // Initialize state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
