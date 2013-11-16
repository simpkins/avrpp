// Copyright (c) 2013, Adam Simpkins
#include <avrpp/avr_registers.h>
#include <avrpp/dbg_endpoint.h>
#include <avrpp/i2c.h>
#include <avrpp/i2c-defs.h>
#include <avrpp/log.h>
#include <avrpp/usb.h>
#include <avrpp/usb_descriptors.h>
#include <avrpp/util.h>

#include <avrpp/eeprom_ctl/usb_config.h>

#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>

F_LOG_LEVEL(2);

template<typename I2cBus>
void run_eeprom_read(I2cBus* bus) {
    FLOG(1, "Performing read\n");

    // We are timing sensitive, so interrupt processing
    cli();

    // uint8_t data_addr = 0x00;
    uint8_t value[1024];
    memset(value, 0, 1024);

    uint8_t stage = 0;
    I2cError ret;
    do {
        stage = 1;
        ret = bus->start();
        if (ret != I2cError::SUCCESS) {
            break;
        }

        // write page 0
        stage = 2;
        ret = bus->write(0xa8);
        if (ret != I2cError::SUCCESS) {
            break;
        }

        // byte 0
        stage = 3;
        ret = bus->write(0x00);
        if (ret != I2cError::SUCCESS) {
            break;
        }

        stage = 4;
        ret = bus->stop();
        if (ret != I2cError::SUCCESS) {
            break;
        }

        stage = 5;
        ret = bus->start();
        if (ret != I2cError::SUCCESS) {
            break;
        }

        // Read current address
        stage = 6;
        ret = bus->write(0xa9);
        if (ret != I2cError::SUCCESS) {
            break;
        }

        stage = 7;
        for (uint16_t n = 0; n < 1023; ++n) {
            ret = bus->read(&value[n]);
            if (ret != I2cError::SUCCESS) {
                break;
            }
        }

        stage = 8;
        ret = bus->read(&value[1023], false);
        if (ret != I2cError::SUCCESS) {
            break;
        }

        stage = 9;
        ret = bus->stop();
    } while (false);
    sei();

    if (ret == I2cError::SUCCESS) {
        FLOG(1, "data:\n");
        for (uint8_t page = 0; page < 64; ++page) {
            uint16_t n = page * 16;
            FLOG(1,
                 "%#02x:  "
                 "%02x %02x %02x %02x %02x %02x %02x %02x "
                 "  %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 page,
                 value[n], value[n + 1], value[n + 2], value[n + 3],
                 value[n + 4], value[n + 5], value[n + 6], value[n + 7],
                 value[n + 8], value[n + 9], value[n + 10], value[n + 11],
                 value[n + 12], value[n + 13], value[n + 14], value[n + 15]);
        };
    } else {
        FLOG(1, "ret: %#x\nstage: %d\n", static_cast<uint8_t>(ret), stage);
    }
}

template<typename I2cBus>
I2cError run_eeprom_write(I2cBus *bus) {
    uint8_t stage = 0;
    I2cError ret;

    // Write page 0, byte 1
    uint8_t dev_addr = 0xa8;
    uint8_t data_addr = 0x02;
    uint8_t value = 0x5a;
    uint16_t ack_loop = 0;

    FLOG(1, "Performing write\n");

    // We are timing sensitive, so interrupt processing
    cli();
    do {
        // Perform the write
        ret = bus->start();
        if (ret != I2cError::SUCCESS) {
            break;
        }
        stage = 2;

        ret = bus->write(dev_addr);
        if (ret != I2cError::SUCCESS) {
            break;
        }
        stage = 3;

        ret = bus->write(data_addr);
        if (ret != I2cError::SUCCESS) {
            break;
        }
        stage = 4;

        ret = bus->write(value);
        if (ret != I2cError::SUCCESS) {
            break;
        }
        stage = 5;

        ret = bus->stop();
        if (ret != I2cError::SUCCESS) {
            break;
        }
        stage = 7;

        // Acknowledge poll
        while (ack_loop < 10000) {
            ret = bus->write(dev_addr, true, true);
            if (ret == I2cError::NO_ACK) {
                ++ack_loop;
                continue;
            }
            break;
        }
    } while (false);
    sei();

    FLOG(1, "ret: %#x\nstage: %d\n", static_cast<uint8_t>(ret), stage);
    FLOG(1, "ack counter: %d\n", ack_loop);
    return ret;
}

void run_eeprom() {
    I2cBus<IoPin<PinF, 0>, IoPin<PinB, 0>> bus;

    cli();
    auto ret = bus.softwareReset();
    sei();
    if (ret != I2cError::SUCCESS) {
        FLOG(1, "failed to reset device\n");
        return;
    }

#if 0
    PORTC = 0x7f;
    ret = run_eeprom_write(&bus);
    if (ret != I2cError::SUCCESS) {
        return;
    }

    _delay_ms(100);
#endif

    run_eeprom_read(&bus);
}

int main() {
    // Set the clock speed as the first thing we do
    set_cpu_prescale();

    DebugIface dbg_if(DEBUG_INTERFACE, DEBUG_ENDPOINT, 4096, DEBUG_SIZE);
    set_log_putchar(DebugIface::putcharC, &dbg_if);
    FLOG(2, "Booting\n");

    // The A pins are not connected.
    // Set them as high output.
    DDRA = 0xff;
    PORTA = 0xff;

    // B0 is SCL
    // Other B pins are not connected
    DDRB = 0xfe;
    PORTB = 0xfe;

    // C0 is A2
    // C7 is WP  (high means write protected; low means writes allowed)
    // Other C pins are not connected
    DDRC = 0xff;
    PORTC = 0xff;

    // D6 is the onboard LED
    // Other D pins are not connected
    DDRD = 0xff;
    PORTD = 0xff;

    // E pins are not connected
    DDRE = 0xff;
    PORTE = 0xff;

    // F0 is SDA
    // Other F pins are not connected
    DDRF = 0xfe;
    PORTF = 0xfe;

    // Initialize USB
    auto usb = UsbController::singleton();
    usb->addInterface(&dbg_if);
    UsbDescriptorMap descriptors(pgm_cast(usb_descriptors));
    usb->init(ENDPOINT0_SIZE, &descriptors);
    // Enable interrupts
    sei();
    // Wait for USB configuration with the host to complete
    while (!usb->configured()) {
        // wait
    }

    // Do our stuff
    FLOG(1, "EEPROM controller initialized\n");
    run_eeprom();
    FLOG(1, "EEPROM logic complete\n");

    while (true) {
        // Blink the LED
        PORTD ^= 0x40;
        _delay_ms(200);
    }
}
