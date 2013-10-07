// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <stdint.h>

class Keyboard {
  public:
    class Callback {
      public:
        // virtual ~Callback() {}

        virtual void onChange(Keyboard* kbd) = 0;
    };

    virtual void prepareLoop();
    void scanKeys();

    /*
     * loop() is a convenience method that calls prepareLoop(),
     * and then loops calling scanKeys().
     */
    void loop();

    void setCallback(Callback* callback) {
        _callback = callback;
    }

    constexpr uint8_t getMaxPressedKeys() const {
        return MAX_KEYS;
    }

    const uint8_t* getPressedKeys() const {
        return _pressedKeys;
    }
    uint8_t getModifierMask() const {
        return _modifierMask;
    }

  protected:
    void keyPressed(uint8_t col, uint8_t row);

  private:
    virtual void scanImpl() = 0;

    enum { MAX_KEYS = 6 };
    uint8_t _modifierMask{0};
    uint8_t _pressedKeys[MAX_KEYS]{0, 0, 0, 0, 0, 0};

    uint8_t _newModifierMask{0};
    uint8_t _newPressedKeys[MAX_KEYS]{0, 0, 0, 0, 0, 0};
    uint8_t _pressedIndex{0};
    bool _changed{false};

    Callback* _callback{nullptr};

  protected:
    uint8_t _numCols{0};
    const uint8_t* _keyTable{nullptr};
    const uint8_t* _modifierTable{nullptr};
};
