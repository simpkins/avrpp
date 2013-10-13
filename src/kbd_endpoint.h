// Copyright (c) 2013, Adam Simpkins
#pragma once

#include "usb.h"
#include <stdint.h>

class KeyboardEndpoint : public UsbEndpoint {
  public:
    explicit KeyboardEndpoint(uint8_t number) : UsbEndpoint(number) {}

    virtual void configure() override;
};

class KeyboardIface : public UsbInterface {
  public:
    enum : uint8_t {
        // The HID specification for boot keyboards defines the report size.
        REPORT_SIZE = 8,
        // The USB HID spec defines the protocol for boot keyboards,
        // and only supports up to 6 simultaneous keys being pressed at once.
        MAX_KEYS = 6,
    };

    class LedCallback {
      public:
        virtual ~LedCallback() {}

        virtual void updateLeds(uint8_t value) = 0;
    };

    KeyboardIface(uint8_t iface, uint8_t endpoint);

    void setLedCallback(LedCallback *callback) {
        _ledCallback = callback;
    }

    void update(const uint8_t* keys, uint8_t modifiers);

    virtual bool addEndpoints(UsbController* usb) override;
    virtual bool handleSetupPacket(const SetupPacket* pkt) override;
    virtual void startOfFrame() override;

  private:
    /*
     * The lower 2 bits of _flags are a 2-bit counter, used for incrementing
     * _idleCount.  (_idleCount is incremented on every 4th call to
     * startOfFrame(), since it represents idle time in 4ms units.)
     *
     * The most significant bit of _flags indicates if we need to send
     * an update.
     */
    enum Flags : uint8_t {
        COUNT_MASK = 0x03,
        UPDATE_PENDING = 0x80,
    };

    bool _sendUpdate();

    KeyboardEndpoint _endpoint;
    LedCallback *_ledCallback{nullptr};

    // the idle configuration, how often we send the report to the
    // host (ms * 4) even when it hasn't changed
    uint8_t _idleConfig{125};
    uint8_t _idleCount{0};
    uint8_t _flags{0};
    uint8_t _protocol{1};

    uint8_t _modifierKeys{0};
    uint8_t _pressedKeys[MAX_KEYS]{0};
};
