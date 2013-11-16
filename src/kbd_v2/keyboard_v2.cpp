// Copyright (c) 2013, Adam Simpkins
#include "keyboard_v2.h"

#include <avrpp/log.h>
#include <avrpp/progmem.h>
#include <avrpp/usb_hid_keyboard.h>
#include <avrpp/util.h>

#include <util/delay.h>

F_LOG_LEVEL(1);
#include <avrpp/kbd/KbdDiodeImpl-defs.h>

static const uint8_t PROGMEM default_key_table[18 * 8] = {
    // Row 0
    KEY_TAB, KEY_NONE, KEY_HOME, KEY_BACKSLASH,
    KEY_LEFT_BRACE, KEY_MINUS, KEY_BACKSPACE, KEY_LEFT_GUI,
    // Row 1
    KEY_RIGHT, KEY_N, KEY_M, KEY_COMMA,
    KEY_PERIOD, KEY_QUOTE, KEY_NONE, KEY_NONE,
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
    KEY_NONE, KEY_NONE, KEY_Z, KEY_X,
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
    KEY_RIGHT_GUI, KEY_LEFT_SHIFT, KEY_NONE, KEY_NONE,
    KEY_MUTE, KEY_NONE, KEY_RIGHT_SHIFT, KEY_LEFT,
};
static const uint8_t PROGMEM default_modifier_table[18 * 8] = {
    0, 0, 0, 0, 0, 0, 0, MOD_LEFT_GUI,  // Row 0
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 1
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
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 13
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 14
    0, 0, 0, 0, 0, 0, 0, 0,  // Row 15
    0, 0, 0, MOD_LEFT_ALT, 0, 0, 0, MOD_RIGHT_ALT,  // Row 16
    MOD_RIGHT_GUI, MOD_LEFT_SHIFT, 0, 0, 0, 0, MOD_RIGHT_SHIFT, 0,  // Row 17
};

KeyboardV2::KeyboardV2() {
    _keyTable = pgm_cast(default_key_table);
    _modifierTable = pgm_cast(default_modifier_table);

    // There are diodes installed on the left and right shift keys.
    _diodes.set(getIndex(1, 17));
    _diodes.set(getIndex(6, 17));
}

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
}

void
KeyboardV2::prepareColScan(uint8_t col) {
    // Set the specified column to low output, with all other
    // columns as pull-up input.
    //
    // We assume that all rows are already configured as pull-up inputs.
    // (After each call to prepareRowScan(), finishRowScan() is always called
    // before subsequent prepareColScan() calls.)
    DDRB = 0x01 << col;
    PORTB = 0xff & ~(0x01 << col);
}

void
KeyboardV2::prepareRowScan(uint8_t row) {
    // Set the specified row to low output, with all other rows and columns
    // as pull-up input.
    DDRB = 0x00;
    PORTB = 0xff;
    if (row < 8) {
        uint8_t bit = row;
        DDRD = 0x01 << bit;
        PORTD = 0xff & ~(0x01 << bit);
    } else if (row < 16) {
        uint8_t bit = row - 8;
        DDRF = 0x01 << bit;
        PORTF = 0xff & ~(0x01 << bit);
    } else {
        uint8_t bit = row - 10;
        DDRE = 0x01 << bit;
        PORTE = 0xc0 & ~(0x01 << bit);
    }
}

void
KeyboardV2::finishRowScan() {
    // Reset all rows to pull-up input.
    DDRD = 0x00;
    PORTD = 0xff;
    DDRF = 0x00;
    PORTF = 0xff;
    DDRE = 0x00;
    PORTE = 0xc0;
}

void
KeyboardV2::readRows(RowMap *rows) {
    rows->bytes[0] = ~PIND;
    rows->bytes[1] = ~PINF;
    rows->bytes[2] = (static_cast<uint8_t>(~PINE) >> 6);
}

void
KeyboardV2::readCols(ColMap *cols) {
    cols->bytes[0] = ~PINB;
}
