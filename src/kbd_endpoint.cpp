// Copyright (c) 2013, Adam Simpkins
#include <avrpp/kbd_endpoint.h>

#include <avrpp/atomic.h>
#include <avrpp/log.h>
#include <avrpp/usb.h>
#include <avrpp/usb_descriptors.h>
#include <avrpp/usb_hid.h>
#include <avrpp/usb_hid_keyboard.h>

#include <avr/interrupt.h>
#include <string.h>

F_LOG_LEVEL(1);

KeyboardIface::KeyboardIface(uint8_t iface, uint8_t endpoint)
    : UsbInterface(iface),
      _endpoint(endpoint) {
}

void
KeyboardIface::update(const uint8_t* keys, uint8_t modifiers) {
    memcpy(_pressedKeys, keys, sizeof(_pressedKeys));
    _modifierKeys = modifiers;

    _sendUpdate();
}

void
KeyboardIface::startOfFrame() {
    if (_flags & Flags::UPDATE_PENDING) {
        // Try to send an update
        _sendUpdate();
        return;
    }

    if (!_idleConfig) {
        // Idle timeout is indefinite, so we have nothing else to do.
        return;
    }

    // Increment our idle counters
    //
    // startOfFrame() will be called every 1ms.  _idleConfig specifies
    // how long we should retransmit, in 4ms units.  We use the lower
    // two bits of _flags to track 4ms intervals, and bump _idleCount every
    // time it rolls over.
    if ((_flags & Flags::COUNT_MASK) == Flags::COUNT_MASK) {
        // Another 4ms has passed, so bump _idleCount.
        _flags &= ~Flags::COUNT_MASK;
        ++_idleCount;
        if (_idleCount >= _idleConfig) {
            _sendUpdate();
            return;
        }
    } else {
        ++_flags;
    }
}

bool
KeyboardIface::_sendUpdate() {
    FLOG(3, "kbd update\n");
    // Set UPDATE_PENDING, so that if we fail now, we will try again the next
    // time startOfFrame() is called.
    _flags |= Flags::UPDATE_PENDING;

    AtomicGuard ag;

    if (!UsbController::singleton()->configured()) {
        return false;
    }

    UENUM = _endpoint.getNumber();
    const auto ueintx_bits = get_UEINTX();
    if (!isset(ueintx_bits, UEINTXFlags::RW_ALLOWED)) {
        // Buffer is full, can't transmit now.
        return false;
    }

    // Send the report data
    UEDATX = _modifierKeys;
    UEDATX = 0;
    for (uint8_t i=0; i < MAX_KEYS; i++) {
        UEDATX = _pressedKeys[i];
    }
    set_UEINTX(ueintx_bits & ~UEINTXFlags::FIFO_CONTROL);

    _idleCount = 0;
    _flags &= ~(Flags::UPDATE_PENDING | Flags::COUNT_MASK);
    return true;
}

bool
KeyboardIface::addEndpoints(UsbController* usb) {
    return usb->addEndpoint(&_endpoint);
}

bool
KeyboardIface::handleSetupPacket(const SetupPacket* pkt) {
    if (pkt->bmRequestType == 0xA1) {
        if (pkt->bRequest == HID_GET_REPORT) {
            UsbController::waitForTxReady();
            UEDATX = _modifierKeys;
            UEDATX = 0;
            for (uint8_t i = 0; i < MAX_KEYS; ++i) {
                UEDATX = _pressedKeys[i];
            }
            UsbController::sendIn();
            return true;
        }
        if (pkt->bRequest == HID_GET_IDLE) {
            UsbController::waitForTxReady();
            UEDATX = _idleConfig;
            UsbController::sendIn();
            return true;
        }
        if (pkt->bRequest == HID_GET_PROTOCOL) {
            UsbController::waitForTxReady();
            UEDATX = _protocol;
            UsbController::sendIn();
            return true;
        }
    }
    if (pkt->bmRequestType == 0x81) {
        if (pkt->bRequest == StdRequestType::GET_DESCRIPTOR) {
            UsbController::singleton()->handleGetDescriptor(pkt);
            return true;
        }
    }
    if (pkt->bmRequestType == 0x21) {
        if (pkt->bRequest == HID_SET_REPORT) {
            UsbController::waitForOutPacket();
            uint8_t led_value = UEDATX;
            if (_ledCallback) {
                _ledCallback->updateLeds(led_value);
            }
            UsbController::ackOut();
            UsbController::sendIn();
            return true;
        }
        if (pkt->bRequest == HID_SET_IDLE) {
            _idleConfig = (pkt->wValue >> 8);
            _idleCount = 0;
            // UsbController::waitForTxReady();
            UsbController::sendIn();
            return true;
        }
        if (pkt->bRequest == HID_SET_PROTOCOL) {
            _protocol = pkt->wValue;
            // UsbController::waitForTxReady();
            UsbController::sendIn();
            return true;
        }
    }

    return false;
}

void
KeyboardEndpoint::configure() {
    configureImpl(UECFG0XFlags::DIRECTION_IN | UECFG0XFlags::INTERRUPT,
                  UECFG1XFlags::DOUBLE_BANK |
                  usb_cfg_size<KeyboardIface::REPORT_SIZE>());
}
