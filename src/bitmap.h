// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <stdint.h>

/*
 * A fixed-size bitmap.
 */
template<uint8_t SIZE>
struct Bitmap {
    bool operator[](uint8_t idx) const {
        return get(idx);
    }

    bool get(uint8_t idx) const {
        return bytes[idx >> 3] & (1 << (idx & 0x7));
    }

    void set(uint8_t idx) {
        bytes[idx >> 3] |= (1 << (idx & 0x7));
    }
    void unset(uint8_t idx) {
        bytes[idx >> 3] &= ~(1 << (idx & 0x7));
    }
    void set(uint8_t idx, bool value) {
        if (value) {
            set(idx);
        } else {
            unset(idx);
        }
    }

    void clear() {
        const uint8_t end = 1 + (SIZE >> 3);
        for (uint8_t n = 0; n < end; ++n) {
            bytes[n] = 0;
        }
    }

    bool operator==(const Bitmap& other) const {
        return memcmp(&bytes, &other.bytes, NUM_BYTES) == 0;
    }
    bool operator!=(const Bitmap& other) const {
        return !(*this == other);
    }

    enum { NUM_BYTES = 1 + (SIZE >> 3) };
    uint8_t bytes[NUM_BYTES]{0};
};
