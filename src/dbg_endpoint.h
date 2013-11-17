// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/usb.h>
#include <stdint.h>

class DebugEndpoint : public UsbEndpoint {
  public:
    explicit DebugEndpoint(uint8_t number, uint8_t report_len);

    virtual void configure() override;

  private:
    uint8_t _reportLength;
};

class AtomicGuard;

class DebugIface : public UsbInterface {
  public:
    /**
     * Create a new debug interface.
     *
     * @param iface       The interface number.
     * @param endpoint    The endpoint number for the interrupt in endpoint.
     * @param buf_len     The size of the internal buffer.  This is used to
     *                    buffer data that cannot be transmitted immediately.
     *                    This may be 0 to disable the buffer entirely,
     *                    and only use the microcontroller's endpoint bank for
     *                    buffering.
     * @param report_len  The size of the HID report for this interface.  This
     *                    must match the report length specified in the HID
     *                    descriptor.  This must also be a power of two, between
     *                    8 and 256.  (Currently this limitation exists since we
     *                    assume the report length exactly matches the endpoint
     *                    bank size.)
     */
    DebugIface(uint8_t iface, uint8_t endpoint,
               uint16_t buf_len=256, uint8_t report_len=32);
    virtual ~DebugIface();

    static bool putcharC(uint8_t c, void *arg);
    bool putchar(uint8_t c);

    virtual bool addEndpoints(UsbController* usb) override;
    virtual bool handleSetupPacket(const SetupPacket *pkt) override;
    virtual void startOfFrame() override;

    bool isPaused() const {
        return _paused;
    }
    void pause(bool pause = true) {
        _paused = pause;
    }
    void unpause() {
        _paused = false;
    }

  private:
    bool _handleSetReport(const SetupPacket *pkt);
    bool _writeToBuffer(uint8_t c);
    bool _tryImmediateWrite(uint8_t c);
    bool _tryUsbWrite(uint8_t c);
    bool _putcharLocked(uint8_t c, const AtomicGuard *ag);

    DebugEndpoint _endpoint;

    // flush_timer is set to 0 when there is no data outstanding waiting to be
    // flushed.  When we receive the first byte in a new packet, flush_timer
    // will be set to DBG_FLUSH_TIMEOUT_MS.  It will be decremented on every
    // SOF packet.  If it reaches 0 before we receive a full packet worth of
    // log data then the partial data that we have will be sent.
    uint8_t _flushTimer{0};

    // A buffer to store log data when we cannot write it immediately to the
    // USB endpoint bank.  This allows log_msg() to work even when USB is not
    // yet fully configured.
    uint16_t _readOffset{0};
    uint16_t _writeOffset{0};
    bool _bufferFull{false};
    bool _paused{false};
    uint16_t _buflen{0};
    uint8_t *_buffer{nullptr};
};
