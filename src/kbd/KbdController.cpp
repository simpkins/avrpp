// Copyright (c) 2013, Adam Simpkins
#include <avrpp/kbd/KbdController.h>

#include <avrpp/avr_registers.h>
#include <avrpp/dbg_endpoint.h>
#include <avrpp/log.h>

#include <avr/sleep.h>
#include <util/delay.h>

F_LOG_LEVEL(2);

KbdController::KbdController(Keyboard *kbd, LedController *leds,
                             uint8_t iface_number, uint8_t endpoint_number)
    : _kbd(kbd),
      _leds(leds),
      _kbdIface(iface_number, endpoint_number) {
}

KbdController::~KbdController() {
    delete _dbgIface;
}

void KbdController::cfgDebugIface(uint8_t iface_number,
                                  uint8_t endpoint_number,
                                  uint16_t buf_len,
                                  uint8_t report_len) {
    if (_dbgIface) {
        return;
    }
    _dbgIface = new DebugIface(iface_number, endpoint_number,
                               buf_len, report_len);
    set_log_putchar(DebugIface::putcharC, _dbgIface);
    UsbController::singleton()->addInterface(_dbgIface);
}

void KbdController::init(uint8_t endpoint0_size,
                         pgm_ptr<UsbDescriptor> descriptors) {
    FLOG(2, "Keyboard booting\n");

    // TODO: Apply other power saving settings as recommended by
    // the at90usb1286 manual.
    // The HalfKay bootloader does seem to leave some features enabled that
    // consume power.  We draw a couple mA more when started via a HalfKay
    // reset than we do when restarted from a plain power on.
    //
    // PRR0 and PRR1 control some power reduction settings.

    // Initialize USB
    auto usb = UsbController::singleton();
    usb->setStateCallback(this);
    _kbdIface.setLedCallback(this);
    usb->addInterface(&_kbdIface);
    usb->init(endpoint0_size, descriptors);
    // Enable interrupts
    sei();
    // Wait for USB configuration with the host to complete
    waitForUsbInit(usb);
}

void KbdController::loop() {
    _kbd->loop(this);
}

void KbdController::waitForUsbInit(UsbController *usb) {
    while (!usb->configured()) {
        _delay_ms(1);
    }
}

void KbdController::onConfigured() {
    _leds->clearErrorLED();
}

void KbdController::onUnconfigured() {
    _leds->setErrorLED();
}

void KbdController::onSuspend() {
    auto led_state = _leds->suspendLEDs();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    AtomicGuard ag;
    if (UsbController::singleton()->suspended()) {
        sleep_enable();
        sei();
        sleep_cpu();
        sleep_disable();
    }
    _leds->restoreLEDs(led_state);
}

void KbdController::onWake() {
}

void KbdController::updateLeds(uint8_t led_value) {
    _leds->setKeyboardLEDs(led_value);
}

void KbdController::onChange(Keyboard* kbd) {
    uint8_t keys_len = KeyboardIface::MAX_KEYS;
    uint8_t pressed_keys[KeyboardIface::MAX_KEYS]{0};
    uint8_t modifier_mask{0};

    kbd->getState(&modifier_mask, pressed_keys, &keys_len);
    _kbdIface.update(pressed_keys, modifier_mask);
}
