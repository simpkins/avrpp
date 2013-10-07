// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avr/io.h>

#define DEFINE_REGISTER(reg_name, IntType) \
    enum class reg_name ## Flags : IntType; \
    static inline constexpr reg_name ## Flags operator|(reg_name ## Flags a, \
                                                        reg_name ## Flags b) { \
        return static_cast<reg_name ## Flags>(static_cast<IntType>(a) | \
                                              static_cast<IntType>(b)); \
    } \
    static inline constexpr reg_name ## Flags operator&(reg_name ## Flags a, \
                                                        reg_name ## Flags b) { \
        return static_cast<reg_name ## Flags>(static_cast<IntType>(a) & \
                                              static_cast<IntType>(b)); \
    } \
    static inline constexpr reg_name ## Flags operator~(reg_name ## Flags a) { \
        return static_cast<reg_name ## Flags>(~static_cast<IntType>(a)); \
    } \
    static inline void set_ ## reg_name(reg_name ## Flags) \
        __attribute__((always_inline)); \
    static inline void set_ ## reg_name(reg_name ## Flags flags) { \
        reg_name = static_cast<IntType>(flags); \
    } \
    static inline reg_name ## Flags get_ ## reg_name() \
        __attribute__((always_inline)); \
    static inline reg_name ## Flags get_ ## reg_name() { \
        return static_cast<reg_name ## Flags>(reg_name); \
    } \
    static inline bool isset_ ## reg_name(reg_name ## Flags) \
        __attribute__((always_inline)); \
    static inline bool isset_ ## reg_name(reg_name ## Flags flags) { \
        return static_cast<IntType>(get_##reg_name() & flags) != 0; \
    } \
    static inline bool isset(reg_name ## Flags, reg_name ## Flags) \
        __attribute__((always_inline)); \
    static inline bool isset(reg_name ## Flags v, reg_name ## Flags f) { \
        return (v & f) == f; \
    } \
    enum class reg_name ## Flags : IntType


DEFINE_REGISTER(PLLCSR, uint8_t) {
    NONE = 0x00,
    LOCK = 0x01,
    ENABLE = 0x02,
    PRESCALER_4 = 0x0C,
    PRESCALER_8_AT90USB64x = 0x18,
    PRESCALER_8_AT90USB128x = 0x14,
};

DEFINE_REGISTER(UHWCON, uint8_t) {
    NONE = 0x00,
    ENABLE_PADS_REGULATOR = 0x01,
    // 0x02, 0x04, and 0x08 are reserved
    ENABLE_UVCON_PIN_CONTROL = 0x10,
    // 0x20 is reserved
    ENABLE_UID_PIN = 0x40,
    DEVICE_MODE = 0x80,
};

DEFINE_REGISTER(USBCON, uint8_t) {
    NONE = 0x00,
    ENABLE_VBUS_TRANSITION_INTR = 0x01,
    ENABLE_ID_TRANSITION_INTR = 0x02,
    // 0x40 and 0x80 are reserved
    ENABLE_OTG_PAD = 0x10,
    FREEZE_CLOCK = 0x20,
    MODE_HOST = 0x40,
    ENABLE = 0x80,
};

DEFINE_REGISTER(UDCON, uint8_t) {
    NONE = 0x00,
    DETACH_DEVICE = 0x01,
    REMOTE_WAKE_UP = 0x02,
    LOW_SPEED_MODE = 0x04,
};

DEFINE_REGISTER(UDIEN, uint8_t) {
    NONE = 0x00,
    SUSPEND = 0x01,
    // 0x02 is reserved
    START_OF_FRAME = 0x04,
    END_OF_RESET = 0x08,
    WAKE_UP = 0x10,
    END_OF_RESUME = 0x20,
    UPSTREAM_RESUME = 0x40,
    // 0x80 is reserved
};

DEFINE_REGISTER(UDINT, uint8_t) {
    NONE = 0x00,
    SUSPEND = 0x01,
    // 0x02 is reserved
    START_OF_FRAME = 0x04,
    END_OF_RESET = 0x08,
    WAKE_UP = 0x10,
    END_OF_RESUME = 0x20,
    UPSTREAM_RESUME = 0x40,
    // 0x80 is reserved
};

DEFINE_REGISTER(UECONX, uint8_t) {
    NONE = 0x00,
    ENABLE = 0x01,
    RESET_DATA_TOGGLE = 0x08,
    STALL_REQUEST_CLEAR = 0x10,
    STALL_REQUEST = 0x20,
};

DEFINE_REGISTER(UECFG0X, uint8_t) {
    DIRECTION_IN = 0x01,
    // Upper 2 bits select the endpoint type
    CONTROL = 0x00,
    ISOCHRONOUS = 0x40,
    BULK = 0x80,
    INTERRUPT = 0xc0,
};

DEFINE_REGISTER(UECFG1X, uint8_t) {
    NONE = 0x00,
    CFG1_ALLOC = 0x02,
    SINGLE_BANK = 0x00,
    DOUBLE_BANK = 0x04,
    SIZE_8 = 0x00,
    SIZE_16 = 0x10,
    SIZE_32 = 0x20,
    SIZE_64 = 0x30,
    SIZE_128 = 0x40,
    SIZE_256 = 0x50,
};

// Run-time computed version of usb_cfg_size()
static inline constexpr UECFG1XFlags usb_cfg_size(uint16_t size) {
    return (size == 8 ? UECFG1XFlags::SIZE_8 :
            (size == 16 ? UECFG1XFlags::SIZE_16 :
             (size == 32 ? UECFG1XFlags::SIZE_32 :
              (size == 64 ? UECFG1XFlags::SIZE_64 :
               (size == 128 ? UECFG1XFlags::SIZE_128 :
                (size == 256 ? UECFG1XFlags::SIZE_256 :
                 UECFG1XFlags::NONE)))))); // Bad param
}
// Compile-time version of usb_cfg_size()
// This will generate a compile error if called with a bad parameter.
template<uint16_t SIZE>
static inline constexpr UECFG1XFlags usb_cfg_size() {
    static_assert(SIZE == 8 || SIZE == 16 || SIZE == 32 ||
                  SIZE == 64 || SIZE == 128 || SIZE == 256,
                  "Invalid USB endpoint size");
    return usb_cfg_size(SIZE);
}

DEFINE_REGISTER(UEINTX, uint8_t) {
    TX_READY = 0x01,
    STALLED = 0x02,
    RX_OUT = 0x04,
    KILL_IN = 0x04,
    RX_SETUP = 0x08,
    NAK_OUT = 0x10,
    RW_ALLOWED = 0x20,
    NAK_IN = 0x40,
    FIFO_CONTROL = 0x80,
};

DEFINE_REGISTER(UEIENX, uint8_t) {
    TX_READY = 0x01,
    STALLED = 0x02,
    RX_OUT = 0x04,
    RX_SETUP = 0x08,
    NAK_OUT = 0x10,
    NAK_IN = 0x40,
    FLOW_ERROR = 0x80,
};

// Undefine our helper DEFINE_REGISTER macro, so it doesn't pollute
// the namespace of files that include us.
#undef DEFINE_REGISTER
