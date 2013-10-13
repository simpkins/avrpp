/*
 * Copyright (c) 2013, Adam Simpkins
 */
#include "keyboard.h"

#include "log.h"
#include "usb.h"
#include "usb_hid_keyboard.h"
#include "kbd_endpoint.h"
#include "util.h"

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

static uint8_t f_log_level = 1;

Keyboard::~Keyboard() {
    free(_detectedKeys);
}

void
Keyboard::prepareLoop() {
  // subclasses should override prepareLoop() to perform any setup required.
}

void
Keyboard::init(uint8_t num_cols,
               uint8_t num_rows,
               const uint8_t* key_table,
               const uint8_t* mod_table) {
    _numCols = num_cols;
    _numRows = num_rows;
    _keyTable = key_table;
    _modifierTable = mod_table;

    // Allocate one contiguous chunk of memory for _detectedKeys,
    // _reportedKeys, _prevKeys, and _diodes.
    //
    // TODO: We could do a better job of organizing this data.
    // These arrays each just store booleans, so we only need 1 bit per key
    // rather than 1 byte per key.  We have a couple of options:
    //
    // - Use a single array of one byte per key, and treat each as a set of
    //   flags with one bit for detected, reported, prev, etc.
    //   The disadvantage here is that some of our memcpy operations would be
    //   more awkward to compute.
    //
    // - Use a packed bitmask for each array.  Both of my current keyboards
    //   have exactly 8 columns, so this would actually pack very nicely into
    //   one byte per column.  However, the code would be more complicated if
    //   we wanted to support keyboards where the number of columns is not a
    //   multiple of 8.
    uint16_t num_keys = (_numCols * _numRows);
    auto data_size = mapSize() * 4;
    _detectedKeys = static_cast<uint8_t*>(malloc(data_size));
    memset(_detectedKeys, 0, data_size);
    _reportedKeys = _detectedKeys + num_keys;
    _prevKeys = _reportedKeys + num_keys;
    _diodes = _prevKeys + num_keys;
}

void
Keyboard::loop() {
    prepareLoop();

    while (true) {
        scanKeys();
        _delay_ms(2);
    }
}

void
Keyboard::scanKeys() {
    memset(_detectedKeys, 0, mapSize());
    scanImpl();

    // Perform anti-ghosting, and block any key presses that may
    // be ghosting rather than real key presses.
    blockKeys();

    if (_callback && memcmp(_prevKeys, _reportedKeys, mapSize()) != 0) {
        _callback->onChange(this);
    }
}

void
Keyboard::keyPressed(uint8_t col, uint8_t row) {
    FLOG(4, "pressed: %#x x %#x\n", col, row);
    auto idx = getIndex(col, row);
    _detectedKeys[idx] = 1;
}

void
Keyboard::blockKeys() {
    // Swap _prevKeys and _reportedKeys, so that _prevKeys
    // points at the reported keys from the last scan loop.
    auto *tmp = _prevKeys;
    _prevKeys = _reportedKeys;
    _reportedKeys = tmp;
    // Update _reportedKeys with the current detected keys.
    memcpy(_reportedKeys, _detectedKeys, mapSize());

    // Walk through the keys, and look for rectangles where 3 or 4 corners
    // are pressed.  Block newly pressed keys on these rectangles,
    // to prevent ghosting.
    //
    // This algorithm is O(num_keys * num_detected_keys).
    // In the normal case num_detected_keys is fortunately fairly small.
    for (uint8_t col = 0; col < _numCols; ++col) {
        for (uint8_t row = 0; row < _numRows; ++row) {
            auto tl_idx = (row * _numCols) + col;
            if (!_detectedKeys[tl_idx]) {
                continue;
            }

            for (uint8_t other_row = row + 1;
                 other_row < _numRows;
                 ++other_row) {
                auto bl_idx = (other_row * _numCols) + col;
                if (!_detectedKeys[bl_idx]) {
                    continue;
                }
                for (uint8_t other_col = col + 1;
                     other_col < _numCols;
                     ++other_col) {
                    auto tr_idx = (row * _numCols) + other_col;
                    auto br_idx = (other_row * _numCols) + other_col;
                    if (_detectedKeys[tr_idx] || _detectedKeys[br_idx]) {
                        clearRect(col, row, other_col, other_row);
                    }
                }
            }
        }
    }
}

void
Keyboard::clearRect(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2) {
    FLOG(3, "found rect: (%d, %d), (%d, %d)\n", c1, r1, c2, r2);
    clearIf(c1, r1, c2, r2);
    clearIf(c1, r2, c2, r1);
    clearIf(c2, r1, c1, r2);
    clearIf(c2, r2, c1, r1);
}

void
Keyboard::clearIf(uint8_t col, uint8_t row,
                  uint8_t diode_col, uint8_t diode_row) {
    auto idx = getIndex(col, row);
    if (_prevKeys[idx]) {
        // This key was previously reported as down,
        // so assume it is still down, and don't block it.
        return;
    }

    auto diode_idx = getIndex(diode_col, diode_row);
    if (_diodes[diode_idx]) {
        // There is a diode on the opposite corner of the rectangle,
        // protecting this key from ghosting.  Therefore we can be sure
        // that this key is really down.
        return;
    }

    // This key may have been detected due to ghosting.
    // Don't report it as down.
    FLOG(2, "jamming (%d, %d) code=%d\n",
         col, row, pgm_read_byte(_keyTable + idx));
    _reportedKeys[idx] = 0;
}

void
Keyboard::getState(uint8_t *modifiers,
                   uint8_t *keys,
                   uint8_t *keys_len) const {
    *modifiers = 0;

    uint8_t pressed_idx = 0;
    for (uint8_t col = 0; col < _numCols; ++col) {
        for (uint8_t row = 0; row < _numRows; ++row) {
            auto idx = getIndex(col, row);
            if (_reportedKeys[idx]) {
                if (pressed_idx < *keys_len) {
                    keys[pressed_idx] = pgm_read_byte(_keyTable + idx);
                    ++pressed_idx;
                }
                uint8_t modifier = pgm_read_byte(_modifierTable + idx);
                *modifiers |= modifier;
            }
        }
    }

    *keys_len = pressed_idx;
}
