// Copyright (c) 2013, Adam Simpkins
#pragma once

#include "pin.h"

#include <util/delay.h>

enum class I2cError : uint8_t {
    SUCCESS = 0,
    CLOCK_TIMEOUT,
    ARBITRATION_LOST,
    NO_ACK,

    NOT_STARTED
};

/**
 * An I2C bus controller using the specified SDA and SCL pins.
 *
 * TODO: The at90usb128x includes a built-in I2C module using
 * pin D0 as SCL and D1 as SDA.  We should probably provide a specialized
 * instantiation of I2cBus with these pins that uses the built-in TWI module,
 * rather than this bit-banging code.
 */
template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US = 10,
         uint16_t CLOCK_TIMEOUT = 50000>
class I2cBus {
  public:
    I2cBus();

    /**
     * Signal a start condition on the bus.
     */
    I2cError start();

    /**
     * Signal a start condition, and then send the specified device address.
     *
     * Returns I2cError::NO_ACK if no slave node acknowledges this address.
     */
    I2cError start(uint8_t deviceAddress);

    /**
     * Signal a stop condition on the bus.
     */
    I2cError stop();

    /**
     * Send a reset signal.
     *
     * This can be used to reset the bus after any interruption in protocol.
     */
    I2cError softwareReset();

    /**
     * Write a byte.
     *
     * The bus must have been started.
     *
     * Returns I2cError::SUCCESS if the byte was written successfully and
     * acknowledge by the slave node.  Returns I2cError::NO_ACK if the byte was
     * written successfully but was not acknowledged.
     */
    I2cError write(uint8_t byte);

    /**
     * Read a byte.
     *
     * The bus must have been started.
     */
    I2cError read(uint8_t *byte, bool ack = true);

  private:
    void delay() const {
        // Delay is called twice per cycle, so delay for half a cycle.
        // (This assumes the other instructions in the cycle take no time,
        // which isn't really true, but it's reasonable enough for our
        // purposes.)
        _delay_us(CYCLE_US / 2);
    }
    inline void dataHigh();
    inline void dataLow();
    bool clockWaitHigh();
    inline void clockLow();

    I2cError writeBit(bool bit);
    I2cError readBit(bool *bit);

    SdaPin _sda;
    SclPin _scl;
    bool _started{false};
};
