#!/usr/bin/python3 -tt
#
# Copyright (c) 2013, Adam Simpkins
#
'''
A simple ctypes wrapper around libusbx.

(We implement our own version here primarily since pyusb isn't packaged for
many distros, especially for python 3.x.)
'''
import contextlib
import ctypes
import ctypes.util
import functools
import time
import struct

from ctypes import POINTER, byref

import usb
import hid


_libusb = None
_ctx = None

c_ctx = ctypes.c_void_p
c_device = ctypes.c_void_p
c_device_handle = ctypes.c_void_p
c_device_list = POINTER(c_device)
c_transfer_callback = ctypes.CFUNCTYPE(None, ctypes.c_void_p)


class c_timeval(ctypes.Structure):
    _fields_ = [
        ('tv_sec', ctypes.c_long),
        ('tv_usec', ctypes.c_long),
    ]


class c_transfer(ctypes.Structure):
    _fields_ = [
        ('dev_handle', c_device_handle),
        ('flags', ctypes.c_uint8),
        ('endpoint', ctypes.c_ubyte),
        ('type', ctypes.c_ubyte),
        ('timeout', ctypes.c_uint),
        # Unfortunately status is an enum type.
        # This assumes your compiler uses the full integer width for it.
        # (Even if it doesn't, it should add padding, which will still behave
        # well enough for our purposes, except on big-endian machines.)
        ('status', ctypes.c_int),
        ('length', ctypes.c_int),
        ('actual_length', ctypes.c_int),
        ('callback', c_transfer_callback),
        ('user_data', ctypes.c_void_p),
        ('buffer', POINTER(ctypes.c_ubyte)),
        ('num_iso_packets', ctypes.c_int),
        # We don't bother dealing with the iso_packet_desc array for now.
    ]

c_transfer_p = POINTER(c_transfer)

TRANSFER_TYPE_CONTROL = 0
TRANSFER_TYPE_ISOCHRONOUS = 1
TRANSFER_TYPE_BULK = 2
TRANSFER_TYPE_INTERRUPT = 3


def init():
    global _libusb
    global _ctx

    libname = ctypes.util.find_library('usb-1.0')
    _libusb = ctypes.cdll.LoadLibrary(libname)

    _libusb.libusb_init.argtypes = (POINTER(c_ctx),)
    _libusb.libusb_init.restype = ctypes.c_int
    _libusb.libusb_set_debug.argtypes = (POINTER(c_ctx), ctypes.c_int)
    _libusb.libusb_set_debug.restype = None
    _libusb.libusb_get_device_list.argtypes = (
            c_ctx, POINTER(c_device_list))
    _libusb.libusb_get_device_list.restype = ctypes.c_ssize_t
    _libusb.libusb_get_device_descriptor.argtypes = (
            c_device, POINTER(DeviceDescriptor))
    _libusb.libusb_get_device_descriptor.restype = ctypes.c_int
    _libusb.libusb_get_active_config_descriptor.argtypes = (
            c_device, POINTER(POINTER(c_config_descriptor)))
    _libusb.libusb_get_active_config_descriptor.restype = ctypes.c_int
    _libusb.libusb_get_config_descriptor.argtypes = (
            c_device, ctypes.c_uint8, POINTER(POINTER(c_config_descriptor)))
    _libusb.libusb_get_config_descriptor.restype = ctypes.c_int
    _libusb.libusb_free_config_descriptor.argtypes = (
            POINTER(c_config_descriptor),)
    _libusb.libusb_free_config_descriptor.restype = None
    _libusb.libusb_open.argtypes = (c_ctx, POINTER(c_device_handle))
    _libusb.libusb_open.restype = ctypes.c_int
    _libusb.libusb_open_device_with_vid_pid.argtypes = (
            c_ctx, ctypes.c_uint16, ctypes.c_uint16)
    _libusb.libusb_open_device_with_vid_pid.restype = c_device_handle
    _libusb.libusb_get_bus_number.argtypes = (c_device,)
    _libusb.libusb_get_bus_number.restype = ctypes.c_uint8
    _libusb.libusb_get_port_number.argtypes = (c_device,)
    _libusb.libusb_get_port_number.restype = ctypes.c_uint8
    _libusb.libusb_get_device_address.argtypes = (c_device,)
    _libusb.libusb_get_device_address.restype = ctypes.c_uint8

    _libusb.libusb_kernel_driver_active.argtypes = (
            c_device_handle, ctypes.c_int)
    _libusb.libusb_kernel_driver_active.restype = ctypes.c_int
    _libusb.libusb_detach_kernel_driver.argtypes = (
            c_device_handle, ctypes.c_int)
    _libusb.libusb_detach_kernel_driver.restype = ctypes.c_int
    _libusb.libusb_attach_kernel_driver.argtypes = (
            c_device_handle, ctypes.c_int)
    _libusb.libusb_attach_kernel_driver.restype = ctypes.c_int
    _libusb.libusb_claim_interface.argtypes = (
            c_device_handle, ctypes.c_int)
    _libusb.libusb_claim_interface.restype = ctypes.c_int
    _libusb.libusb_release_interface.argtypes = (
            c_device_handle, ctypes.c_int)
    _libusb.libusb_release_interface.restype = ctypes.c_int
    _libusb.libusb_set_interface_alt_setting.argtypes = (
            c_device_handle, ctypes.c_int, ctypes.c_int)
    _libusb.libusb_set_interface_alt_setting.restype = ctypes.c_int

    _libusb.libusb_interrupt_transfer.argtypes = (
            c_device_handle, ctypes.c_ubyte, POINTER(ctypes.c_ubyte),
            ctypes.c_int, POINTER(ctypes.c_int), ctypes.c_uint)
    _libusb.libusb_interrupt_transfer.restype = ctypes.c_int
    _libusb.libusb_alloc_transfer.argtypes = (ctypes.c_int,)
    _libusb.libusb_alloc_transfer.restype = c_transfer_p
    _libusb.libusb_free_transfer.argtypes = (c_transfer_p,)
    _libusb.libusb_free_transfer.restype = None
    _libusb.libusb_submit_transfer.argtypes = (c_transfer_p,)
    _libusb.libusb_submit_transfer.restype = ctypes.c_int
    _libusb.libusb_cancel_transfer.argtypes = (c_transfer_p,)
    _libusb.libusb_cancel_transfer.restype = ctypes.c_int
    _libusb.libusb_handle_events_timeout_completed.argtypes = (
            c_ctx, POINTER(c_timeval), POINTER(ctypes.c_int))
    _libusb.libusb_handle_events_timeout_completed.restype = ctypes.c_int

    _ctx = ctypes.c_void_p()
    _libusb.libusb_init(byref(_ctx))
    return _ctx


def _ensure_init():
    if _ctx is None:
        init()


def cleanup():
    global _libusb
    global _ctx

    if _ctx is None:
        return

    _libusb.libusb_exit(_ctx)

    _ctx = None
    _libusb = None


ERROR_SUCCESS = 0
ERROR_IO = -1
ERROR_INVALID_PARAM = -2
ERROR_ACCESS = -3
ERROR_NO_DEVICE = -4
ERROR_NOT_FOUND = -5
ERROR_BUSY = -6
ERROR_TIMEOUT = -7
ERROR_OVERFLOW = -8
ERROR_PIPE = -9
ERROR_INTERRUPTED = -10
ERROR_NO_MEM = -11
ERROR_NOT_SUPPORTED = -12
ERROR_OTHER = -99

_ERROR_STRS = {
    ERROR_SUCCESS: 'Success',
    ERROR_IO: 'Input/output error',
    ERROR_INVALID_PARAM: 'Invalid parameter',
    ERROR_ACCESS: 'Access denied',
    ERROR_NO_DEVICE: 'No such device',
    ERROR_NOT_FOUND: 'Entity not found',
    ERROR_BUSY: 'Resource busy',
    ERROR_TIMEOUT: 'Operation timed out',
    ERROR_OVERFLOW: 'Overflow',
    ERROR_PIPE: 'Pipe error',
    ERROR_INTERRUPTED: 'System call interrupted',
    ERROR_NO_MEM: 'Insufficient memory',
    ERROR_NOT_SUPPORTED: 'Operation not supported',
    ERROR_OTHER: 'Other error',
}


TRANSFER_COMPLETED = 0
TRANSFER_ERROR = 1
TRANSFER_TIMED_OUT = 2
TRANSFER_CANCELLED = 3
TRANSFER_STALL = 4
TRANSFER_NO_DEVICE = 5
TRANSFER_OVERFLOW = 6

_TRANSFER_STATUS_TO_ERR = {
    TRANSFER_COMPLETED: ERROR_SUCCESS,
    TRANSFER_ERROR: ERROR_IO,
    TRANSFER_TIMED_OUT: ERROR_TIMEOUT,
    TRANSFER_CANCELLED: ERROR_IO,
    TRANSFER_STALL: ERROR_PIPE,
    TRANSFER_NO_DEVICE: ERROR_NO_DEVICE,
    TRANSFER_OVERFLOW: ERROR_OVERFLOW,
}


def _err_from_transfer_status(status):
    return _TRANSFER_STATUS_TO_ERR.get(status, -99)


def get_error_string(code):
    error_msg = _ERROR_STRS.get(code)
    if error_msg is None:
        return 'unknown error %r' % (code,)
    return error_msg


class UsbError(Exception):
    pass


class LibusbError(UsbError):
    def __init__(self, code, msg, *args, **kwargs):
        self.code = code
        if args or kwargs:
            msg = msg.format(*args, **kwargs)
        full_msg = '{}: {}'.format(msg, get_error_string(code))
        super().__init__(full_msg)


def _simple_memo(fn):
    cache = {}
    @functools.wraps(fn)
    def wrapper(self):
        if self not in cache:
            cache[self] = fn(self)
        return cache[self]
    return wrapper


class Device:
    def __init__(self, device):
        self.device = device
        _libusb.libusb_ref_device(device)
        self._dev_descriptor = None

    def __del__(self):
        _libusb.libusb_unref_device(self.device)

    def get_handle(self):
        handle = c_device_handle()
        err = _libusb.libusb_open(self.device, byref(handle))
        if err != 0:
            raise LibusbError(err, 'failed to open device')
        return DeviceHandle(self, handle)

    @property
    @_simple_memo
    def device_descriptor(self):
        desc = DeviceDescriptor()
        err = _libusb.libusb_get_device_descriptor(self.device, byref(desc))
        if err != 0:
            raise LibusbError(err, 'failed to get device descriptor')
        return desc

    def get_active_config_descriptor(self):
        desc = POINTER(c_config_descriptor)()
        err = _libusb.libusb_get_active_config_descriptor(self.device,
                                                          byref(desc))
        if err != 0:
            raise LibusbError(err, 'failed to get active config descriptor')
        try:
            return ConfigDescriptor(desc.contents)
        finally:
            _libusb.libusb_free_config_descriptor(desc)

    def get_config_descriptor(self, idx):
        desc = c_config_descriptor()
        err = _libusb.libusb_get_config_descriptor(self.device, idx,
                                                   byref(desc))
        if err != 0:
            raise LibusbError(err, 'failed to get active config descriptor')
        return desc

    @property
    @_simple_memo
    def bus(self):
        return _libusb.libusb_get_bus_number(self.device)

    @property
    @_simple_memo
    def port(self):
        return _libusb.libusb_get_port_number(self.device)

    @property
    @_simple_memo
    def address(self):
        return _libusb.libusb_get_device_address(self.device)


class DeviceHandle:
    def __init__(self, device, handle):
        self.device = device
        self.handle = handle

    def __del__(self):
        if self.handle is not None:
            _libusb.libusb_close(self.handle)
            self.handle = None

    @contextlib.contextmanager
    def interface(self, idx, force=True):
        cfg = self.device.get_active_config_descriptor()
        if not (0 <= idx < len(cfg.interfaces)):
            raise IndexError('invalid interface index')
        if_cfg = cfg.interfaces[idx]

        reattach = False
        if force:
            # Detach any existing driver before we attach
            kernel_active = _libusb.libusb_kernel_driver_active(self.handle,
                                                                idx)
            if kernel_active < 0:
                raise LibusbError(kernel_active,
                                  'failed to determine kernel driver state')
            elif kernel_active:
                err = _libusb.libusb_detach_kernel_driver(self.handle, idx)
                if err != 0:
                    raise LibusbError(err, 'failed to detach kernel driver')
                reattach = True

        err = _libusb.libusb_claim_interface(self.handle, idx)
        if err != 0:
            raise LibusbError(err, 'failed to claim interface')

        try:
            yield InterfaceHandle(self, idx, if_cfg)
        finally:
            err = _libusb.libusb_release_interface(self.handle, idx)
            if err in (ERROR_NO_DEVICE, ERROR_OTHER):
                # ERROR_NO_DEVICE occurs if the device has been disconnected
                # ERROR_OTHER occurs when another process has claimed the
                # interface, so we no longer own it.
                return
            if err != 0:
                raise LibusbError(err, 'failed to release interface')
            if reattach:
                # Note that the re-attach attempt
                # can block for several seconds
                err = _libusb.libusb_attach_kernel_driver(self.handle, idx)
                # When the device has just been disconnected,
                # libusb_release_interface() may still succeed, but
                # libusb_attach_kernel_driver() will fail with ERROR_NOT_FOUND.
                if err != 0 and err != ERROR_NOT_FOUND:
                    raise LibusbError(err, 'failed to re-attach kernel driver')

    def control_endpoint(self):
        return ControlEndpoint(self)

    def close(self):
        if self.handle is None:
            raise Exception('not open')
        _libusb.libusb_close(self.handle)
        self.handle = None


class InterfaceHandle:
    def __init__(self, handle, idx, cfg):
        self.handle = handle
        self.idx = idx
        self.cfg = cfg

        # libusb_claim_interface() always resets the interface to the
        # first alternate setting.
        self.current_setting = self.cfg.settings[0]

    def set_alt_setting(self, value):
        for setting in self.cfg.settings:
            if setting.bAlternateSetting == value:
                break
        else:
            raise LibusbError(ERROR_NOT_FOUND,
                              'invalid alternate setting value')

        err = _libusb.libusb_set_interface_alt_setting(
            self.handle.handle, self.idx, value)
        if err != 0:
            raise LibusbError(err, 'failed to select alternate setting')
        self.current_setting = setting

    def get_endpoint(self, number):
        for ep_desc in self.current_setting.endpoints:
            if (ep_desc.bEndpointAddress & 0xf) == number:
                break
        else:
            raise LibusbError(ERROR_NOT_FOUND, 'invalid endpoint number')

        if (ep_desc.bEndpointAddress & usb.ENDPOINT_IN):
            return InEndpoint(self, ep_desc)
        else:
            return OutEndpoint(self, ep_desc)
        return EndpointHandle(self, ep_desc)


class EndpointHandle:
    def __init__(self, iface, cfg):
        if iface is None:
            # control endpoint.  The ControlEndpoint() __init__ function
            # will set self.handle
            self.iface = None
            self.handle = None
        else:
            self.iface = iface
            self.handle = iface.handle
        self.cfg = cfg

    def _libusb_handle(self):
        return self.handle.handle

    def _get_timeout_ms(self, timeout):
        if timeout is None:
            timeout_ms = 0
            end_time = None
        elif timeout == 0:
            # Unfortunately libusb uses 0 to represent "no timeout",
            # so they don't have a good mechanism to indicate a non-blocking
            # transfer.  (We can pass a 0 timeout in to
            # libusb_handle_events_timeout_completed(), but if the transfer
            # doesn't complete we have to cancel it, and this appears to take
            # over 1ms anyway just to cancel it.)
            timeout_ms = 1
            end_time = time.time()
        else:
            timeout_ms = int(timeout * 1000)
            end_time = time.time() + timeout

        return (timeout_ms, end_time)

    def _perform_transfer(self, buf, trans_type, timeout):
        (timeout_ms, end_time) = self._get_timeout_ms(timeout)

        complete = ctypes.c_int(0)
        def callback(transfer):
            complete.value = 1

        transfer = _libusb.libusb_alloc_transfer(0)
        try:
            transfer.contents.dev_handle = self._libusb_handle()
            if self.cfg is None:
                # Control endpoint
                transfer.contents.endpoint = 0
                direction = (buf[0] & 0x80)
            else:
                transfer.contents.endpoint = self.cfg.bEndpointAddress
                direction = (self.cfg.bEndpointAddress & 0x80)
            transfer.contents.type = trans_type
            transfer.contents.timeout = timeout_ms
            transfer.contents.buffer = buf
            transfer.contents.length = len(buf)
            transfer.contents.callback = c_transfer_callback(callback)

            err = _libusb.libusb_submit_transfer(transfer)
            if err != 0:
                raise LibusbError(err, 'failed to submit async transfer')

            # wait for completion
            tv = c_timeval()
            while complete.value == 0:
                # The transfer timeout is sufficient for our purposes.
                # We just default to 1 minute here.  If this is too long
                # the transfer timeout will fire first.  If it isn't long
                # enough and the transfer is still incomplete we will
                # simply loop again.
                tv.tv_sec = 60
                tv.tv_usec = 0
                err = _libusb.libusb_handle_events_timeout_completed(
                        _ctx, byref(tv), byref(complete))
                if err != 0:
                    raise LibusbError(err, 'error handling events')

            status = transfer.contents.status
            if status != TRANSFER_COMPLETED:
                err = _err_from_transfer_status(status)
                raise LibusbError(err, 'error performing input transfer')

            if direction == usb.ENDPOINT_IN:
                len_read = transfer.contents.actual_length
                result = bytearray(len_read)
                for n in range(len_read):
                    result[n] = transfer.contents.buffer[n]
                return result
            else:
                return
        finally:
            if not complete.value:
                _libusb.libusb_cancel_transfer(transfer)
            _libusb.libusb_free_transfer(transfer)


class InEndpoint(EndpointHandle):
    def read(self, size=None, timeout=0.1):
        if size is None:
            size = self.cfg.wMaxPacketSize
        buf = (ctypes.c_ubyte * size)()

        return self._perform_transfer(buf, trans_type=TRANSFER_TYPE_INTERRUPT,
                                      timeout=timeout)


class OutEndpoint(EndpointHandle):
    pass


class ControlEndpoint(EndpointHandle):
    def __init__(self, handle):
        super().__init__(None, None)
        self.handle = handle

    def hid_set_feature(self, request, interface, report_id=0, timeout=0.1):
        return self.hid_set_report(request=request, interface=interface,
                                   report_type=hid.REPORT_TYPE_FEATURE,
                                   report_id=report_id, timeout=timeout)

    def hid_set_report(self, request, interface,
                       report_type=hid.REPORT_TYPE_OUTPUT,
                       report_id=0, timeout=0.1):
        if isinstance(interface, InterfaceHandle):
            interface = interface.idx
        if (report_type & 0xff00) != report_type:
            raise ValueError('Invalid report type %r' % (report_type,))
        if not (0 <= report_id <= 0xff):
            raise ValueError('Invalid report ID %r' % (report_id,))
        request_type = (usb.ENDPOINT_OUT | usb.REQUEST_TYPE_CLASS |
                        usb.RECIPIENT_INTERFACE)
        wValue = (report_type | report_id)
        self.setup_request(request=request,
                           bmRequestType=request_type,
                           bRequest=hid.SET_REPORT,
                           wValue=wValue,
                           wIndex=interface,
                           timeout=timeout)

    def setup_request(self, request, bmRequestType, bRequest, wValue, wIndex,
                      timeout=0.1):
        CONTROL_SETUP_SIZE = 8
        full_input_len = CONTROL_SETUP_SIZE + len(request)

        buf = (ctypes.c_ubyte * full_input_len)()
        struct.pack_into('<BBHHH', buf, 0,
                         bmRequestType, bRequest, wValue, wIndex, len(request))
        for n in range(len(request)):
            buf[n + CONTROL_SETUP_SIZE] = request[n]

        self._perform_transfer(buf, trans_type=TRANSFER_TYPE_CONTROL,
                               timeout=timeout)


class DeviceDescriptor(ctypes.Structure):
    _fields_ = [
        ('bLength', ctypes.c_uint8),
        ('bDescriptorType', ctypes.c_uint8),
        ('bcdUSB', ctypes.c_uint16),
        ('bDeviceClass', ctypes.c_uint8),
        ('bDeviceSubClass', ctypes.c_uint8),
        ('bDeviceProtocol', ctypes.c_uint8),
        ('bMaxPacketSize0', ctypes.c_uint8),
        ('idVendor', ctypes.c_uint16),
        ('idProduct', ctypes.c_uint16),
        ('bcdDevice', ctypes.c_uint16),
        ('iManufacturer', ctypes.c_uint8),
        ('iProduct', ctypes.c_uint8),
        ('iSerialNumber', ctypes.c_uint8),
        ('bNumConfigurations', ctypes.c_uint8),
    ]


class c_endpoint_descriptor(ctypes.Structure):
    _fields_ = [
        ('bLength', ctypes.c_uint8),
        ('bDescriptorType', ctypes.c_uint8),
        ('bEndpointAddress', ctypes.c_uint8),
        ('bmAttributes', ctypes.c_uint8),
        ('wMaxPacketSize', ctypes.c_uint16),
        ('bInterval', ctypes.c_uint8),
        ('bRefresh', ctypes.c_uint8),
        ('bSynchAddress', ctypes.c_uint8),
        ('extra', POINTER(ctypes.c_ubyte)),
        ('extra_length', ctypes.c_int),
    ]


class c_interface_descriptor(ctypes.Structure):
    _fields_ = [
        ('bLength', ctypes.c_uint8),
        ('bDescriptorType', ctypes.c_uint8),
        ('bInterfaceNumber', ctypes.c_uint8),
        ('bAlternateSetting', ctypes.c_uint8),
        ('bNumEndpoints', ctypes.c_uint8),
        ('bInterfaceClass', ctypes.c_uint8),
        ('bInterfaceSubClass', ctypes.c_uint8),
        ('bInterfaceProtocol', ctypes.c_uint8),
        ('iInterface', ctypes.c_uint8),
        ('endpoint', POINTER(c_endpoint_descriptor)),
        ('extra', POINTER(ctypes.c_ubyte)),
        ('extra_length', ctypes.c_int),
    ]


class c_interface(ctypes.Structure):
    _fields_ = [
        ('altsetting', POINTER(c_interface_descriptor)),
        ('num_altsetting', ctypes.c_int),
    ]


class c_config_descriptor(ctypes.Structure):
    _fields_ = [
        ('bLength', ctypes.c_uint8),
        ('bDescriptorType', ctypes.c_uint8),
        ('wTotalLength', ctypes.c_uint16),
        ('bNumInterfaces', ctypes.c_uint8),
        ('bConfigurationValue', ctypes.c_uint8),
        ('iConfiguration', ctypes.c_uint8),
        ('bmAttributes', ctypes.c_uint8),
        ('MaxPower', ctypes.c_uint8),
        ('interface', POINTER(c_interface)),
        ('extra', POINTER(ctypes.c_ubyte)),
        ('extra_length', ctypes.c_int),
    ]


def _parse_extra(desc):
    extra = bytearray(desc.extra_length)
    for n in range(desc.extra_length):
        extra[n] = desc.extra[n]
    return extra


class ConfigDescriptor:
    def __init__(self, desc):
        self.bLength = desc.bLength
        self.bDescriptorType = desc.bDescriptorType
        self.wTotalLength = desc.wTotalLength
        self.bNumInterfaces = desc.bNumInterfaces
        self.bConfigurationValue = desc.bConfigurationValue
        self.iConfiguration = desc.iConfiguration
        self.bmAttributes = desc.bmAttributes
        self.MaxPower = desc.MaxPower
        self.extra = _parse_extra(desc)

        self.interfaces = []
        for idx in range(desc.bNumInterfaces):
            iface_info = InterfaceInfo(desc.interface[idx])
            self.interfaces.append(iface_info)


class InterfaceInfo:
    def __init__(self, desc):
        self.settings = []
        for idx in range(desc.num_altsetting):
            iface = InterfaceDescriptor(desc.altsetting[idx])
            self.settings.append(iface)


class InterfaceDescriptor:
    def __init__(self, desc):
        self.bLength = desc.bLength
        self.bDescriptorType = desc.bDescriptorType
        self.bInterfaceNumber = desc.bInterfaceNumber
        self.bAlternateSetting = desc.bAlternateSetting
        self.bNumEndpoints = desc.bNumEndpoints
        self.bInterfaceClass = desc.bInterfaceClass
        self.bInterfaceSubClass = desc.bInterfaceSubClass
        self.bInterfaceProtocol = desc.bInterfaceProtocol
        self.iInterface = desc.iInterface
        self.extra = _parse_extra(desc)

        self.endpoints = []
        for idx in range(desc.bNumEndpoints):
            endpoint = EndpointDescriptor(desc.endpoint[idx])
            self.endpoints.append(endpoint)


class EndpointDescriptor:
    def __init__(self, desc):
        self.bLength = desc.bLength
        self.bDescriptorType = desc.bDescriptorType
        self.bEndpointAddress = desc.bEndpointAddress
        self.bmAttributes = desc.bmAttributes
        self.wMaxPacketSize = desc.wMaxPacketSize
        self.bInterval = desc.bInterval
        self.bRefresh = desc.bRefresh
        self.bSynchAddress = desc.bSynchAddress
        self.extra = _parse_extra(desc)


def list_devices():
    _ensure_init()

    dev_list = c_device_list()
    num_devices = _libusb.libusb_get_device_list(_ctx, byref(dev_list))
    try:
        if num_devices < 0:
            raise UsbError('error calling libusb_get_device_list()')
        for n in range(num_devices):
            dev = c_device(dev_list[n])
            yield Device(dev)
    finally:
        _libusb.libusb_free_device_list(dev_list, 1)


def find_device(vendor_id, product_id, default=LibusbError):
    for dev in list_devices():
        desc = dev.device_descriptor
        if desc.idVendor == vendor_id and desc.idProduct == product_id:
            return dev

    if default is LibusbError:
        raise LibusbError(ERROR_NO_DEVICE, 'device not found')
    return default


def open_device(vendor_id, product_id):
    # libusb does provide a libusb_open_device_with_vid_pid() function,
    # but it doesn't give very useful error information, so just implement it
    # ourselves.
    dev = find_device(vendor_id, product_id)
    return dev.get_handle()
