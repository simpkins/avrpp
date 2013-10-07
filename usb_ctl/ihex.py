#!/usr/bin/python3 -tt
#
# Copyright (c) 2013, Adam Simpkins
#
'''
This module contains code for parsing Intel HEX formatted data.
http://en.wikipedia.org/wiki/Intel_HEX
'''
import binascii
import collections
import struct


class IntelHexError(Exception):
    def __init__(self, path, line_num, msg):
        self.path = path
        self.line_num = line_num
        self.msg = msg

        full_msg = '{}:{}: {}'.format(self.path, self.line_num, self.msg)
        super().__init__(full_msg)


Record = collections.namedtuple('Record', ('address', 'data', 'record_type',
                                           'line_number'))

REC_TYPE_DATA = 0
REC_TYPE_EOF = 1
REC_TYPE_EXT_SEGMENT_ADDRESS = 2
REC_TYPE_START_SEGMENT_ADDRESS = 3
REC_TYPE_EXT_LINEAR_ADDRESS = 4
REC_TYPE_START_LINEAR_ADDRESS = 5


class IHexData:
    def __init__(self):
        self.initial_cs = None
        self.initial_ip = None
        self.initial_eip = None

        # TODO: Get a better way of determining initial size
        # TODO: Use the bisect module to handle cases where we have a sparse
        # address space.
        self.buf = bytearray(130048)

    def set_data(self, addr, data):
        self.buf[addr:addr+len(data)] = data

    def get_data(self, addr, length):
        if addr >= len(self.buf):
            return None

        data = self.buf[addr:addr + length]
        if data == b'\x00' * len(data):
            return None
        if len(data) < length:
            data = data + (b'\x00' * (length - len(data)))
        return data


def parse_file(path):
    eof_seen = False
    extended_addr = 0
    result = IHexData()
    for rec in _parse_records(path):
        if eof_seen:
            raise IntelHexError(path, rec.line_number,
                                'additional data after EOF record')

        if rec.record_type == REC_TYPE_DATA:
            full_addr = extended_addr + rec.address
            result.set_data(full_addr, rec.data)
        elif rec.record_type == REC_TYPE_EOF:
            eof_seen = True
        elif rec.record_type == REC_TYPE_EXT_SEGMENT_ADDRESS:
            if rec.address != 0:
                msg = ('non-zero address ({:#x}) on extended segment '
                       'address record'.format(rec.address))
                raise IntelHexError(path, rec.line_number, msg)
            if len(rec.data) != 2:
                msg = ('unexpected data length for extended segment '
                       'address record')
                raise IntelHexError(path, rec.line_number, msg)
            (segment_addr,) = struct.unpack(b'>H', rec.data)
            extended_addr = (segment_addr << 4)
        elif rec.record_type == REC_TYPE_START_SEGMENT_ADDRESS:
            # This field specifies the initial values for the CS and IP
            # registers on x86 processors.
            if rec.address != 0:
                msg = ('non-zero address ({:#x}) on start segment '
                       'address record'.format(rec.address))
                raise IntelHexError(path, rec.line_number, msg)
            if len(rec.data) != 4:
                msg = ('unexpected data length for start segment '
                       'address record')
                raise IntelHexError(path, rec.line_number, msg)
            (cs_value, ip_value) = struct.unpack(b'>HH', rec.data)
            result.initial_cs = cs_value
            result.initial_ip = ip_value
        elif rec.record_type == REC_TYPE_EXT_LINEAR_ADDRESS:
            if rec.address != 0:
                msg = ('non-zero address ({:#x}) on extended linear '
                       'address record'.format(rec.address))
                raise IntelHexError(path, rec.line_number, msg)
            if len(rec.data) != 2:
                msg = ('unexpected data length for extended linear '
                       'address record')
                raise IntelHexError(path, rec.line_number, msg)
            linear_addr = struct.unpack(b'>H', rec.data)
            extended_addr = (linear_addr << 16)
        elif rec.record_type == REC_TYPE_START_LINEAR_ADDRESS:
            # This field specifies the initial value for the EIP
            # registers on an x86 processors.
            if rec.address != 0:
                msg = ('non-zero address ({:#x}) on start segment '
                       'address record'.format(rec.address))
                raise IntelHexError(path, rec.line_number, msg)
            if len(rec.data) != 4:
                msg = ('unexpected data length for start segment '
                       'address record')
                raise IntelHexError(path, rec.line_number, msg)
            (result.initial_eip) = struct.unpack(b'>I', rec.data)
        else:
            msg = 'unknown record type {}'.format(rec.record_type)
            raise IntelHexError(path, rec.line_number, msg)

    if not eof_seen:
        raise IntelHexError(path, 0, 'no EOF record found')

    return result


def _parse_records(path):
    with open(path, 'rb') as f:
        line_num = 0
        for line in f:
            line_num += 1
            record = _parse_line(line, path, line_num)
            yield record


def _parse_line(line, path, line_num):
    if not line.startswith(b':'):
        raise IntelHexError(path, line_num, 'invalid start code')

    byte_count = int(line[1:3], 16)
    address = int(line[3:7], 16)
    rec_type = int(line[7:9], 16)
    data_end = 9 + (2 * byte_count)
    data = binascii.unhexlify(line[9:data_end])
    checksum = int(line[data_end:data_end + 2], 16)
    end = line[data_end + 2:]
    if end != b'\r\n' and end != b'\n':
        raise IntelHexError(path, line_num, 'unexpected data')

    # Verify the checksum
    ck_bytes = binascii.unhexlify(line[1:data_end])
    cksum = 0
    for b in ck_bytes:
        cksum += b
    cksum = (0x100 - (cksum & 0xff)) & 0xff

    if cksum != checksum:
        msg = 'bad checksum: read={} calculated={}'.format(
                checksum, cksum)
        raise IntelHexError(path, line_num, msg)

    return Record(address=address, data=data, record_type=rec_type,
                  line_number=line_num)
