// Copyright (c) 2013, Adam Simpkins
#include "keyboard_v2.h"

#include <avrpp/progmem.h>
#include <avrpp/usb_hid_keyboard.h>
#include <avrpp/util.h>

#include <util/delay.h>

static const uint8_t PROGMEM default_key_table[18 * 8] = {
    // Row 0
    KEY_TAB, KEY_NONE, KEY_HOME, KEY_BACKSLASH,
    KEY_LEFT_BRACE, KEY_MINUS, KEY_BACKSPACE, KEY_LEFT_GUI,
    // Row 1
    KEY_RIGHT, KEY_N, KEY_M, KEY_COMMA,
    KEY_PERIOD, KEY_QUOTE, KEY_RIGHT_SHIFT, KEY_NONE,
    // Row 2
    KEY_NONE, KEY_Y, KEY_U, KEY_I,
    KEY_O, KEY_P, KEY_VOLUME_DOWN, KEY_NONE,
    // Row 3
    KEY_NONE, KEY_F7, KEY_F8, KEY_F9,
    KEY_F10, KEY_F11, KEY_F12, KEY_NONE,
    // Row 4
    KEY_APPLICATION, KEYPAD_MINUS, KEYPAD_1, KEYPAD_2,
    KEYPAD_3, KEYPAD_PLUS, KEY_RIGHT_ALT, KEY_NONE,
    // Row 5
    KEY_CAPS_LOCK, KEY_NUM_LOCK, KEYPAD_7, KEYPAD_8,
    KEYPAD_9, KEYPAD_ASTERIX, KEY_SCROLL_LOCK, KEY_NONE,
    // Row 6
    KEY_NONE, KEY_LEFT_CTRL, KEY_A, KEY_S,
    KEY_D, KEY_F, KEY_G, KEY_ENTER,
    // Row 7
    KEY_NONE, KEY_MENU, KEY_1, KEY_2,
    KEY_3, KEY_4, KEY_5, KEY_NONE,
    // Row 8
    KEY_SPACE, KEY_DELETE, KEY_EQUAL, KEY_RIGHT_BRACE,
    KEY_SLASH, KEY_END, KEY_NONE, KEY_UP,
    // Row 9
    KEY_DOWN, KEY_H, KEY_J, KEY_K,
    KEY_L, KEY_SEMICOLON, KEY_RIGHT_CTRL, KEY_NONE,
    // Row 10
    KEY_NONE, KEY_6, KEY_7, KEY_8,
    KEY_9, KEY_0, KEY_TILDE, KEY_NONE,
    // Row 11
    KEY_PAGE_UP, KEYPAD_ENTER, KEYPAD_0, KEY_INSERT,
    KEY_BACKSPACE, KEYPAD_PERIOD, KEY_PAGE_DOWN, KEY_NONE,
    // Row 12
    KEY_F13, KEYPAD_EQUAL, KEYPAD_4, KEYPAD_5,
    KEYPAD_6, KEYPAD_SLASH, KEY_F15, KEY_NONE,
    // Row 13
    KEY_NONE, KEY_LEFT_SHIFT, KEY_Z, KEY_X,
    KEY_C, KEY_V, KEY_B, KEY_ESC,
    // Row 14
    KEY_NONE, KEY_VOLUME_UP, KEY_Q, KEY_W,
    KEY_E, KEY_R, KEY_T, KEY_NONE,
    // Row 15
    KEY_NONE, KEY_F1, KEY_F2, KEY_F3,
    KEY_F4, KEY_F5, KEY_F6, KEY_NONE,
    // Row 16
    KEY_ENTER, KEY_NONE, KEY_NONE, KEY_LEFT_ALT,
    KEY_NONE, KEY_NONE, KEY_NONE, KEY_RIGHT_ALT,
    // Row 17
    KEY_RIGHT_GUI, KEY_NONE, KEY_NONE, KEY_NONE,
    KEY_MUTE, KEY_NONE, KEY_NONE, KEY_LEFT,
};
static const uint8_t PROGMEM default_modifier_table[18 * 8] = {
    0, 0, 0, 0, 0, 0, 0, MOD_LEFT_GUI,  // Row 0
    0, 0, 0, 0, 0, 0, MOD_RIGHT_SHIFT, 0,  // Row 1
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 2
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 3
    0, 0, 0, 0, 0, 0, MOD_RIGHT_ALT, 0,  // Row 4
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 5
    0, MOD_LEFT_CTRL, 0, 0, 0, 0, 0, 0,  // Row 6
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 7
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 8
    0, 0, 0, 0, 0, 0, MOD_RIGHT_CTRL, 0,  // Row 9
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 10
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 11
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 12
    0, MOD_LEFT_SHIFT, 0, 0, 0, 0, 0, 0,  // Row 13
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 14
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 15
    0, 0, 0, MOD_LEFT_ALT, 0, 0, 0, MOD_RIGHT_ALT,  // Row 16
    MOD_RIGHT_GUI, 0, 0, 0, 0, 0, 0, 0,  // Row 17
};

void
KeyboardV2::prepare() {
    // Set columns to output, low
    DDRB = 0xff;
    PORTB = 0x00;

    // Set rows to input, with pull-up resistors
    DDRD = 0x00;
    PORTD = 0xff;
    DDRF = 0x00;
    PORTF = 0xff;
    DDRE = 0x00;
    PORTE = 0xc0;

    init(8, 18, pgm_cast(default_key_table), pgm_cast(default_modifier_table));
}

void
KeyboardV2::scanImpl() {
    for (uint8_t col = 0; col < 8; ++col) {
        // Turn the specified column pin into an output pin, and bring it low.
        //
        // It is important that the other column pins are configured as inputs.
        // If multiple keys are pressed down, they may create a circuit
        // connecting to other column pins as well.  We don't want an output
        // voltage on these pins to affect our reading on the column currently
        // being scanned.
        DDRB = 0x01 << col;
        PORTB = 0xff & ~(0x01 << col);

        // We need a very brief delay here to allow the signal
        // to propagate to the input pins.
        _delay_us(5);

        // Read which row pins are active
        uint8_t d = ~PIND;
        uint8_t f = ~PINF;
        uint8_t e = ~PINE;

        if (UNLIKELY(d)) {
            for (uint8_t n = 0; n < 8; ++n) {
                if (d & (0x1 << n)) {
                    keyPressed(col, n);
                }
            }
        }
        if (UNLIKELY(f)) {
            for (uint8_t n = 0; n < 8; ++n) {
                if (f & (0x1 << n)) {
                    keyPressed(col, n + 8);
                }
            }
        }
        if (e & 0x40) {
            keyPressed(col, 16);
        }
        if (e & 0x80) {
            keyPressed(col, 17);
        }
    }
    DDRB = 0x00;
    PORTB = 0xff;
}
