// Copyright (c) 2013, Adam Simpkins
#include "keyboard_v2.h"

#include <avrpp/kbd_v2/usb_config.h>

#include <avrpp/avr_registers.h>
#include <avrpp/log.h>
#include <avrpp/dbg_endpoint.h>
#include <avrpp/kbd_endpoint.h>
#include <avrpp/usb.h>
#include <avrpp/usb_descriptors.h>
#include <avrpp/usb_hid_keyboard.h>
#include <avrpp/util.h>

#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

F_LOG_LEVEL(2);

class LedController {
  public:
    LedController() {
        // The LEDs are C0 through C4.
        // Start with the power and error LEDs on, and all others off.
        // (The error LED will be cleared when USB is configured.)
        DDRC = 0x3f;
        PORTC = 0xe7;
    }

    void setPowerLED() {
        PORTC &= ~0x08;
    }
    void clearPowerLED() {
        PORTC |= 0x08;
    }
    void setErrorLED() {
        PORTC &= ~0x10;
    }
    void clearErrorLED() {
        PORTC |= 0x10;
    }

    void setKeyboardLEDs(uint8_t led_value) {
        uint8_t new_pins = PORTC;
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
        PORTC = new_pins;
    }

  private:
    enum : uint8_t{
        LED_MASK = 0x1f,
        PIN_NUM_LOCK = 0x01,
        PIN_CAPS_LOCK = 0x02,
        PIN_SCROLL_LOCK = 0x04,
    };
};

class Controller : private Keyboard::Callback,
                   private KeyboardIface::LedCallback,
                   private UsbController::StateCallback {
  public:
    Controller(Keyboard *kbd, LedController *leds)
        : _kbd(kbd),
          _leds(leds) {
#if USB_DEBUG
        set_log_putchar(DebugIface::putcharC, &_dbgIface);
#endif
    }

    void init() {
        FLOG(2, "Booting\n");

        // TODO: Apply other power saving settings as recommended by
        // the at90usb1286 manual.
        // The HalfKay bootloader does seem to leave some features enabled that
        // consume power.  We draw a couple mA more when started via a HalfKay
        // reset than we do when restarted from a plain power on.
        //
        // PRR0 and PRR1 control some power reduction settings.

        // Initialize USB
        auto usb = UsbController::singleton();
        usb->setStateCallback(this);
        _kbdIface.setLedCallback(this);
        usb->addInterface(&_kbdIface);
#if USB_DEBUG
        usb->addInterface(&_dbgIface);
#endif
        usb->init(ENDPOINT0_SIZE, pgm_cast(usb_descriptors));
        // Enable interrupts
        sei();
        // Wait for USB configuration with the host to complete
        waitForUsbInit(usb);
    }

    void loop() {
        _kbd->loop(this);
    }

  private:
    void waitForUsbInit(UsbController *usb) {
        while (!usb->configured()) {
            _delay_ms(1);
        }
    }

    // USB state changes
    virtual void onConfigured() override {
        _leds->clearErrorLED();
    }
    virtual void onUnconfigured() override {
        _leds->setErrorLED();
    }
    virtual void onSuspend() override {
        _leds->clearPowerLED();
    }
    virtual void onWake() override {
        _leds->setPowerLED();
    }

    virtual void updateLeds(uint8_t led_value) {
        _leds->setKeyboardLEDs(led_value);
    }

    virtual void onChange(Keyboard* kbd) override {
        uint8_t keys_len = KeyboardIface::MAX_KEYS;
        uint8_t pressed_keys[KeyboardIface::MAX_KEYS]{0};
        uint8_t modifier_mask{0};

        kbd->getState(&modifier_mask, pressed_keys, &keys_len);
        _kbdIface.update(pressed_keys, modifier_mask);
    }

    Keyboard *_kbd{nullptr};
    LedController *_leds;
    KeyboardIface _kbdIface{KEYBOARD_INTERFACE, KEYBOARD_ENDPOINT};
#if USB_DEBUG
    DebugIface _dbgIface{DEBUG_INTERFACE, DEBUG_ENDPOINT, 4096, DEBUG_SIZE};
#endif
};

int main() {
    // Set the clock speed as the first thing we do
    set_cpu_prescale();

    KeyboardV2 kbd;
    LedController leds;
    Controller controller(&kbd, &leds);
    controller.init();
    FLOG(1, "Keyboard initialized\n");
    controller.loop();
}
