// Copyright (c) 2013, Adam Simpkins
#include "keyboard_v1.h"

#include <avrpp/kbd_v1/usb_config.h>

#include <avrpp/avr_registers.h>
#include <avrpp/log.h>
#include <avrpp/kbd/KbdController.h>
#include <avrpp/usb_descriptors.h>
#include <avrpp/usb_hid_keyboard.h>
#include <avrpp/util.h>

F_LOG_LEVEL(2);

class LedController : public KbdController::LedController {
  public:
    LedController() {
        // The LEDs are D0 through D4.
        //
        // Note that I unfortunately wired up the LEDs in a slightly odd
        // fashion.  They are in the following physical placement, from left to
        // right:
        //   0x10, 0x01, 0x02, 0x04, 0x08
        //
        // Start with the power and error LEDs on, and all others off.
        // (The error LED will be cleared when USB is configured.)
        DDRD = 0xff;
        PORTD = (PIN_POWER | PIN_ERROR);
    }

    void setPowerLED() override {
        PORTD |= PIN_POWER;
    }
    void clearPowerLED() override {
        PORTD &= ~PIN_POWER;
    }
    void setErrorLED() override {
        PORTD |= PIN_ERROR;
    }
    void clearErrorLED() override {
        PORTD &= ~PIN_ERROR;
    }

    void setKeyboardLEDs(uint8_t led_value) override {
        uint8_t new_pins = PORTD;
        if (led_value & LED_NUM_LOCK) {
            new_pins |= PIN_NUM_LOCK;
        } else {
            new_pins &= ~PIN_NUM_LOCK;
        }
        if (led_value & LED_CAPS_LOCK) {
            new_pins |= PIN_CAPS_LOCK;
        } else {
            new_pins &= ~PIN_CAPS_LOCK;
        }
        if (led_value & LED_SCROLL_LOCK) {
            new_pins |= PIN_SCROLL_LOCK;
        } else {
            new_pins &= ~PIN_SCROLL_LOCK;
        }
        PORTD = new_pins;
    }

    // Turn off all LEDs for entering suspend mode.
    // Returns the current LED state.  This can be restored after suspending
    // by calling restoreLEDs().
    uint8_t suspendLEDs() override {
        uint8_t current_leds = (PORTD & LED_MASK);
        PORTD &= ~LED_MASK;
        return current_leds;
    }
    void restoreLEDs(uint8_t value) override {
        PORTD |= (value & LED_MASK);
    }

  private:
    enum : uint8_t{
        LED_MASK = 0x1f,
        PIN_NUM_LOCK = 0x01, // middle left, yellow
        PIN_CAPS_LOCK = 0x10, // leftmost, red
        PIN_SCROLL_LOCK = 0x04, // middle right, yellow
        PIN_POWER = 0x02, // center, green
        PIN_ERROR = 0x08, // rightmost, red
        PIN_TEENSY_ONBOARD = 0x40,
    };
};

int main() {
    // Set the clock speed as the first thing we do
    set_cpu_prescale();

    KeyboardV1 kbd;
    LedController leds;
    KbdController controller(&kbd, &leds,
                             KEYBOARD_INTERFACE, KEYBOARD_ENDPOINT);
#if USB_DEBUG
    controller.cfgDebugIface(DEBUG_INTERFACE, DEBUG_ENDPOINT, 4096, DEBUG_SIZE);
#endif
    controller.init(ENDPOINT0_SIZE, pgm_cast(usb_descriptors));
    FLOG(1, "Keyboard initialized\n");
    controller.loop();
    return 0;
}
