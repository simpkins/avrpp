// Copyright (c) 2013, Adam Simpkins
//
// Utilities for accessing data in program space memory.
//
// It's quite unfortunate that gcc implements named address space support
// for C only, and not for C++.
// http://gcc.gnu.org/onlinedocs/gcc/Named-Address-Spaces.html
#pragma once

#include <avr/pgmspace.h>

static inline char _progmem_get(const char* ptr) {
    return pgm_read_byte(ptr);
}
static inline uint8_t _progmem_get(const uint8_t* ptr) {
    return pgm_read_byte(ptr);
}
static inline int8_t _progmem_get(const int8_t* ptr) {
    return pgm_read_byte(ptr);
}
static inline uint16_t _progmem_get(const uint16_t* ptr) {
    return pgm_read_word(ptr);
}
static inline int16_t _progmem_get(const int16_t* ptr) {
    return pgm_read_word(ptr);
}
static inline uint32_t _progmem_get(const uint32_t* ptr) {
    return pgm_read_dword(ptr);
}
static inline int32_t _progmem_get(const int32_t* ptr) {
    return pgm_read_dword(ptr);
}

/**
 * Template to indicate a pointer to program space memory.
 *
 * The named address space extension (the __flash qualifier) would be much
 * nicer.  However, it isn't available for C++, so this is the best we can do
 * for now.  The main drawback is that this doesn't allow us to easily define
 * string literals in program space.
 */
template<typename T>
struct pgm_ptr {
    explicit pgm_ptr(const T* p) : _value(p) {}
    pgm_ptr& operator++() {
        ++_value;
        return *this;
    }
    pgm_ptr operator++(int) {
        pgm_ptr copy(_value);
        ++_value;
        return copy;
    }

    T operator*() const {
        return _progmem_get(_value);
    }

    const T* value() const {
        return _value;
    }

    pgm_ptr<T> operator+(size_t idx) const {
        return pgm_ptr<T>(_value + idx);
    }

  private:
    const T* _value;
};

template<typename T>
pgm_ptr<T> pgm_cast(const T* p) {
    return pgm_ptr<T>(p);
}

/**
 * A helper macro for defining string literals in program space.
 *
 * This is very similar to avr-libc's PSTR() macro, except that it returns a
 * pgm_ptr<char>, so we can have separate types for pointer to program space.
 * This allows compile-time checking to make sure we aren't accidentally mixing
 * program-space and data-space pointers.
 *
 * The _() macro name was chosen since it is easy to type, and is already
 * commonly used to wrap strings in other projects (for gettext() support).
 */
#define _(s) (pgm_ptr<char>(PSTR(s)))
