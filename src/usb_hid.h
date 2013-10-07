// Copyright (c) 2013, Adam Simpkins
#pragma once

enum HidRequestType : uint8_t {
    HID_GET_REPORT = 1,
    HID_GET_IDLE = 2,
    HID_GET_PROTOCOL = 3,
    HID_SET_REPORT = 9,
    HID_SET_IDLE = 10,
    HID_SET_PROTOCOL = 11,
};
