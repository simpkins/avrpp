// Copyright (c) 2013, Adam Simpkins
#include "keyboard_v2.h"

#include <avrpp/kbd_v2/usb_config.h>

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
        // The LEDs are C0 through C4.
        // Note that C6 and C7 are inputs for the dip switch,
        // and should be configured as inputs with pull-up resistors.
        //
        // Start with the power and error LEDs on, and all others off.
        // (The error LED will be cleared when USB is configured.)
        DDRC = 0x3f;
        PORTC = 0xff & ~(PIN_POWER | PIN_ERROR);
    }

    void setPowerLED() override {
        PORTC &= ~PIN_POWER;
    }
    void clearPowerLED() override {
        PORTC |= PIN_POWER;
    }
    void setErrorLED() override {
        PORTC &= ~PIN_ERROR;
    }
    void clearErrorLED() override {
        PORTC |= PIN_ERROR;
    }

    void setKeyboardLEDs(uint8_t led_value) override {
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

    // Turn off all LEDs for entering suspend mode.
    // Returns the current LED state.  This can be restored after suspending
    // by calling restoreLEDs().
    uint8_t suspendLEDs() override {
        uint8_t current_leds = ((~PORTC) & LED_MASK);
        PORTC |= LED_MASK;
        return current_leds;
    }
    void restoreLEDs(uint8_t value) override {
        PORTC = (PORTC & ~LED_MASK) | ~value;
    }

  private:
    enum : uint8_t{
        LED_MASK = 0x1f,
        PIN_NUM_LOCK = 0x01,
        PIN_CAPS_LOCK = 0x02,
        PIN_SCROLL_LOCK = 0x04,
        PIN_POWER = 0x08,
        PIN_ERROR = 0x10,
    };
};

int main() {
    // Set the clock speed as the first thing we do
    set_cpu_prescale();

    KeyboardV2 kbd;
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
