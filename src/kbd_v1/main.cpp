// Copyright (c) 2013, Adam Simpkins
#include "keyboard_v1.h"

#include <avrpp/avr_registers.h>
#include <avrpp/log.h>
#include <avrpp/dbg_endpoint.h>
#include <avrpp/kbd_endpoint.h>
#include <avrpp/usb.h>
#include <avrpp/usb_descriptors.h>
#include <avrpp/usb_hid_keyboard.h>
#include <avrpp/util.h>

#include <avrpp/kbd_v1/usb_config.h>

#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

F_LOG_LEVEL(2);

void wait_for_usb_init(UsbController *usb) {
    // Cycle the LEDs through a pattern while we wait
    //
    // Note that I unfortunately wired up the LEDs in a slightly odd fashion.
    // They are in the following physical placement, from left to right:
    //   0x10, 0x01, 0x02, 0x04, 0x08
#if 1
    // Left to right, then back
    static const uint8_t PROGMEM pattern[] = {
        0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01,
    };
#elif 0
    // Outside to in, then back
    static const uint8_t PROGMEM pattern[] = {
        0x18, 0x05, 0x02, 0x05,
    };
#else
    // A pair moving left to right.
    static const uint8_t PROGMEM pattern[] = {
        0x10, 0x11, 0x03, 0x06, 0x0c, 0x08
    };
#endif

    uint8_t pattern_idx = 0;
    uint8_t n = 0;
    while (!usb->configured()) {
        _delay_ms(1);
        ++n;
        if (n == 100) {
            n = 0;
            ++pattern_idx;
            if (pattern_idx >= sizeof(pattern)) {
                pattern_idx = 0;
            }
            PORTD = pgm_read_byte(pattern + pattern_idx);
        }
    }

    // Clear all the LEDs when we are done.
    PORTD = 0;
}

class LedCallback : public KeyboardIface::LedCallback {
  public:
    virtual void updateLeds(uint8_t led_value) {
        _leds = led_value;

        enum : uint8_t{
            PIN_NUM_LOCK = 0x04,
            PIN_CAPS_LOCK = 0x08,
            PIN_SCROLL_LOCK = 0x04,
        };
        uint8_t new_pins = PORTD;
        if (led_value & LED_NUM_LOCK) {
            new_pins &= ~PIN_NUM_LOCK;
        } else {
            new_pins |= PIN_NUM_LOCK;
        }
        if (led_value & LED_CAPS_LOCK) {
            new_pins &= ~PIN_CAPS_LOCK;
        } else {
            new_pins |= PIN_CAPS_LOCK;
        }
        if (led_value & LED_SCROLL_LOCK) {
            new_pins &= ~PIN_SCROLL_LOCK;
        } else {
            new_pins |= PIN_SCROLL_LOCK;
        }
        PORTD = new_pins;
    }

    void refresh() {
        AtomicGuard ag;
        updateLeds(_leds);
    }

  private:
    uint8_t _leds{0};
};

class KeyboardCallback : public Keyboard::Callback {
  public:
    KeyboardCallback(KeyboardIface *iface) : _interface(iface) {}

    virtual void onChange(Keyboard* kbd) override {
        uint8_t keys_len = KeyboardIface::MAX_KEYS;
        uint8_t pressed_keys[KeyboardIface::MAX_KEYS]{0};
        uint8_t modifier_mask{0};

        kbd->getState(&modifier_mask, pressed_keys, &keys_len);
        _interface->update(pressed_keys, modifier_mask);
    }

  private:
    KeyboardIface *_interface;
};

int main() {
    // Set the clock speed as the first thing we do
    set_cpu_prescale();

    KeyboardIface kbd_if(KEYBOARD_INTERFACE, KEYBOARD_ENDPOINT);
    DebugIface dbg_if(DEBUG_INTERFACE, DEBUG_ENDPOINT, 4096, DEBUG_SIZE);
    set_log_putchar(DebugIface::putcharC, &dbg_if);
    FLOG(2, "Booting\n");

    // The LEDs are hooked up to the D pins, so specify them as output.
    DDRD = 0xff;
    // Turn all LEDs on during initialization
    PORTD = 0xff;

    // Configure all B and C pins as input, using pull-up resistors
    DDRB = 0x00;
    PORTB = 0xff;
    DDRC = 0x00;
    PORTC = 0xff;

    // The F pins are the rows, and we scan over them, setting them
    // to output as we scan.  However, configure them for input initially.
    DDRF = 0x00;
    PORTF = 0xff;

    KeyboardV1 kbd;
    KeyboardCallback kbd_callback(&kbd_if);

    // TODO: Apply other power saving settings as recommended by
    // the at90usb1286 manual.
    // The HalfKay bootloader does seem to leave some features enabled that
    // consume power.  We draw a couple mA more when started via a HalfKay
    // reset than we do when restarted from a plain power on.
    //
    // PRR0 and PRR1 control some power reduction settings.

    // Initialize USB
    auto usb = UsbController::singleton();
    LedCallback led_callback;
    kbd_if.setLedCallback(&led_callback);
    usb->addInterface(&kbd_if);
    usb->addInterface(&dbg_if);
    usb->init(ENDPOINT0_SIZE, pgm_cast(usb_descriptors));
    // Enable interrupts
    sei();
    // Wait for USB configuration with the host to complete
    wait_for_usb_init(usb);

    // wait_for_usb_init() modifies the LEDs, so reset them back
    // to the desired state.
    led_callback.refresh();

    FLOG(1, "Keyboard initialized\n");
    kbd.loop(&kbd_callback);
}
