/*
 * Copyright (c) 2013, Adam Simpkins
 */
#include "usb_descriptors.h"

#include "usb.h"

UsbDescriptorMap::UsbDescriptorMap(pgm_ptr<UsbDescriptor> descriptors)
    : _descriptors(descriptors) {
}

bool
UsbDescriptorMap::findDescriptor(uint16_t wValue,
                                 uint16_t wIndex,
                                 const uint8_t **desc_addr,
                                 uint8_t *desc_length) {
    // Simple linear search through the descriptors for now
    for (const UsbDescriptor *p = _descriptors.value(); true; ++p) {
        auto *entry = reinterpret_cast<const uint8_t*>(p);
        const uint8_t *addr =
            reinterpret_cast<const uint8_t*>(pgm_read_word(entry + 4));
        if (addr == nullptr) {
            // Hit the end
            return false;
        }

        if (pgm_read_word(entry) != wValue) {
            continue;
        }
        if (pgm_read_word(entry + 2) != wIndex) {
            continue;
        }

        *desc_addr = addr;
        *desc_length = pgm_read_byte(entry + 6);
        return true;
    }
}
