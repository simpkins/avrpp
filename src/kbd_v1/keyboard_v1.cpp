// Copyright (c) 2013, Adam Simpkins
#include "keyboard_v1.h"

#include <avrpp/progmem.h>
#include <avrpp/usb_hid_keyboard.h>
#include <avrpp/util.h>

#include <util/delay.h>

/*
 * Key maps.
 *
 * The Maltron keyboard wiring rows and columns somewhat match up to physical
 * rows and columns, but diverges in various places (perhaps to avoid common
 * ghosting patterns).
 *
 * However, when I soldered the wires up to my controller, I kept the rows in
 * order for the F pins, but the columns are wired up rather arbitrarily to the
 * B and C pins.  This makes the column layout here somewhat unintuitive.
 */

/*
 * The default Maltron 89-series QWERTY layout.
 */
static const uint8_t PROGMEM maltron89_qwerty_key_table[8 * 16] = {
    // Column 0
    KEY_NONE, KEY_RIGHT, KEY_NONE, KEY_NONE,
    KEY_LEFT, KEY_PAGE_DOWN, KEY_PAGE_UP, KEY_NONE,
    KEY_NONE, KEY_SLASH, KEY_SPACE, KEY_DOWN,
    KEY_UP, KEY_ENTER, KEY_BACKSLASH, KEY_NONE,
    // Column 1
    KEY_NONE, KEY_ENTER /* right thumb return */, KEY_MINUS, KEY_EQUAL,
    KEY_TAB, KEY_RIGHT_ALT /* AltGr */, KEY_MENU, KEY_NONE,
    KEY_RIGHT_GUI, KEY_RIGHT_BRACE, KEY_DELETE, KEY_END,
    KEY_HOME, KEY_BACKSPACE, KEY_LEFT_BRACE, KEY_LEFT_GUI,
    // Column 2
    KEY_X, KEY_RIGHT_CTRL, KEY_V, KEY_M,
    KEY_LEFT_CTRL, KEY_PERIOD, KEY_NONE, KEY_RIGHT_SHIFT,
    KEY_QUOTE, KEY_COMMA, KEY_N, KEY_RIGHT_ALT,
    KEY_LEFT_ALT, KEY_B, KEY_C, KEY_Z,
    // Column 3
    KEY_S, KEYPAD_PERIOD, KEY_F, KEY_J,
    KEYPAD_ENTER, KEY_L, KEY_INSERT, KEY_CAPS_LOCK /* right caps lock */,
    KEY_SEMICOLON, KEY_K, KEY_H, KEY_BACKSPACE /* keypad backspace */,
    KEYPAD_0, KEY_G, KEY_D, KEY_A,
    // Column 4
    KEY_W, KEYPAD_PLUS, KEY_R, KEY_U,
    KEYPAD_MINUS, KEY_O, KEYPAD_2, KEY_NONE /* right blank */,
    KEY_P, KEY_I, KEY_Y, KEYPAD_3,
    KEYPAD_1, KEY_T, KEY_E, KEY_Q,
    // Column 5
    KEY_2, KEYPAD_SLASH, KEY_4, KEY_7,
    KEY_EQUAL /* keypad equal */, KEY_9, KEYPAD_5, KEY_TILDE,
    KEY_0, KEY_8, KEY_6, KEYPAD_6,
    KEYPAD_4, KEY_5, KEY_3, KEY_1,
    // Column 6
    KEY_NONE /* left blank */, KEYPAD_ASTERIX, KEY_LEFT_SHIFT, KEY_NONE,
    KEY_NUM_LOCK, KEY_NONE, KEYPAD_8, KEY_NONE,
    KEY_NONE, KEY_NONE, KEY_NONE, KEYPAD_9,
    KEYPAD_7, KEY_NONE, KEY_CAPS_LOCK, KEY_ESC,
    // Column 7
    KEY_F1, KEY_F9, KEY_F3, KEY_F11,
    KEY_F5, KEY_PRINTSCREEN, KEY_F7, KEY_PAUSE,
    KEY_SCROLL_LOCK, KEY_F12, KEY_F10, KEY_F8,
    KEY_F6, KEY_F4, KEY_F2, KEY_NONE /* Qwerty/Maltron */,
};
static const uint8_t PROGMEM maltron89_qwerty_modifier_table[8 * 16] = {
    // Column 0
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 1
    0, 0, 0, 0, 0, MOD_RIGHT_ALT, 0, 0,
    MOD_RIGHT_GUI, 0, 0, 0, 0, 0, 0, MOD_LEFT_GUI,
    // Column 2
    0, MOD_RIGHT_CTRL, 0, 0, MOD_LEFT_CTRL, 0, 0, MOD_RIGHT_SHIFT,
    0, 0, 0, MOD_RIGHT_ALT, MOD_LEFT_ALT, 0, 0, 0,
    // Column 3
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 4
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 5
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 6
    0, 0, MOD_LEFT_SHIFT, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 7
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

/*
 * My preferred layout.
 * This is pretty similar to the Maltron 89-series qwerty layout,
 * but has some frequently used keys swapped to the thumbs.
 *
 * In particular, escape, tab, and the OS key are all mapped to more convenient
 * locations.  Caps Lock is turned into a control key.
 */
static const uint8_t PROGMEM simpkins_key_table[8 * 16] = {
    // Column 0
    KEY_NONE, KEY_TAB, KEY_NONE, KEY_NONE,
    KEY_LEFT_GUI, KEY_PAGE_DOWN, KEY_PAGE_UP, KEY_NONE,
    KEY_NONE, KEY_SLASH, KEY_SPACE, KEY_DOWN,
    KEY_UP, KEY_ENTER, KEY_BACKSLASH, KEY_NONE,
    // Column 1
    KEY_NONE, KEY_ENTER /* right thumb return */, KEY_MINUS, KEY_EQUAL,
    KEY_ESC, KEY_RIGHT_ALT /* AltGr */, KEY_MENU, KEY_NONE,
    KEY_END, KEY_RIGHT_BRACE, KEY_DELETE, KEY_RIGHT,
    KEY_LEFT, KEY_BACKSPACE, KEY_LEFT_BRACE, KEY_HOME,
    // Column 2
    KEY_X, KEY_RIGHT_CTRL, KEY_V, KEY_M,
    KEY_LEFT_CTRL, KEY_PERIOD, KEY_NONE, KEY_RIGHT_SHIFT,
    KEY_QUOTE, KEY_COMMA, KEY_N, KEY_RIGHT_ALT,
    KEY_LEFT_ALT, KEY_B, KEY_C, KEY_Z,
    // Column 3
    KEY_S, KEYPAD_PERIOD, KEY_F, KEY_J,
    KEYPAD_ENTER, KEY_L, KEY_INSERT, KEY_CAPS_LOCK /* right caps lock */,
    KEY_SEMICOLON, KEY_K, KEY_H, KEY_BACKSPACE /* keypad backspace */,
    KEYPAD_0, KEY_G, KEY_D, KEY_A,
    // Column 4
    KEY_W, KEYPAD_PLUS, KEY_R, KEY_U,
    KEYPAD_MINUS, KEY_O, KEYPAD_2, KEY_RIGHT_GUI,
    KEY_P, KEY_I, KEY_Y, KEYPAD_3,
    KEYPAD_1, KEY_T, KEY_E, KEY_Q,
    // Column 5
    KEY_2, KEYPAD_SLASH, KEY_4, KEY_7,
    KEY_EQUAL /* keypad equal */, KEY_9, KEYPAD_5, KEY_TILDE,
    KEY_0, KEY_8, KEY_6, KEYPAD_6,
    KEYPAD_4, KEY_5, KEY_3, KEY_1,
    // Column 6
    KEY_LEFT_GUI, KEYPAD_ASTERIX, KEY_LEFT_SHIFT, KEY_NONE,
    KEY_NUM_LOCK, KEY_NONE, KEYPAD_8, KEY_NONE,
    KEY_NONE, KEY_NONE, KEY_NONE, KEYPAD_9,
    KEYPAD_7, KEY_NONE, KEY_LEFT_CTRL, KEY_ESC,
    // Column 7
    KEY_F1, KEY_F9, KEY_F3, KEY_F11,
    KEY_F5, KEY_PRINTSCREEN, KEY_F7, KEY_PAUSE,
    KEY_SCROLL_LOCK, KEY_F12, KEY_F10, KEY_F8,
    KEY_F6, KEY_F4, KEY_F2, KEY_NONE /* Qwerty/Maltron */,
};
static const uint8_t PROGMEM simpkins_modifier_table[8 * 16] = {
    // Column 0
    0, 0, 0, 0, MOD_LEFT_GUI, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 1
    0, 0, 0, 0, 0, MOD_RIGHT_ALT, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 2
    0, MOD_RIGHT_CTRL, 0, 0, MOD_LEFT_CTRL, 0, 0, MOD_RIGHT_SHIFT,
    0, 0, 0, MOD_RIGHT_ALT, MOD_LEFT_ALT, 0, 0, 0,
    // Column 3
    0, 0, 0, 0, 0, 0, 0, MOD_RIGHT_CTRL,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 4
    0, 0, 0, 0, 0, 0, 0, MOD_RIGHT_GUI,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 5
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // Column 6
    MOD_LEFT_GUI, 0, MOD_LEFT_SHIFT, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, MOD_LEFT_CTRL, 0,
    // Column 7
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

void
KeyboardV1::prepareLoop() {
    // Set LEDs to output, high (which is off for my LEDs)
    DDRD = 0xff;
    PORTD = 0xff;

    // Set columns to output, low
    DDRF = 0xff;
    PORTF = 0x00;

    // Set rows to input, with pull-up resistors
    DDRB = 0x00;
    PORTB = 0xff;
    DDRC = 0x00;
    PORTC = 0xff;

    _numCols = 16;
    _keyTable = simpkins_key_table;
    _modifierTable = simpkins_modifier_table;
}

void
KeyboardV1::scanImpl() {
    for (uint8_t col = 0; col < 8; ++col) {
        // Turn the specified column pin into an output pin, and bring it low.
        //
        // It is important that the other column pins are configured as inputs.
        // If multiple keys are pressed down, they may create a circuit
        // connecting to other column pins as well.  We don't want an output
        // voltage on these pins to affect our reading on the column currently
        // being scanned.
        DDRF = 0x01 << col;
        PORTF = 0xff & ~(0x01 << col);

        // We need a very brief delay here to allow the signal
        // to propagate to the input pins.
        _delay_us(5);

        // Read which row pins are active
        uint8_t b = ~PINB;
        uint8_t c = ~PINC;

        if (UNLIKELY(b)) {
            for (uint8_t n = 0; n < 8; ++n) {
                if (b & (0x1 << n)) {
                    keyPressed(n, col);
                }
            }
        }
        if (UNLIKELY(c)) {
            for (uint8_t n = 0; n < 8; ++n) {
                if (c & (0x1 << n)) {
                    keyPressed(n + 8, col);
                }
            }
        }
    }
    DDRF = 0x00;
    PORTF = 0xff;
}
