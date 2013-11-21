// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/kbd_endpoint.h>
#include <avrpp/kbd/Keyboard.h>
#include <avrpp/usb.h>

class DebugIface;

class KbdController : private Keyboard::Callback,
                      private KeyboardIface::LedCallback,
                      private UsbController::StateCallback {
  public:
    class LedController {
      public:
        virtual ~LedController() {}

        virtual void setPowerLED() = 0;
        virtual void clearPowerLED() = 0;
        virtual void setErrorLED() = 0;
        virtual void clearErrorLED() = 0;
        virtual void setKeyboardLEDs(uint8_t led_value) = 0;
        /*
         * Turn off all LEDs for entering suspend mode.
         * Returns the current LED state.  This can be restored after suspending
         * by calling restoreLEDs().
         */
        virtual uint8_t suspendLEDs() = 0;
        virtual void restoreLEDs(uint8_t value) = 0;
    };

    KbdController(Keyboard *kbd, LedController *leds,
                  uint8_t iface_number, uint8_t endpoint_number);
    virtual ~KbdController();

    /*
     * Configure the debug interface.
     * This must be called before init() if you plan to use the debug
     * interface.
     */
    void cfgDebugIface(uint8_t iface_number, uint8_t endpoint_number,
                       uint16_t buf_len, uint8_t report_len);
    void init(uint8_t endpoint0_size, pgm_ptr<UsbDescriptor> descriptors);
    void loop();

  private:
    // Forbidden copy constructor and assignment operator
    KbdController(KbdController const &) = delete;
    KbdController& operator=(KbdController const &) = delete;

    void waitForUsbInit(UsbController *usb);

    // USB state changes
    virtual void onConfigured() override;
    virtual void onUnconfigured() override;
    virtual void onSuspend() override;
    virtual void onWake() override;

    virtual void updateLeds(uint8_t led_value);

    virtual void onChange(Keyboard* kbd) override;

    Keyboard *_kbd{nullptr};
    LedController *_leds;
    KeyboardIface _kbdIface;
    DebugIface *_dbgIface{nullptr};
};
