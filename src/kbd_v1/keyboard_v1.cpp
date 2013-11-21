// Copyright (c) 2013, Adam Simpkins
#include "keyboard_v1.h"

#include <avrpp/log.h>
#include <avrpp/progmem.h>
#include <avrpp/usb_hid_keyboard.h>
#include <avrpp/util.h>

#include <util/delay.h>

F_LOG_LEVEL(1);
#include <avrpp/kbd/KbdDiodeImpl-defs.h>

static const uint8_t PROGMEM default_key_table[16 * 8] = {
    // Row 0
    KEY_NONE, KEY_NONE, KEY_X, KEY_S,
    KEY_W, KEY_2, KEY_LEFT_GUI, KEY_F1,
    // Row 1
    KEY_TAB, KEY_ENTER /* right thumb return */, KEY_RIGHT_ALT, KEYPAD_PERIOD,
    KEYPAD_PLUS, KEYPAD_SLASH, KEYPAD_ASTERIX, KEY_F9,
    // Row 2
    KEY_NONE, KEY_MINUS, KEY_V, KEY_F,
    KEY_R, KEY_4, KEY_LEFT_SHIFT, KEY_F3,
    // Row 3
    KEY_NONE, KEY_EQUAL, KEY_M, KEY_J,
    KEY_U, KEY_7, KEY_NONE, KEY_F11,
    // Row 4
    KEY_LEFT_GUI, KEY_ESC, KEY_LEFT_ALT, KEYPAD_ENTER,
    KEYPAD_MINUS, KEY_EQUAL /* keypad equal */, KEY_NUM_LOCK, KEY_F5,
    // Row 5
    KEY_PAGE_DOWN, KEY_RIGHT_ALT /* AltGr */, KEY_PERIOD, KEY_L,
    KEY_O, KEY_9, KEY_NONE, KEY_PRINTSCREEN,
    // Row 6
    KEY_PAGE_UP, KEY_MENU, KEY_NONE, KEY_INSERT,
    KEYPAD_2, KEYPAD_5, KEYPAD_8, KEY_F7,
    // Row 7
    KEY_NONE, KEY_NONE, KEY_RIGHT_SHIFT, KEY_RIGHT_CTRL,
    KEY_RIGHT_GUI, KEY_TILDE, KEY_NONE, KEY_PAUSE,
    // Row 8
    KEY_NONE, KEY_END, KEY_QUOTE, KEY_SEMICOLON,
    KEY_P, KEY_0, KEY_NONE, KEY_SCROLL_LOCK,
    // Row 9
    KEY_SLASH, KEY_RIGHT_BRACE, KEY_COMMA, KEY_K,
    KEY_I, KEY_8, KEY_NONE, KEY_F12,
    // Row 10
    KEY_SPACE, KEY_DELETE, KEY_N, KEY_H,
    KEY_Y, KEY_6, KEY_NONE, KEY_F10,
    // Row 11
    KEY_DOWN, KEY_RIGHT, KEY_RIGHT_CTRL, KEY_BACKSPACE /* keypad backspace */,
    KEYPAD_3, KEYPAD_6, KEYPAD_9, KEY_F8,
    // Row 12
    KEY_UP, KEY_LEFT, KEY_LEFT_CTRL, KEYPAD_0,
    KEYPAD_1, KEYPAD_4, KEYPAD_7, KEY_F6,
    // Row 13
    KEY_ENTER, KEY_BACKSPACE, KEY_B, KEY_G,
    KEY_T, KEY_5, KEY_NONE, KEY_F4,
    // Row 14
    KEY_BACKSLASH, KEY_LEFT_BRACE, KEY_C, KEY_D,
    KEY_E, KEY_3, KEY_LEFT_CTRL, KEY_F2,
    // Row 15
    KEY_NONE, KEY_HOME, KEY_Z, KEY_A,
    KEY_Q, KEY_1, KEY_ESC, KEY_CAPS_LOCK /* Qwerty/Maltron */,
};
static const uint8_t PROGMEM default_modifier_table[16 * 8] = {
    0, 0, 0, 0, 0, 0, MOD_LEFT_GUI, 0,  // Row 0
    0, 0, MOD_RIGHT_ALT, 0, 0, 0, 0, 0,  // Row 1
    0, 0, 0, 0, 0, 0, MOD_LEFT_SHIFT, 0,  // Row 2
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 3
    MOD_LEFT_GUI, 0, MOD_LEFT_ALT, 0, 0, 0, 0, 0,  // Row 4
    0, MOD_RIGHT_ALT, 0, 0, 0, 0, 0, 0,  // Row 5
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 6
    0, 0, MOD_RIGHT_SHIFT, MOD_RIGHT_CTRL, MOD_RIGHT_GUI, 0, 0, 0,  // Row 7
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 8
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 9
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 10
    0, 0, MOD_RIGHT_CTRL, 0, 0, 0, 0, 0,  // Row 11
    0, 0, MOD_LEFT_CTRL, 0, 0, 0, 0, 0,  // Row 12
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 13
    0, 0, 0, 0, 0, 0, MOD_LEFT_CTRL, 0,  // Row 14
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 15
};


KeyboardV1::KeyboardV1() {
    _keyTable = pgm_cast(default_key_table);
    _modifierTable = pgm_cast(default_modifier_table);

    // There are diodes installed on the left and right shift and control keys,
    // as well as the control and alt thumb keys.
    _diodes.set(getIndex(6, 2)); // Left shift
    _diodes.set(getIndex(2, 7)); // Right shift
    _diodes.set(getIndex(6, 14)); // Left ctrl
    _diodes.set(getIndex(3, 7)); // Right ctrl
    _diodes.set(getIndex(2, 4)); // Left thumb ctrl
    _diodes.set(getIndex(2, 1)); // Right thumb ctrl
    _diodes.set(getIndex(3, 12)); // Left thumb alt
    _diodes.set(getIndex(2, 11)); // Right thumb alt
}

void
KeyboardV1::prepare() {
    // Set columns to output, low
    DDRF = 0xff;
    PORTF = 0x00;

    // Set rows to input, with pull-up resistors
    DDRB = 0x00;
    PORTB = 0xff;
    DDRC = 0x00;
    PORTC = 0xff;
}

void
KeyboardV1::prepareColScan(uint8_t col) {
    // Set the specified column to low output, with all other
    // columns as pull-up input.
    //
    // We assume that all rows are already configured as pull-up inputs.
    // (After each call to prepareRowScan(), finishRowScan() is always called
    // before subsequent prepareColScan() calls.)
    DDRF = 0x01 << col;
    PORTF = 0xff & ~(0x01 << col);
}

void
KeyboardV1::prepareRowScan(uint8_t row) {
    // Set the specified row to low output, with all other rows and columns
    // as pull-up input.
    DDRF = 0x00;
    PORTF = 0xff;
    if (row < 8) {
        uint8_t bit = row;
        DDRB = 0x01 << bit;
        PORTB = 0xff & ~(0x01 << bit);
    } else if (row < 16) {
        uint8_t bit = row - 8;
        DDRC = 0x01 << bit;
        PORTC = 0xff & ~(0x01 << bit);
    }
}

void
KeyboardV1::finishRowScan() {
    // Reset all rows to pull-up input.
    DDRB = 0x00;
    PORTB = 0xff;
    DDRC = 0x00;
    PORTC = 0xff;
}

void
KeyboardV1::readRows(RowMap *rows) {
    rows->bytes[0] = ~PINB;
    rows->bytes[1] = ~PINC;
}

void
KeyboardV1::readCols(ColMap *cols) {
    cols->bytes[0] = ~PINF;
}
