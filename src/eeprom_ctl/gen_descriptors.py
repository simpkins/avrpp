#!/usr/bin/python3 -tt
#
# Copyright (c) 2013, Adam Simpkins
#
import os
import sys

sys.path.insert(0, os.path.dirname(sys.path[0]))
import usb_config


def gen_config():
    ENDPOINT0_SIZE = 32
    DEBUG_INTERFACE = 0
    DEBUG_ENDPOINT = 3
    DEBUG_SIZE = 32

    # Vendor and Product ID
    #
    # This Vendor ID is assigned to voti.nl, which resells product IDs.
    # Product IDs 0x03e8 through 0x03f1 are listed as "free (for lab use
    # only)" Since my keyboard is a one-off, this seems reasonable to use
    # for now.
    config = usb_config.UsbConfig(endpoint0_size=ENDPOINT0_SIZE,
                                  vendor_id=0x16c0,
                                  product_id=0x03f1)
    config.dev_descriptor.manufacturer = 'Simpkins'
    config.dev_descriptor.product = 'EEPROM Controller'
    config.dev_descriptor.serial = 'EC-0001'

    config.add_constants(DEBUG_INTERFACE=DEBUG_INTERFACE,
                         DEBUG_ENDPOINT=DEBUG_ENDPOINT,
                         DEBUG_SIZE=DEBUG_SIZE)

    dbg_report_desc = usb_config.HidReportDescriptor(bytearray([
        0x06, 0x31, 0xFF,     # Usage Page 0xFF31 (vendor defined)
        0x09, 0x74,           # Usage 0x74
        0xA1, 0x53,           # Collection 0x53
        0x75, 0x08,           #   report size = 8 bits
        0x15, 0x00,           #   logical minimum = 0
        0x26, 0xFF, 0x00,     #   logical maximum = 255
        0x95, DEBUG_SIZE,     #   report count
        0x09, 0x75,           #   usage
        0x81, 0x02,           #   Input (array)
        0xC0                  # end collection
    ]))

    dbg_endpoint = usb_config.EndpointDescriptor(
            address=0x80 | DEBUG_ENDPOINT,
            attributes=0x03,
            max_packet_size=DEBUG_SIZE,
            interval=1)
    dbg_iface = usb_config.IfaceDescriptor(
            number=DEBUG_INTERFACE,
            iface_class=usb_config.CLASS_HID,
            subclass=0,
            protocol=0,
            endpoints=[dbg_endpoint])
    dbg_hid = usb_config.HidDescriptor([dbg_report_desc])

    config.configs[0].descriptors = [dbg_iface, dbg_hid, dbg_endpoint]

    config.add_descriptor(dbg_report_desc, 'debug_hid_report_desc',
                          0x2200, DEBUG_INTERFACE)

    return config


if __name__ == '__main__':
    rc = usb_config.main(gen_config)
    sys.exit(rc)
