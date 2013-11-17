// Copyright (c) 2013, Adam Simpkins
#include <avrpp/avr_registers.h>
#include <avrpp/log.h>

#include <avr/wdt.h>
#include <stdlib.h>

void set_cpu_prescale() {
    static_assert(F_CPU * (1 << CPU_PRESCALE) == 16000000,
                  "F_CPU and CPU_PRESCALE definitions do not match");

    // Set the CLKPR register to 0x80 to enable changing the CPU scaling
    CLKPR = 0x80;
    // Now set it to a value with the high bit clear, to change the actual
    // scaling.
    CLKPR = CPU_PRESCALE;
}

// Disable the watchdog very early during startup.
// This prevents us resetting in a loop after a watchdog system reset
void disable_watchdog()
    __attribute__((naked))
    __attribute__((section(".init3")));
void disable_watchdog() {
    // We could save MCUSR here for main() to examine later
    // if it wanted to look at the reset reason.
    MCUSR = 0;
    wdt_disable();
}

// Reset on a pure virtual function call
extern "C" void __cxa_pure_virtual() {
    // Enable the watchdog timer and then wait
    // for it to expire
    cli();
    wdt_reset();
    MCUSR &= ~(1 << WDRF);
    wdt_enable(WDTO_15MS);
}

void operator delete(void* p) {
    free(p);
}
