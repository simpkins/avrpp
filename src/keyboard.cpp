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

static uint8_t f_log_level = 1;

void
Keyboard::prepareLoop() {
  // subclasses should override prepareLoop() to perform any setup required.
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
    _newModifierMask = 0;
    memset(_newPressedKeys, 0, sizeof(_newPressedKeys));
    _pressedIndex = 0;
    _changed = false;

    scanImpl();

    if (!_changed) {
        if (_pressedIndex < MAX_KEYS &&
            _newPressedKeys[_pressedIndex] != _pressedKeys[_pressedIndex]) {
            _changed = true;
        }
        _changed |= (_newModifierMask != _modifierMask);
    }
    if (LIKELY(!_changed)) {
        return;
    }

    FLOG(2, "Keys=[%#x %#x %#x %#x %#x %#x], Mod=%#04x\n",
         _newPressedKeys[0], _newPressedKeys[1], _newPressedKeys[2],
         _newPressedKeys[3], _newPressedKeys[4], _newPressedKeys[5],
         _newModifierMask);

    memcpy(_pressedKeys, _newPressedKeys, sizeof(_pressedKeys));
    _modifierMask = _newModifierMask;

    if (_callback) {
        _callback->onChange(this);
    }
}

void
Keyboard::keyPressed(uint8_t col, uint8_t row) {
    auto idx = (row * _numCols) + col;
    uint8_t key_code = pgm_read_byte(_keyTable + idx);
    uint8_t modifier = pgm_read_byte(_modifierTable + idx);
    FLOG(4, "pressed: %#x x %#x (code=%d, modifier=%#x)\n",
         col, row, key_code, modifier);

    _newModifierMask |= modifier;
    if (UNLIKELY(_pressedIndex >= MAX_KEYS)) {
        FLOG(2, "pressed exceeds max: %#x x %#x\n", col, row);
        return;
    }
    if (_pressedKeys[_pressedIndex] != key_code) {
        _changed = true;
    }
    _newPressedKeys[_pressedIndex] = key_code;
    ++_pressedIndex;
}
