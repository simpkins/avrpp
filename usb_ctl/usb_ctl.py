#!/usr/bin/python3 -tt
#
# Copyright (c) 2013, Adam Simpkins
#
import argparse
import binascii
import struct
import sys
import time

import ihex
import libusb
import usb

# This is the Vendor and Product ID that my keyboard currently reports.
# This vendor ID is assigned to voti.nl, who resells product IDs.
# This product ID is listed as free for lab use.
#
# http://www.voti.nl/pids/
DEFAULT_VENDOR_ID = 0x16c0
DEFAULT_PRODUCT_ID = 0x03f1

HALFKAY_VENDOR_ID = 0x16c0
HALFKAY_PRODUCT_ID = 0x0478


def log(msg, *args, **kwargs):
    if args or kwargs:
        msg = msg.format(*args, **kwargs)
    print(msg)


def wait_for_any(ids):
    first = True
    n = 0
    while True:
        for dev in libusb.list_devices():
            desc = dev.device_descriptor
            for vid, pid in ids:
                if desc.idVendor == vid and desc.idProduct == pid:
                    if not first:
                        print('')
                    return dev

        if first:
            print('Waiting for device...', end='', flush=True)
            first = False
        elif n == 30:
            print('.', end='', flush=True)
            n = 0

        n += 1
        time.sleep(0.1)


def wait_for_device(args):
    dev = wait_for_any([(args.device_vendor, args.device_product)])
    return dev.get_handle()


def show_descriptors(dev):
    log('Device Descriptor')
    dev_desc = dev.device_descriptor
    for field, field_type in dev_desc._fields_:
        log('  {}: {}', field, getattr(dev_desc, field))

    log('Config Descriptor')
    cfg_desc = dev.get_active_config_descriptor()
    log('  bLength: {}', cfg_desc.bLength)
    log('  bDescriptorType: {}', cfg_desc.bDescriptorType)
    log('  wTotalLength: {}', cfg_desc.wTotalLength)
    log('  bNumInterfaces: {}', cfg_desc.bNumInterfaces)
    log('  bConfigurationValue: {}', cfg_desc.bConfigurationValue)
    log('  iConfiguration: {}', cfg_desc.iConfiguration)
    log('  bmAttributes: {}', cfg_desc.bmAttributes)
    log('  MaxPower: {}', cfg_desc.MaxPower)
    log('  extra: {}', binascii.hexlify(cfg_desc.extra))
    for iface_idx, iface_info in enumerate(cfg_desc.interfaces):
        log('  Interface {}', iface_idx)
        for setting_idx, iface in enumerate(iface_info.settings):
            log('    Setting {}', setting_idx)
            log('      bLength: {}', iface.bLength)
            log('      bDescriptorType: {}', iface.bDescriptorType)
            log('      bAlternateSetting: {}', iface.bAlternateSetting)
            log('      bInterfaceNumber: {}', iface.bInterfaceNumber)
            log('      bAlternateSetting: {}', iface.bAlternateSetting)
            log('      bNumEndpoints: {}', iface.bNumEndpoints)
            log('      bInterfaceClass: {}', iface.bInterfaceClass)
            log('      bInterfaceSubClass: {}', iface.bInterfaceSubClass)
            log('      bInterfaceProtocol: {}', iface.bInterfaceProtocol)
            log('      iInterface: {}', iface.iInterface)
            log('      extra: {}', binascii.hexlify(iface.extra))
            for endpoint_idx, endpoint in enumerate(iface.endpoints):
                log('    Endpoint {}', endpoint_idx)
                log('      bLength: {}', endpoint.bLength)
                log('      bDescriptorType: {}', endpoint.bDescriptorType)
                log('      bEndpointAddress: {:#04x}',
                    endpoint.bEndpointAddress)
                log('      bmAttributes: {}', endpoint.bmAttributes)
                log('      wMaxPacketSize: {}', endpoint.wMaxPacketSize)
                log('      bInterval: {}', endpoint.bInterval)
                log('      bRefresh: {}', endpoint.bRefresh)
                log('      bSynchAddress: {}', endpoint.bSynchAddress)
                log('      extra: {}', binascii.hexlify(endpoint.extra))


def cmd_descriptors(args):
    dev = libusb.find_device(args.device_vendor, args.device_product)
    show_descriptors(dev)


def show_log(args, endpoint):
    while True:
        try:
            buf = endpoint.read()
        except libusb.LibusbError as ex:
            if ex.code == libusb.ERROR_TIMEOUT:
                continue
            if ex.code == libusb.ERROR_IO:
                # When the device is disconnected we get ERROR_IO for a bit
                # before the disconnect completes and we start getting
                # ERROR_NO_DEVICE.
                log('-- I/O error')
                time.sleep(0.1)
                continue
            raise

        if args.raw:
            log('msg: {!r}', buf)
        else:
            # Messages are padded with NUL characters.
            # Strip them out
            buf = b''.join(buf.split(b'\x00'))
            sys.stdout.write(buf.decode('utf-8', errors='replace'))


def get_debug_iface(dev):
    '''
    Get the debug interface information from the specified device.

    Returns (interface number, endpoint number)
    '''
    if isinstance(dev, libusb.DeviceHandle):
        dev = dev.device

    cfg_desc = dev.get_active_config_descriptor()
    for iface_idx, iface_info in enumerate(cfg_desc.interfaces):
        iface = iface_info.settings[0]
        if iface.bInterfaceClass != usb.CLASS_HID:
            continue
        if iface.bInterfaceSubClass != 0:
            continue
        if iface.bInterfaceProtocol != 0:
            continue

        # TODO: Get the HID descriptor, and check the usage information.

        if len(iface.endpoints) != 1:
            continue
        endpoint = iface.endpoints[0]
        ep_num = (endpoint.bEndpointAddress & 0x7f)
        return (iface_idx, ep_num)

    raise Exception('no debug interface found')


def cmd_log(args):
    try:
        while True:
            try:
                handle = wait_for_device(args)
                dbg_iface, dbg_ep = get_debug_iface(handle)
                with handle.interface(dbg_iface) as iface:
                    try:
                        log('-- Reading log:')
                        ep = iface.get_endpoint(dbg_ep)
                        show_log(args, ep)
                    except KeyboardInterrupt:
                        # If we are interrupted while we have the interface
                        # claimed, releasing it and re-attaching the original
                        # driver may block for several seconds.
                        # Therefore print the "Interrupted" message now,
                        # to avoid the appearance that we have hung.
                        log('\nInterrupted.  Releasing device...')
                        return
            except libusb.LibusbError as ex:
                if ex.code == libusb.ERROR_NO_DEVICE:
                    log('\n-- Device disconnected')
                else:
                    log('\n-- Error: {}'.format(ex))
    except KeyboardInterrupt:
        log('\nInterrupted')


def reboot_device(dev, halfkay=False):
    handle = dev.get_handle()
    dbg_iface, dbg_ep = get_debug_iface(handle)
    if halfkay:
        value = b'\x01'
    else:
        value = b'\x02'

    with handle.interface(dbg_iface) as iface:
        ep = handle.control_endpoint()
        ep.hid_set_feature(value, interface=iface)


def cmd_reset(args):
    dev = libusb.find_device(args.device_vendor, args.device_product)
    reboot_device(dev, halfkay=args.halfkay)


def program_device(dev, path):
    ihex_data = ihex.parse_file(path)

    handle = dev.get_handle()
    with handle.interface(0) as iface:
        ep = handle.control_endpoint()
        _load_program(ep, ihex_data)

        # Ask HalfKay to reboot into the user code
        log('Rebooting...')
        _halfkay_reboot(ep)


def _halfkay_reboot(ep):
    block_size = 256
    buf = b'\xff\xff' + (b'\x00' * block_size)
    ep.hid_set_report(buf, interface=0, timeout=2)


def _load_program(ep, ihex_data):
    code_size = 130048
    block_size = 256

    struct_fmt = '<H{}s'.format(block_size)
    for addr in range(0, code_size, block_size):
        data = ihex_data.get_data(addr, block_size)
        if data is None:
            if addr == 0:
                # Always write to address 0.
                # HalfKay erases the entire chip on the first write,
                # so this will trigger the full chip write.
                data = b'\x00' * 256
            else:
                continue

        orig_addr = addr
        if code_size > 0xffff:
            # For devices with more than 16kb of program space,
            # HalfKay accepts the address shifted by 1 byte, so we can
            # address the entire space.
            addr = addr >> 8
        halfkay_data = struct.pack(struct_fmt, addr, data)
        ep.hid_set_report(halfkay_data, interface=0, timeout=2)


def cmd_program(args):
    # Wait for the device.
    # Look for it either in normal mode or in the HalfKay loader mode.
    ids = [(args.device_vendor, args.device_product),
           (HALFKAY_VENDOR_ID, HALFKAY_PRODUCT_ID)]
    dev = wait_for_any(ids)
    desc = dev.device_descriptor
    if (desc.idVendor == args.device_vendor and
        desc.idProduct == args.device_product):
        # We found the device running normally.
        # Ask it to jump to the HalfKay bootloader
        reboot_device(dev, halfkay=True)
        # Now wait for HalfKay to finish booting and appear
        dev = wait_for_any([(HALFKAY_VENDOR_ID, HALFKAY_PRODUCT_ID)])

    program_device(dev, args.program)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-d', '--device',
                    help='The device to use.')
    ap.add_argument('-V', '--vendor',
                    type=int, default=DEFAULT_VENDOR_ID,
                    dest='device_vendor',
                    help='The vendor ID to search for.')
    ap.add_argument('-P', '--product',
                    type=int, default=DEFAULT_PRODUCT_ID,
                    dest='device_product',
                    help='The product ID to search for.')
    cmd_parsers = ap.add_subparsers(dest='cmd', title='Commands')

    log_parser = cmd_parsers.add_parser(
            'descriptors',
            help='Show the device descriptors')
    log_parser.set_defaults(func=cmd_descriptors)

    # log arguments
    log_parser = cmd_parsers.add_parser(
            'log', help='Show the device debug log')
    log_parser.add_argument('--raw',
                            action='store_true', default=False,
                            help='Print the raw bytes received.')
    log_parser.set_defaults(func=cmd_log)

    # reset arguments
    reset_parser = cmd_parsers.add_parser(
            'reset', help='Reset the device')
    reset_parser.add_argument('-H', '--halfkay',
                              action='store_true', default=False,
                              help='Boot into the HalfKay loader')
    reset_parser.set_defaults(func=cmd_reset)

    # program arguments
    pgm_parser = cmd_parsers.add_parser(
            'program', help='Upload a new program to the device')
    pgm_parser.add_argument('program',
                            help='The program file to upload to the device.')
    pgm_parser.set_defaults(func=cmd_program)

    args = ap.parse_args()

    if args.cmd is None:
        ap.error('no command specified')

    return args.func(args)


if __name__ == '__main__':
    rc = main()
    sys.exit(rc)
