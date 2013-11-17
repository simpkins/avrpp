// Copyright (c) 2013, Adam Simpkins
#include <avrpp/usb.h>

#include <avrpp/atomic.h>
#include <avrpp/avr_registers.h>
#include <avrpp/log.h>
#include <avrpp/usb_descriptors.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

F_LOG_LEVEL(1);

UsbController UsbController::s_controller;

UsbController::UsbController() {
}

bool
UsbController::addInterface(UsbInterface* iface) {
    UsbInterface** free_entry{nullptr};
    auto number = iface->getNumber();
    for (auto &entry : _interfaces) {
        if (entry == nullptr) {
            if (free_entry == nullptr) {
                free_entry = &entry;
            }
        } else if (entry->getNumber() == number) {
            // Duplicate interface number
            return false;
        }
    }

    if (free_entry == nullptr) {
        // No free interface slots
        return false;
    }

    // Add the interface endpoints
    if (!iface->addEndpoints(this)) {
        return false;
    }
    *free_entry = iface;
    return true;
}

bool
UsbController::addEndpoint(UsbEndpoint* endpoint) {
    UsbEndpoint** free_entry{nullptr};
    auto number = endpoint->getNumber();
    for (auto &entry : _endpoints) {
        if (entry == nullptr) {
            if (free_entry == nullptr) {
                free_entry = &entry;
            }
        } else if (entry->getNumber() == number) {
            // Duplicate endpoint number
            return false;
        }
    }

    if (free_entry == nullptr) {
        // No free endpoint slots
        return false;
    }

    *free_entry = endpoint;
    return true;
}

void
UsbController::init(uint8_t endpoint0_size,
                    pgm_ptr<UsbDescriptor> descriptors) {
    AtomicGuard guard;

    _endpoint0Size = endpoint0_size;
    _descriptors.reset(descriptors);

    // Enable the USB pads regulators, and configure for device mode
    set_UHWCON(UHWCONFlags::DEVICE_MODE | UHWCONFlags::ENABLE_PADS_REGULATOR);
    // Enable USB, but disable the clock since the PLL is not configured
    set_USBCON(USBCONFlags::ENABLE | USBCONFlags::FREEZE_CLOCK);

    // Enable the PLL, and wait for it to lock with the input clock
    // (According to the at90usb1286 docs, it takes about 100ms to lock.)
    set_PLLCSR(PLLCSRFlags::ENABLE | PLLCSRFlags::PRESCALER_8_AT90USB128x);
    while (!isset_PLLCSR(PLLCSRFlags::LOCK)) {
        // wait
    }

    // Now disable the FREEZE_CLOCK flag, and enable the OTG pad,
    // which is required for USB operation
    set_USBCON(USBCONFlags::ENABLE | USBCONFlags::ENABLE_OTG_PAD);
    // Enable the attach resistor
    set_UDCON(UDCONFlags::NONE);
    _configured = false;
    // Enable desired interrupts
    set_UDIEN(UDIENFlags::END_OF_RESET | UDIENFlags::START_OF_FRAME);
}

void
UsbController::endpointInterrupt() {
    UENUM = 0;
    const UEINTXFlags intr_bits = get_UEINTX();
    if (isset(intr_bits, UEINTXFlags::RX_SETUP)) {
        processSetupPacket();
        return;
    }

    FLOG(1, "unhandled endpoint interrupt: intr_bits=%#02x\n",
         static_cast<uint8_t>(intr_bits));
    stall();
}

void
UsbController::generalInterrupt() {
    const auto intr_flags = get_UDINT();
    set_UDINT(UDINTFlags::NONE);

    if (isset(intr_flags, UDINTFlags::END_OF_RESET)) {
        // We have received a reset signal from the host.
        // Reset the USB configuration.  We'll re-enable the _configured
        // flag once we receive a SET_CONFIGURATION request from the host.
        UENUM = 0;
        set_UECONX(UECONXFlags::ENABLE);
        set_UECFG0X(UECFG0XFlags::CONTROL);
        set_UECFG1X(UECFG1XFlags::CFG1_ALLOC | UECFG1XFlags::SINGLE_BANK |
                    usb_cfg_size(_endpoint0Size));
        set_UEIENX(UEIENXFlags::RX_SETUP);
        _configured = false;
    }

    // TODO: Handle SUSPEND and WAKE_UP interrupts.

    if (isset(intr_flags, UDINTFlags::START_OF_FRAME) && _configured) {
        for (const auto& iface : _interfaces) {
            if (iface) {
                iface->startOfFrame();
            }
        }
    }
}

bool
UsbController::waitForTxReady() {
    // FIXME: Add a loop limit, so we cannot loop here infinitely.
    //
    // Currently most callers do not check the return value, so they
    // will need to be fixed.
    while (!isset_UEINTX(UEINTXFlags::TX_READY)) {
        // wait
    }
    return true;
}

bool
UsbController::waitForTxTransfer() {
    // FIXME: Add a loop limit, so we cannot loop here infinitely.
    while (true) {
        const auto intr_bits = get_UEINTX();
        if (isset(intr_bits, UEINTXFlags::RX_OUT)) {
            return false;
        }
        if (isset(intr_bits, UEINTXFlags::TX_READY)) {
            return true;
        }
    }
}

void
UsbController::handleGetDescriptor(const SetupPacket *pkt) {
    const uint8_t *desc_addr;
    uint8_t desc_length;
    if (!_descriptors.findDescriptor(pkt->wValue, pkt->wIndex,
                                     &desc_addr, &desc_length)) {
        stall();
        return;
    }

    UENUM = 0;

    uint16_t len_left = pkt->wLength;
    if (len_left > desc_length) {
        len_left = desc_length;
    }
    while (len_left > 0) {
        // Wait for the TX bank to be ready for us to write to it.
        if (!waitForTxTransfer()) {
            return; // abort
        }
        const uint8_t packet_len = (len_left < _endpoint0Size ?
                                    len_left : _endpoint0Size);
        // Send the next segment of the descriptor
        // (up to the max endpoint 0 packet size)
        for (uint8_t n = 0; n < packet_len; ++n) {
            UEDATX = pgm_read_byte(desc_addr++);
        }
        len_left -= packet_len;
        sendIn();
    }
}

void
UsbController::processSetupPacket() {
    // Read the setup packet
    SetupPacket pkt;
    pkt.bmRequestType = UEDATX;
    pkt.bRequest = UEDATX;
    pkt.wValue = UEDATX;
    pkt.wValue |= (UEDATX << 8);
    pkt.wIndex = UEDATX;
    pkt.wIndex |= (UEDATX << 8);
    pkt.wLength = UEDATX;
    pkt.wLength |= (UEDATX << 8);

    // Clear RX_SETUP and RX_OUT, to let the controller
    // know we have read the setup packet and the data in this bank
    // can be cleared to read a new packet.
    set_UEINTX(get_UEINTX() &
               ~(UEINTXFlags::RX_SETUP | UEINTXFlags::RX_OUT));

    FLOG(2, "SETUP received: bmRequestType=%#04x bRequest=%#04x "
         "wValue=%#06x wIndex=%#06x wLength=%#06x\n",
         pkt.bmRequestType, pkt.bRequest, pkt.wValue, pkt.wIndex,
         pkt.wLength);

    bool handled = false;
    const uint8_t recipient = (pkt.bmRequestType & 0x1f);
    if (recipient == RECIPIENT_DEVICE) {
        handled = processDeviceSetupPacket(&pkt);
    } else if (recipient == RECIPIENT_INTERFACE) {
        const uint8_t num = (pkt.wIndex & 0xff);
        for (const auto& iface : _interfaces) {
            if (iface && iface->getNumber() == num) {
                handled = iface->handleSetupPacket(&pkt);
                break;
            }
        }
    } else if (recipient == RECIPIENT_ENDPOINT) {
        for (const auto& ep : _endpoints) {
            const uint8_t num = (pkt.wIndex & 0xf);
            if (ep && ep->getNumber() == num) {
                handled = ep->handleSetupPacket(&pkt);
                break;
            }
        }
    }

    if (!handled) {
        FLOG(1, "unhandled SETUP packet: bmRequestType=%#04x bRequest=%#04x "
             "wValue=%#06x wIndex=%#06x wLength=%#06x\n",
             pkt.bmRequestType, pkt.bRequest, pkt.wValue, pkt.wIndex,
             pkt.wLength);
        stall();
    }
}

bool
UsbController::processDeviceSetupPacket(const SetupPacket *pkt) {
    // We currently only handle standard request types
    if ((pkt->bmRequestType & SETUP_TYPE_MASK) != TYPE_STD) {
        return false;
    }

    if (pkt->bRequest == StdRequestType::GET_DESCRIPTOR) {
        handleGetDescriptor(pkt);
        return true;
    } else if (pkt->bRequest == StdRequestType::SET_ADDRESS) {
        // USB dictates that we respond with a 0 byte IN packet
        sendIn();
        waitForTxReady();
        // The AVR firmware will have already recorded the specified address
        // in UADD.
        // Now set ADDEN to enable actually using this address.
        UDADDR = pkt->wValue | (1 << ADDEN);
        return true;
    } else if (pkt->bRequest == StdRequestType::SET_CONFIGURATION) {
        _configured = pkt->wValue;
        sendIn();
        for (const auto& ep : _endpoints) {
            if (!ep) {
                continue;
            }
            ep->configure();
        }
        UERST = 0x1E;
        UERST = 0;
        return true;
    } else if (pkt->bRequest == StdRequestType::GET_CONFIGURATION) {
        waitForTxReady();
        UEDATX = _configured;
        sendIn();
        return true;
    } else if (pkt->bRequest == StdRequestType::GET_STATUS) {
        waitForTxReady();
        UEDATX = 0;
        UEDATX = 0;
        sendIn();
        return true;
    }
    return false;
}

bool
UsbEndpoint::handleSetupPacket(const SetupPacket* pkt) {
    if ((pkt->bRequest == StdRequestType::CLEAR_FEATURE ||
         pkt->bRequest == StdRequestType::SET_FEATURE)
        && pkt->wValue == 0) {
        // Support endpoint halt functionality
        UsbController::sendIn();
        UENUM = _number;
        if (pkt->bRequest == SET_FEATURE) {
            set_UECONX(UECONXFlags::STALL_REQUEST | UECONXFlags::ENABLE);
        } else {
            set_UECONX(UECONXFlags::STALL_REQUEST_CLEAR |
                       UECONXFlags::RESET_DATA_TOGGLE |
                       UECONXFlags::ENABLE);
            UERST = (1 << _number);
            UERST = 0;
        }
        return true;
    } else if (pkt->bRequest == StdRequestType::GET_STATUS) {
        UsbController::waitForTxReady();
        UENUM = _number;
        bool stalled = isset_UECONX(UECONXFlags::STALL_REQUEST);
        UENUM = 0;
        UEDATX = stalled ? 1 : 0;
        UEDATX = 0;
        UsbController::sendIn();
        return true;
    }

    return false;
}

void
UsbEndpoint::configureImpl(UECFG0XFlags cfg0, UECFG1XFlags cfg1) {
    UENUM = _number;
    set_UECONX(UECONXFlags::ENABLE);
    set_UECFG0X(cfg0);
    set_UECFG1X(UECFG1XFlags::CFG1_ALLOC | cfg1);
}

// USB Endpoint/Pipe Interrupt
ISR(USB_COM_vect) {
    UsbController::singleton()->endpointInterrupt();
}

// USB General Interrupt
ISR(USB_GEN_vect) {
    UsbController::singleton()->generalInterrupt();
}
