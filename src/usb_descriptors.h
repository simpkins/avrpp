// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/progmem.h>
#include <stdint.h>

struct UsbDescriptor {
    uint16_t wValue;
    uint16_t wIndex;
    const uint8_t *addr;
    uint8_t length;
};

class UsbDescriptorMap {
  public:
    UsbDescriptorMap() {}
    explicit UsbDescriptorMap(pgm_ptr<UsbDescriptor> descriptors);

    void reset(pgm_ptr<UsbDescriptor> descriptors) {
        _descriptors = descriptors;
    }

    /*
     * Find the descriptor with the specified wValue and wIndex.
     *
     * Returns false if no matching descriptor is found.
     * Returns true if a matching descriptor is found, and updates
     * *desc_addr and *desc_length with the descriptor information.
     * Note that the descriptor data will be in program memory.
     */
    bool findDescriptor(uint16_t wValue, uint16_t wIndex,
                        const uint8_t **desc_addr, uint8_t *desc_length);

    pgm_ptr<UsbDescriptor> _descriptors;
};
