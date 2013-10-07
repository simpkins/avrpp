// Copyright (c) 2013, Adam Simpkins
#include "dbg_endpoint.h"

#include "atomic.h"
#include "log.h"
#include "usb.h"
#include "usb_descriptors.h"
#include "usb_hid.h"
#include "pjrc/teensy.h"

#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdlib.h>

static uint8_t f_log_level = 2;

enum : uint8_t { DBG_FLUSH_TIMEOUT_MS = 10 };

DebugIface::DebugIface(uint8_t iface, uint8_t endpoint,
                       uint16_t buf_len, uint8_t report_len)
    : UsbInterface(iface),
      _endpoint(endpoint, report_len),
      _buflen(buf_len) {
    if (buf_len > 0) {
        _buffer = static_cast<uint8_t*>(malloc(buf_len));
    }
}

DebugIface::~DebugIface() {
    free(_buffer);
}

bool
DebugIface::putcharC(uint8_t c, void *arg) {
    auto iface = reinterpret_cast<DebugIface*>(arg);
    return iface->putchar(c);
}

bool
DebugIface::putchar(uint8_t c) {
    // Run with interrupts disabled.
    // This prevents putchar() from being called again in interrupt context
    // while we are still in the middle of the current putchar() call.
    AtomicGuard ag;

    if (c == '\n') {
        return _putcharLocked('\r', &ag) && _putcharLocked('\n', &ag);
    } else {
        return _putcharLocked(c, &ag);
    }
}

bool
DebugIface::_putcharLocked(uint8_t c, const AtomicGuard *ag) {
    // If interrupts were already disabled when we were called,
    // don't try writing immediately.
    //
    // This is primarily to ensure that we don't try writing to a USB endpoint
    // from inside an interrupt that is already in the middle of doing some USB
    // processing.
    //
    // TODO: It would be nice to re-organize the USB code to add a better guard
    // to detect this.  As-is, anyone that disables interrupts will prevent an
    // immediate write attempt here, even if they aren't performing USB
    // processing.
    if (_paused || (ag->getInitialState() & 0x80) == 0) {
        return _writeToBuffer(c);
    }

    // If USB is fully configured, the buffer is empty, and we have room to
    // write in the endpoint FIFO, just write directly to the FIFO.
    if (_tryImmediateWrite(c)) {
        return true;
    }

    // If we are still here, we can't write directly to the USB FIFO at the
    // moment, so store the data in our internal buffer.
    return _writeToBuffer(c);
}

bool
DebugIface::_writeToBuffer(uint8_t c) {
    if (_bufferFull || !_buffer) {
        // We have to drop something.  We can drop either the oldest data
        // in the buffer, or the new character.
        // For now we drop the new character.
        //
        // TODO: It would be nice to track where data is missing, so we can
        // report that.  Dropping the oldest data would make it easier to
        // report that.
        return false;
    }

    _buffer[_writeOffset] = c;
    ++_writeOffset;
    if (_writeOffset == _buflen) {
        _writeOffset = 0;
    }
    if (_writeOffset == _readOffset) {
        _bufferFull = true;
    }
    return true;
}

bool
DebugIface::_tryImmediateWrite(uint8_t c) {
    // If USB isn't fully configured yet, we can't write now.
    if (!UsbController::singleton()->configured()) {
        return false;
    }

    // If we have data already in the buffer, we can't write directly to USB,
    // as that would send the data out of order.
    if (_bufferFull || (_readOffset != _writeOffset)) {
        return false;
    }

    UENUM = _endpoint.getNumber();
    return _tryUsbWrite(c);
}

/**
 * Try writing a character to the endpoint bank.
 *
 * This should only be called with interrupts disabled, and with UENUM already
 * set to the correct endpoint number.
 */
bool
DebugIface::_tryUsbWrite(uint8_t c) {
    // Check the RW_ALLOWED flag to see if the endpoint bank is full.
    // If it is, we can't write directly to USB right now.
    if (!isset_UEINTX(UEINTXFlags::RW_ALLOWED)) {
        return false;
    }

    // If we are still here we can write into the FIFO now.
    UEDATX = c;

    // Send the packet if the FIFO is full now.
    const auto ueintx_bits = get_UEINTX();
    if (!isset(ueintx_bits, UEINTXFlags::RW_ALLOWED)) {
        // Clear the FIFOCON bit
        set_UEINTX(ueintx_bits & ~UEINTXFlags::FIFO_CONTROL);
        _flushTimer = 0;
    } else if (_flushTimer == 0) {
        _flushTimer = DBG_FLUSH_TIMEOUT_MS;
    }
    return true;
}

bool
DebugIface::addEndpoints(UsbController* usb) {
    return usb->addEndpoint(&_endpoint);
}

void
DebugIface::startOfFrame() {
    UENUM = _endpoint.getNumber();

    // If we have any data pending in the write buffer,
    // try writing it to our endpoint bank.
    if (!_paused) {
        while (_bufferFull || _readOffset != _writeOffset) {
            const uint8_t c = _buffer[_readOffset];
            if (!_tryUsbWrite(c)) {
                break;
            }
            ++_readOffset;
            if (_readOffset == _buflen) {
                _readOffset = 0;
            }
            _bufferFull = false;
        }
    }

    // If we have a partial packet pending, flush it now if it has been pending
    // for more than DBG_FLUSH_TIMEOUT milliseconds.
    if (_flushTimer != 0) {
        --_flushTimer;
        if (_flushTimer == 0) {
            // PJRC's hid_listen program on Windows doesn't seem to behave
            // well if we don't always send full packets.  (On the other hand,
            // Linux is fine with partial packets.)
            //
            // Pad the remainder of the packet with nul bytes before we send
            // it.
            while (true) {
                const auto ueintx_bits = get_UEINTX();
                if (!isset(ueintx_bits, UEINTXFlags::RW_ALLOWED)) {
                    set_UEINTX(ueintx_bits & ~UEINTXFlags::FIFO_CONTROL);
                    break;
                }
                UEDATX = 0;
            }
        }
    }
}

bool
DebugIface::handleSetupPacket(const SetupPacket *pkt) {
    if (pkt->bRequest == HID_GET_REPORT && pkt->bmRequestType == 0xA1) {
        auto usb_ctl = UsbController::singleton();
        const uint8_t ep0_size = usb_ctl->getEndpoint0Size();
        uint8_t len_left = pkt->wLength;
        while (len_left > 0) {
            // Wait for the TX bank to be ready for us to write to it
            if (!usb_ctl->waitForTxTransfer()) {
                return true; // abort
            }
            // Send the next segment of the report
            // (up to the max endpoint 0 packet size)
            const auto packet_len = len_left < ep0_size ? len_left : ep0_size;
            for (uint8_t n = 0; n < packet_len; ++n) {
                UEDATX = 0;
            }
            len_left -= packet_len;
            UsbController::sendIn();
        }
        return true;
    }

    if (pkt->bmRequestType == 0x81) {
        if (pkt->bRequest == StdRequestType::GET_DESCRIPTOR) {
            UsbController::singleton()->handleGetDescriptor(pkt);
            return true;
        }
    }

    if (pkt->bmRequestType == 0x21) {
        if (pkt->bRequest == HID_SET_REPORT) {
            return _handleSetReport(pkt);
        } else if (pkt->bRequest == HID_SET_IDLE) {
            // We don't have state that gets retransmitted on every
            // interrupt poll, so we just ignore the idle setting.
            UsbController::sendIn();
            return true;
        }
    }

    return false;
}

bool
DebugIface::_handleSetReport(const SetupPacket *pkt) {
    if ((pkt->wValue >> 8) == 3) {
        // Jump to the bootloader when we get a set feature
        // request.
        uint8_t report_id = pkt->wValue & 0xff;

        if (pkt->wLength < 1) {
            return false;
        }

        UsbController::waitForOutPacket();
        uint8_t value1 = UEDATX;
        UsbController::ackOut();

        UsbController::sendIn();
        FLOG(1, "dbg set feature: rid=%d len=%d v1=%#x\n",
             report_id, pkt->wLength, value1);
        if (value1 == 0x01) {
            // A value of 1 triggers us to jump the the HalfKay loader code.
            FLOG(1, "Running HalfKay...\n");
            _delay_ms(5);
            jump_to_bootloader();
        } else if (value1 == 0x02) {
            // A value of 2 triggers us to reset the device, doing
            // a normal boot instead of HalfKay.
            //
            // We do this by enabling the watchdog timer, then waiting
            // for it to expire.
            FLOG(1, "Resetting...\n");
            cli();
            wdt_reset();
            MCUSR &= ~(1 << WDRF);
            wdt_enable(WDTO_15MS);
            while (true) {
                // wait
            }
        }
        return true;
    }

    return false;
}

DebugEndpoint::DebugEndpoint(uint8_t number, uint8_t report_len)
    : UsbEndpoint(number),
      _reportLength(report_len) {
}

void
DebugEndpoint::configure() {
    configureImpl(UECFG0XFlags::DIRECTION_IN | UECFG0XFlags::INTERRUPT,
                  UECFG1XFlags::DOUBLE_BANK | usb_cfg_size(_reportLength));
}
