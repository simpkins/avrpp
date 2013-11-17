// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/i2c.h>

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
void
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::dataHigh() {
    // Set SDA as input.
    // The port setting should already be 0 (no pull-up resistor).
    _sda.setInput();
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
void
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::dataLow() {
    // Set SDA as output.
    // The port setting should already be 0 (output low).
    _sda.setOutput();
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
bool
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::clockWaitHigh() {
    // Set SCL as input.
    // The port setting should already be 0 (no pull-up resistor).
    _scl.setInput();

    // Wait for the clock to go high.
    // (A slave node may be holding it low for clock stretching.)
    uint16_t n = 0;
    while (_scl.read() == 0) {
        ++n;
        if (n > CLOCK_TIMEOUT) {
            return false;
        }
    }
    return true;
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
void
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::clockLow() {
    // Set SCL as output.
    // The port setting should already be 0 (output low).
    _scl.setOutput();
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::I2cBus() {
    // Configure SDA and SCL as input, with no pull-up resistor
    _sda.setInput(false);
    _scl.setInput(false);
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
I2cError
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::start() {
    if (_started) {
        dataHigh();
        delay();
        if (!clockWaitHigh()) {
            return I2cError::CLOCK_TIMEOUT;
        }
        delay();
    } else {
        // Make sure the clock is high
        if (!clockWaitHigh()) {
            return I2cError::CLOCK_TIMEOUT;
        }
    }

    if (_sda.read() != 1) {
        return I2cError::ARBITRATION_LOST;
    }

    // Bring the data line low
    dataLow();
    delay();
    clockLow();

    _started = true;
    return I2cError::SUCCESS;
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
I2cError
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::stop() {
    dataLow();
    delay();
    if (!clockWaitHigh()) {
        return I2cError::CLOCK_TIMEOUT;
    }
    delay();

    dataHigh();
    _delay_us(1);
    if (_sda.read() != 1) {
        return I2cError::ARBITRATION_LOST;
    }

    _started = false;
    return I2cError::SUCCESS;
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
I2cError
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::softwareReset() {
    auto ret = start();
    if (ret != I2cError::SUCCESS) {
        return ret;
    }

    for (uint8_t n = 0; n < 9; ++n) {
        delay();
        clockWaitHigh();
        delay();
        clockLow();
    }

    ret = start();
    if (ret != I2cError::SUCCESS) {
        return ret;
    }
    return stop();
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
I2cError
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::write(uint8_t byte) {
    I2cError ret;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        ret = writeBit((byte & 0x80) != 0);
        if (ret != I2cError::SUCCESS) {
            return ret;
        }
        byte <<= 1;
    }

    bool ack_bit;
    ret = readBit(&ack_bit);
    if (ret != I2cError::SUCCESS) {
        return ret;
    }
    // The slave node pulls the ack bit low (false)
    // to indicate acknowledgement.
    if (ack_bit != 0) {
        ret = I2cError::NO_ACK;
    }
    return ret;
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
I2cError
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::read(uint8_t *byte, bool ack) {
    *byte = 0;
    for (uint8_t n = 0; n < 8; ++n) {
        bool bit;
        auto ret = readBit(&bit);
        if (ret != I2cError::SUCCESS) {
            return ret;
        }
        if (bit) {
            *byte |= (1 << (7 - n));
        }
    }

    auto ret = writeBit(!ack);
    if (ret != I2cError::SUCCESS) {
        return ret;
    }

    return I2cError::SUCCESS;
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
I2cError
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::writeBit(bool bit) {
    if (bit) {
        dataHigh();
    } else {
        dataLow();
    }
    delay();

    if (!clockWaitHigh()) {
        return I2cError::CLOCK_TIMEOUT;
    }

    if (bit && (_sda.read() == 0)) {
        return I2cError::ARBITRATION_LOST;
    }

    delay();
    clockLow();
    return I2cError::SUCCESS;
}

template<typename SdaPin, typename SclPin,
         uint16_t CYCLE_US, uint16_t CLOCK_TIMEOUT>
I2cError
I2cBus<SdaPin, SclPin, CYCLE_US, CLOCK_TIMEOUT>::readBit(bool *bit) {
    // Let the slave drive SDA
    dataHigh();
    delay();
    if (!clockWaitHigh()) {
        return I2cError::CLOCK_TIMEOUT;
    }

    *bit = _sda.read();

    delay();
    clockLow();
    return I2cError::SUCCESS;
}
