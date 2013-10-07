// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avr/interrupt.h>

/*
 * AtomicGuard ensures that the remaining code in the current scope
 * runs atomically.
 */
class AtomicGuard {
  public:
    AtomicGuard() : oldState_(SREG) {
        cli();
    }
    ~AtomicGuard() {
        SREG = oldState_;
        __asm__ volatile ("" ::: "memory");
    }

    uint8_t getInitialState() const {
        return oldState_;
    }

  private:
    const uint8_t oldState_;
};
