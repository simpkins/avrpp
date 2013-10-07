// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avr/sfr_defs.h>
#include <stdint.h>

/*
 * A template class for representing a single I/O pin.
 *
 * This makes it possible to write code with configurable pin assignments,
 * but the pin assignment can be selected at compile time with minimal run-time
 * overhead.
 *
 * However, note that if you are compiling with -Os, gcc ususally won't inline
 * all of these calls, so this isn't quite as efficient as hard-coding pin
 * assignments directly into your code.  However, with -O/-O2/-O3 it generally
 * will inline everything, at least in my experience.
 */
template<typename PinTraits, uint8_t PIN>
class IoPin {
  public:
    IoPin() { static_assert(PIN < 8, "invalid pin number"); }
    bool read() const {
        return (_pin.pin() & (1 << PIN));
    }
    void set() {
        _pin.port() |= (1 << PIN);
    }
    void set(bool value) {
        if (value) {
            set();
        } else {
            clear();
        }
    }
    void clear() {
        _pin.port() &= ~(1 << PIN);
    }

    void setInput() {
        _pin.direction() &= ~(1 << PIN);
    }
    void setInput(bool pullUp) {
        setInput();
        set(pullUp);
    }
    void setOutput() {
        _pin.direction() |= (1 << PIN);
    }
    void setOutput(bool value) {
        setOutput();
        set(value);
    }

  private:
    PinTraits _pin;
};

template<uint8_t PinAddr, uint8_t DdrAddr, uint8_t PortAddr>
class PinTraits {
  public:
    constexpr volatile uint8_t& pin() {
        return _SFR_IO8(PinAddr);
    }
    constexpr volatile uint8_t& direction() {
        return _SFR_IO8(DdrAddr);
    }
    constexpr volatile uint8_t& port() {
        return _SFR_IO8(PortAddr);
    }
};

#if defined __AVR_AT90USB1286__ || \
    defined __AVR_AT90USB1287__ || \
    defined _AVR_AT90USB646_H_
typedef PinTraits<0x00, 0x01, 0x02> PinA;
typedef PinTraits<0x03, 0x04, 0x05> PinB;
typedef PinTraits<0x06, 0x07, 0x08> PinC;
typedef PinTraits<0x09, 0x0a, 0x0b> PinD;
typedef PinTraits<0x0c, 0x0d, 0x0e> PinE;
typedef PinTraits<0x0f, 0x10, 0x11> PinF;
#else
#error "Pin definitions not provided for this platform yet."
#endif
