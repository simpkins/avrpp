// Copyright (c) 2013, Adam Simpkins
#pragma once

#include "avr_registers.h"

#include <avr/io.h>
#include <stdint.h>

struct SetupPacket {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

enum SetupRecipient : uint8_t {
    RECIPIENT_DEVICE = 0,
    RECIPIENT_INTERFACE = 1,
    RECIPIENT_ENDPOINT = 2,
    RECIPIENT_OTHER = 3,
};

enum : uint8_t { SETUP_TYPE_MASK = 0x60 };
enum SetupReqType : uint8_t {
    TYPE_STD = 0x00,
    TYPE_CLASS = 0x20,
    TYPE_VENDOR = 0x40,
    TYPE_RESERVED = 0x60,
};

enum StdRequestType : uint8_t {
    GET_STATUS = 0,
    CLEAR_FEATURE = 1,
    SET_FEATURE = 3,
    SET_ADDRESS = 5,
    GET_DESCRIPTOR = 6,
    GET_CONFIGURATION = 8,
    SET_CONFIGURATION = 9,
    GET_INTERFACE = 10,
    SET_INTERFACE = 11,
};

enum DescriptorType : uint8_t {
    DT_DEVICE = 1,
    DT_CONFIG = 2,
    DT_STRING = 3,
    DT_INTERFACE = 4,
    DT_ENDPOINT = 5,
    DT_DEVICE_QUALIFIER = 6,
    DT_OTHER_SPEED_CONFIGURATION = 7,
    DT_INTERFACE_POWER = 8,

    DT_HID = 0x21,
    DT_HID_REPORT = 0x22,
    DT_HID_PHY_DESCRIPTOR = 0x23,
};

enum {
    MAX_INTERFACES = 4,
    MAX_ENDPOINTS = 6,  // AT90USB128X/64x supports up to 6 endpoints
};

class UsbController;
class UsbDescriptorMap;

class UsbEndpoint {
  public:
    explicit UsbEndpoint(uint8_t number) : _number(number) {}
    virtual ~UsbEndpoint() {}

    uint8_t getNumber() const {
        return _number;
    }

    virtual bool handleSetupPacket(const SetupPacket* pkt);
    virtual void configure() = 0;

  protected:
    void configureImpl(UECFG0XFlags cfg0, UECFG1XFlags cfg1);

  private:
    const uint8_t _number{0xff};
};

class UsbInterface {
  public:
    explicit UsbInterface(uint8_t number) : _number(number) {}
    virtual ~UsbInterface() {}

    uint8_t getNumber() const {
        return _number;
    }

    virtual bool addEndpoints(UsbController* usb) = 0;
    virtual bool handleSetupPacket(const SetupPacket* pkt) = 0;
    virtual void startOfFrame() {}

  private:
    const uint8_t _number{0xff};
};

class UsbController {
  public:
    static UsbController *singleton() {
        return &s_controller;
    }

    bool addInterface(UsbInterface *iface);
    bool addEndpoint(UsbEndpoint *endpoint);

    void init(uint8_t endpoint0_size, UsbDescriptorMap *descriptors);

    /**
     * Return whether or not USB is configured.
     *
     * This returns true if the host device has selected a configuration,
     * and false if we do not have a configuration negotiated with the host.
     */
    bool configured() const {
        return _configured;
    }

    uint8_t getEndpoint0Size() const {
        return _endpoint0Size;
    }

    void endpointInterrupt();
    void generalInterrupt();

    void handleGetDescriptor(const SetupPacket *pkt);

    /**
     * Wait for the current endpoint transmit buffer to be ready
     * to receive a new IN packet.
     *
     * Returns true if the buffer is ready to transmit a new packet, or
     * false if a new OUT packet was received before the IN buffer became ready.
     */
    static bool waitForTxReady();

    /**
     * Wait for the current endpoint transmit buffer to be ready
     * to receive a new IN packet, but fail if a new OUT packet is received
     * before the IN bank becomes ready.
     *
     * This is useful when sending a multi-packet transfer, where we should
     * fail if another request is received from the host before we have
     * completed our transfer.
     *
     * Returns true if the buffer is ready to transmit a new packet, or
     * false if a new OUT packet was received before the IN buffer became ready.
     */
    static bool waitForTxTransfer();

    /**
     * Wait for a new packet to be ready in the OUT bank.
     *
     * FIXME: Update this API so it can fail if the operation times out.
     */
    static inline void waitForOutPacket() {
        while (!isset_UEINTX(UEINTXFlags::RX_OUT)) {
            // wait
        }
    }

    /**
     * Acknowledge a received OUT packet, so the controller can clear the data
     * in the OUT bank and be ready to receive another OUT packet.
     */
    static inline void ackOut() {
        set_UEINTX(get_UEINTX() & ~UEINTXFlags::RX_OUT);
    }

    /**
     * Signal the hardware that the current IN bank has been filled
     * and is ready to be transmitted.
     */
    static inline void sendIn() {
        set_UEINTX(get_UEINTX() & ~UEINTXFlags::TX_READY);
    }

  private:
    UsbController();

    void stall() {
        set_UECONX(UECONXFlags::STALL_REQUEST | UECONXFlags::ENABLE);
    }

    void processSetupPacket();
    bool processDeviceSetupPacket(const SetupPacket *pkt);

    volatile bool _configured{false};
    UsbInterface* _interfaces[MAX_INTERFACES]{nullptr};
    UsbEndpoint* _endpoints[MAX_ENDPOINTS]{nullptr};
    uint8_t _endpoint0Size{32};
    UsbDescriptorMap* _descriptors{nullptr};

    static UsbController s_controller;
};
