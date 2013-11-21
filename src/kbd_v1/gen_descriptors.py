#!/usr/bin/python3 -tt
#
# Copyright (c) 2013, Adam Simpkins
#
import os
import sys

sys.path.insert(0, os.path.dirname(sys.path[0]))
import usb_config


# TODO: Share code with the kbd_v2 gen_descriptors.py program,
# rather than copy-and-pasting most of it.


def gen_config():
    ENDPOINT0_SIZE = 32
    KEYBOARD_INTERFACE = 0
    KEYBOARD_ENDPOINT = 1
    KEYBOARD_SIZE = 8

    usb_debug = True
    if usb_debug:
        DEBUG_INTERFACE = 1
        DEBUG_ENDPOINT = 2
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
    config.dev_descriptor.product = 'Keyboard v1'
    config.dev_descriptor.serial = 'KBD1-0001'

    config.add_constants(KEYBOARD_INTERFACE=KEYBOARD_INTERFACE,
                         KEYBOARD_ENDPOINT=KEYBOARD_ENDPOINT,
                         KEYBOARD_SIZE=KEYBOARD_SIZE)

    # Boot Keyboard descriptor.
    # This report format is prescribed by the HID 1.11 specification,
    # in Appendix A.
    kbd_report_desc = usb_config.HidReportDescriptor(bytearray([
        0x05, 0x01,  # Usage Page (Generic Desktop),
        0x09, 0x06,  # Usage (Keyboard),
        0xA1, 0x01,  # Collection (Application),
        0x75, 0x01,  #   Report Size (1),
        0x95, 0x08,  #   Report Count (8),
        0x05, 0x07,  #   Usage Page (Key Codes),
        0x19, 0xE0,  #   Usage Minimum (224),
        0x29, 0xE7,  #   Usage Maximum (231),
        0x15, 0x00,  #   Logical Minimum (0),
        0x25, 0x01,  #   Logical Maximum (1),
        0x81, 0x02,  #   Input (Data, Variable, Absolute), (Modifier byte)
        0x95, 0x01,  #   Report Count (1),
        0x75, 0x08,  #   Report Size (8),
        0x81, 0x03,  #   Input (Constant),                 (Reserved byte)
        0x95, 0x05,  #   Report Count (5),
        0x75, 0x01,  #   Report Size (1),
        0x05, 0x08,  #   Usage Page (LEDs),
        0x19, 0x01,  #   Usage Minimum (1),
        0x29, 0x05,  #   Usage Maximum (5),
        0x91, 0x02,  #   Output (Data, Variable, Absolute), (LED report)
        0x95, 0x01,  #   Report Count (1),
        0x75, 0x03,  #   Report Size (3),
        0x91, 0x03,  #   Output (Constant),                 (LED report padding)
        0x95, 0x06,  #   Report Count (6),
        0x75, 0x08,  #   Report Size (8),
        0x15, 0x00,  #   Logical Minimum (0),
        0x25, 0x91,  #   Logical Maximum(145),
        0x05, 0x07,  #   Usage Page (Key Codes),
        0x19, 0x00,  #   Usage Minimum (0),
        0x29, 0x91,  #   Usage Maximum (145),
        0x81, 0x00,  #   Input (Data, Array),
        0xc0         # End Collection
    ]))

    kbd_endpoint = usb_config.EndpointDescriptor(
            address=0x80 | KEYBOARD_ENDPOINT,
            attributes=0x03,
            max_packet_size=KEYBOARD_SIZE,
            interval=10)
    kbd_boot_iface = usb_config.IfaceDescriptor(
            number=KEYBOARD_INTERFACE,
            iface_class=usb_config.CLASS_HID,
            subclass=usb_config.HID_SUBCLASS_BOOT,
            protocol=usb_config.HID_PROTOCOL_KEYBOARD,
            endpoints=[kbd_endpoint])
    kbd_hid = usb_config.HidDescriptor([kbd_report_desc])

    if usb_debug:
        config.add_constants(DEBUG_INTERFACE=DEBUG_INTERFACE,
                             DEBUG_ENDPOINT=DEBUG_ENDPOINT,
                             DEBUG_SIZE=DEBUG_SIZE,
                             USB_DEBUG=1)
        dbg_report_desc = usb_config.HidReportDescriptor(bytearray([
            0x06, 0x31, 0xFF,     # Usage Page 0xFF31 (vendor defined)
            0x09, 0x74,           # Usage 0x74
            0xA1, 0x53,           # Collection 0x53
            0x75, 0x08,           #   Report Size = 8 bits
            0x15, 0x00,           #   Logical minimum = 0
            0x26, 0xFF, 0x00,     #   Logical maximum = 255
            0x95, DEBUG_SIZE,     #   Report Count
            0x09, 0x75,           #   Local Usage
            0x81, 0x02,           #   Input (array)
            0x09, 0x76,           #   Local Usage
            0x95, 0x01,           #   Report Count = 1
            0xB1, 0x00,           #   Feature (variable)
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

    config.configs[0].descriptors = [
        kbd_boot_iface,
        kbd_hid,
        kbd_endpoint,
    ]
    config.add_descriptor(kbd_report_desc, 'keyboard_hid_report_desc',
                          0x2200, KEYBOARD_INTERFACE)

    if usb_debug:
        config.configs[0].descriptors.extend([
            dbg_iface,
            dbg_hid,
            dbg_endpoint
        ])
        config.add_descriptor(dbg_report_desc, 'debug_hid_report_desc',
                              0x2200, DEBUG_INTERFACE)

    return config


if __name__ == '__main__':
    rc = usb_config.main(gen_config)
    sys.exit(rc)
