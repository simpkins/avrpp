#!/usr/bin/python3 -tt
#
# Copyright (c) 2013, Adam Simpkins
#
import argparse
import math
import os
import sys
from tempfile import NamedTemporaryFile

DT_DEVICE = 1
DT_CONFIG = 2
DT_STRING = 3
DT_INTERFACE = 4
DT_ENDPOINT = 5
DT_DEVICE_QUALIFIER = 6
DT_OTHER_SPEED_CONFIGURATION = 7
DT_INTERFACE_POWER = 8
DT_HID = 0x21
DT_HID_REPORT = 0x22
DT_HID_PHY_DESCRIPTOR = 0x23

# USB language codes
# The full list of USB language ID codes is available at
# http://www.usb.org/developers/docs/USB_LANGIDs.pdf
LANG_ENGLISH_US = 0x0409

CLASS_HID = 0x03

HID_SUBCLASS_BOOT = 1

HID_PROTOCOL_KEYBOARD = 1

DEFAULT_ENDPOINT0_SIZE = 32


def bcd(value):
    high = math.floor(value / 10)
    low = value % 10
    return (high << 4) | low


class DescriptorInfo:
    def __init__(self, desc, name, value, index):
        self.desc = desc
        self.name = name
        self.value = value
        self.index = index


class UsbConfig:
    def __init__(self, vendor_id, product_id, endpoint0_size=None):
        if endpoint0_size is None:
            endpoint0_size = DEFAULT_ENDPOINT0_SIZE
        self.dev_descriptor = DeviceDescriptor(self,
                                               endpoint0_size=endpoint0_size,
                                               vendor_id=vendor_id,
                                               product_id=product_id)
        self.configs = [ConfigDescriptor(1)]
        self.other_descriptors = []
        self.strings_map = {}
        self.strings = []
        self.constants_map = {}
        self.constants = []

        self.add_constant('ENDPOINT0_SIZE', 32)

    def add_constant(self, name, value):
        if name in self.constants_map:
            raise KeyError('constant "%s" already exists' % (name,))
        self.constants_map[name] = value
        self.constants.append((name, value))

    def add_constants(self, **kwargs):
        for name, value in kwargs.items():
            self.add_constant(name, value)

    def define_string(self, string):
        if string is None:
            return 0

        idx = self.strings_map.get(string)
        if idx is not None:
            return idx
        idx = len(self.strings_map) + 1
        self.strings_map[string] = idx
        self.strings.append(string)
        return idx

    def add_descriptor(self, desc, name, value, index):
        info = DescriptorInfo(desc, name, value, index)
        self.other_descriptors.append(info)

    def emit_header(self, outf):
        outf.write('#pragma once\n')
        outf.write('\n')
        outf.write('#include <avrpp/progmem.h>\n')
        outf.write('\n')
        outf.write('class UsbDescriptor;\n')
        outf.write('extern const UsbDescriptor PROGMEM usb_descriptors[];\n')
        outf.write('\n')

        # TODO: It would be nice to use something other than #define here.
        # An inline constexpr function that returns the value would perhaps be
        # nicer.
        for name, value in self.constants:
            outf.write('#define %s (%s)\n' % (name, value))

    def emit(self, outf):
        outf.write('#include <avrpp/progmem.h>\n')
        outf.write('#include <avrpp/usb_descriptors.h>\n')
        outf.write('#include <stdint.h>\n')
        outf.write('\n')
        outf.write('extern const UsbDescriptor PROGMEM usb_descriptors[];\n')
        outf.write('\n')

        emit_descriptor(outf, self.dev_descriptor, 'device_descriptor')

        for idx, cfg_desc in enumerate(self.configs):
            name = 'config%d_descriptor' % (idx + 1)
            emit_descriptor(outf, cfg_desc, name)

        for desc_info in self.other_descriptors:
            emit_descriptor(outf, desc_info.desc, desc_info.name)

        outf.write('\n')
        self.emit_strings(outf)

        outf.write('\n')
        self.emit_descriptors(outf)

    def emit_strings(self, outf):
        # Emit the list of supported language IDs
        # For now we only support US English
        langs = [LANG_ENGLISH_US]
        lang_desc_len = 2 + (2 * len(langs))
        outf.write('static const uint8_t PROGMEM language_ids[] = {\n')
        outf.write('    %d, 3,\n' % lang_desc_len)
        for lang in langs:
            outf.write('    %#04x, %#04x,\n' % ((lang & 0xff), (lang >> 8)))
        outf.write('};\n\n')

        # Emit the strings themselves
        for idx, s in enumerate(self.strings):
            name = 'string_%d' % (idx + 1)
            desc_len = 2 + (len(s) * 2)
            encoded = s.encode('utf-16le')

            outf.write('// %r\n' % s)
            outf.write('static const uint8_t PROGMEM %s[] = {\n' % name)
            outf.write('    %d, 3,\n' % desc_len)
            step = 8
            for n in range(0, len(encoded), step):
                current_slice = encoded[n:n+8]
                octets = ', '.join('%#04x' % byte for byte in current_slice)
                outf.write('    %s,\n' % octets)
            outf.write('};\n')

    def emit_descriptors(self, outf):
        outf.write('const UsbDescriptor PROGMEM usb_descriptors[] = {\n')
        outf.write('    {\n')
        outf.write('        0x0100, 0x0000,\n')
        outf.write('        device_descriptor, sizeof(device_descriptor),\n')
        outf.write('    },\n')

        for idx, cfg_desc in enumerate(self.configs):
            outf.write('    {\n')
            value = (0x0200 | idx)
            name = 'config%d_descriptor' % (idx + 1)
            outf.write('        %#06x, 0x0000,\n' % value)
            outf.write('        %s, sizeof(%s),\n' % (name, name))
            outf.write('    },\n')

        for info in self.other_descriptors:
            outf.write('    {\n')
            outf.write('        %#06x, %#06x,\n' % (info.value, info.index))
            outf.write('        %s, sizeof(%s),\n' % (info.name, info.name))
            outf.write('    },\n')

        outf.write('    {\n')
        outf.write('        0x0300, 0x0000,\n')
        outf.write('        language_ids, sizeof(language_ids),\n')
        outf.write('    },\n')

        lang_id = LANG_ENGLISH_US
        for idx, s in enumerate(self.strings):
            name = 'string_%d' % (idx + 1)
            value = (0x0300 | (idx + 1))
            outf.write('    {\n')
            outf.write('        %#06x, %#06x,\n' % (value, lang_id))
            outf.write('        %s, sizeof(%s),\n' % (name, name))
            outf.write('    },\n')

        outf.write('    { 0, 0, nullptr, 0 }\n')
        outf.write('};\n')


class DeviceDescriptor:
    def __init__(self,
                 usb_config,
                 endpoint0_size,
                 vendor_id,
                 product_id,
                 usb_version=(2, 0),
                 dev_class=0,
                 subclass=0,
                 protocol=0,
                 dev_version=(1, 0)):
        self.usb_config = usb_config
        self.descriptor_type = DT_DEVICE

        self.usb_version = usb_version
        self.dev_class = dev_class
        self.subclass = subclass
        self.protocol = protocol
        self.endpoint0_size = endpoint0_size
        self.vendor_id = vendor_id
        self.product_id = product_id
        self.dev_version = dev_version

        self.manufacturer = None
        self.product = None
        self.serial = None

    def serialize(self):
        num_configs = len(self.usb_config.configs)
        manufacturer_str_idx = self.usb_config.define_string(self.manufacturer)
        product_str_idx = self.usb_config.define_string(self.product)
        serial_str_idx = self.usb_config.define_string(self.serial)

        b = bytearray([
            0, self.descriptor_type,
            bcd(self.usb_version[1]), bcd(self.usb_version[0]),
            self.dev_class, self.subclass, self.protocol,
            self.endpoint0_size,
            (self.vendor_id & 0xff), (self.vendor_id >> 8),
            (self.product_id & 0xff), (self.product_id >> 8),
            bcd(self.dev_version[1]), bcd(self.dev_version[0]),
            manufacturer_str_idx, product_str_idx, serial_str_idx,
            num_configs,
        ])
        b[0] = len(b)
        return b


CONFIG_ATTR_RESERVED_HIGH = 0x80
CONFIG_ATTR_SELF_POWERED = 0x40
CONFIG_ATTR_REMOTE_WAKEUP = 0x20


class ConfigDescriptor:
    def __init__(self, number, descriptors=None):
        self.descriptor_type = DT_CONFIG

        self.config_value = number
        self.config_str_idx = 0
        self.attributes = CONFIG_ATTR_RESERVED_HIGH
        if descriptors is None:
            descriptors = []
        self.descriptors = descriptors

        # With all 5 LEDs lit, my keyboard draws around 32mA.
        # List the max power as 40mA just to be on the conservative side.
        self.max_power = 40 # in milliamps
        assert(self.max_power <= 100)

    def serialize(self):
        num_interfaces = 0
        for desc in self.descriptors:
            if desc.descriptor_type == DT_INTERFACE:
                num_interfaces += 1

        config_only = bytearray([
            0, self.descriptor_type,
            0, 0,  # Will be filled in with the total length below
            num_interfaces,
            self.config_value,
            self.config_str_idx,
            self.attributes,
            math.ceil(self.max_power / 2),
        ])
        config_only[0] = len(config_only)

        extra_serialized = [d.serialize() for d in self.descriptors]
        total = bytearray([]).join([config_only] + extra_serialized)

        total[2] = (len(total) & 0xff)
        total[3] = (len(total) >> 8)

        return total


class IfaceDescriptor:
    '''
    Interface descriptor

    Specified in section 9.6.5 of the USB 2.0 specification.
    '''
    def __init__(self, number,
                 iface_class,
                 subclass,
                 protocol,
                 endpoints,
                 alt_setting=0,
                 str_index=0):
        self.descriptor_type = DT_INTERFACE

        self.iface_num = number
        self.alt_setting = alt_setting
        self.endpoints = endpoints
        self.iface_class = iface_class
        self.subclass = subclass
        self.protocol = protocol
        self.iface_str_idx = str_index

    def serialize(self):
        b = bytearray([
            0, self.descriptor_type,
            self.iface_num,
            self.alt_setting,
            len(self.endpoints),
            self.iface_class,
            self.subclass,
            self.protocol,
            self.iface_str_idx,
        ])
        b[0] = len(b)
        return b


class EndpointDescriptor:
    '''
    Endpoint descriptor

    Specified in section 9.6.6 of the USB 2.0 specification.
    '''
    def __init__(self, address, attributes, max_packet_size, interval):
        self.descriptor_type = DT_ENDPOINT

        self.address = address
        self.attributes = attributes
        self.max_packet_size = max_packet_size
        self.interval = interval

    def serialize(self):
        b = bytearray([
            0, self.descriptor_type,
            self.address,
            self.attributes,
            (self.max_packet_size & 0xff), (self.max_packet_size >> 8),
            self.interval,
        ])
        b[0] = len(b)
        return b


class HidDescriptor:
    '''
    HID Descriptor

    Described in section 6.2.1 of the HID 1.11 specification.
    '''
    def __init__(self, descriptors,
                 hid_version=(1, 11),
                 country_code=0):
        self.descriptor_type = DT_HID

        self.hid_version = hid_version
        self.country_code = country_code
        self.descriptors = descriptors

    def serialize(self):
        b = bytearray([
            0, self.descriptor_type,
            bcd(self.hid_version[1]), bcd(self.hid_version[0]),
            self.country_code,
            len(self.descriptors),
        ])
        for desc in self.descriptors:
            desc_len = len(desc.serialize())
            desc_info = bytearray([
                desc.descriptor_type,
                (desc_len & 0xff), (desc_len >> 8)
            ])
            b += desc_info
        b[0] = len(b)
        return b


class HidReportDescriptor:
    '''
    HID Report Descriptor

    Described in section 6.2.2 of the HID 1.11 specification.
    '''
    def __init__(self, data):
        self.descriptor_type = DT_HID_REPORT
        self.data = data

    def serialize(self):
        return self.data


def emit_descriptor(outf, desc, name):
    data = desc.serialize()
    outf.write('static const uint8_t PROGMEM %s[%d] = {\n' % (name, len(data)))
    step = 8
    for n in range(0, len(data), step):
        current_slice = data[n:n+8]
        byte_values = ', '.join('%#04x' % byte for byte in current_slice)
        outf.write('    %s,\n' % byte_values)
    outf.write('};\n')



class AtomicFileWriter(object):
    def __init__(self, path, tmp_prefix=None, encoding='utf-8'):
        self.name = path
        output_dir, base = os.path.split(path)
        if tmp_prefix is None:
            tmp_prefix = base + '.'

        self.tmpf = NamedTemporaryFile(dir=output_dir, prefix=tmp_prefix,
                                       mode='w', encoding=encoding,
                                       delete=False)

    def __enter__(self):
        self.tmpf.__enter__()
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        tmp_name = self.tmpf.name
        result = self.tmpf.__exit__(exc_type, exc_value, exc_traceback)
        if result or exc_type is None:
            os.rename(tmp_name, self.name)
        else:
            os.unlink(tmp_name)
        return result

    def write(self, data):
        return self.tmpf.write(data)


def main(gen_config):
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument('-h', '--header',
                    help='The output header filename')
    ap.add_argument('output',
                    help='The output filename')
    ap.add_argument('-?', '--help',
                    action='help',
                    help='Show this help message and exit')
    args = ap.parse_args()

    print('Writing output to {}'.format(args.output))

    config = gen_config()

    output_dir, output_name = os.path.split(args.output)
    tmp_prefix = '.' + output_name + '.tmp.'
    with AtomicFileWriter(args.output) as tmpf, \
            AtomicFileWriter(args.header) as tmph:
        config.emit_header(tmph)
        config.emit(tmpf)
