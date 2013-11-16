// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/progmem.h>
#include <stdint.h>

class Keyboard {
  public:
    class Callback {
      public:
        virtual ~Callback() {}
        virtual void onChange(Keyboard* kbd) = 0;
    };

    virtual ~Keyboard() {}

    /*
     * prepare() should be called before the first invocation of scanKeys().
     *
     * This gets the keyboard ready for scanning.
     */
    virtual void prepare() = 0;

    /*
     * Scan all keys.
     *
     * Returns true if the keys have changed since the last call to scanKeys(),
     * or false if the keys are the same as last time.
     *
     * If scanKeys() returns true, call getState() to get the new keyboard
     * state.
     *
     * Callers should wait for 2 to 5 milliseconds between calls to scanKeys().
     * This avoids false key changes detected due to key bounce.
     */
    virtual bool scanKeys() = 0;

    /*
     * Continuously scan the keys, notifying the callback on any state change.
     *
     * This is a convenience method that simply loops calling scanKeys()
     * repeatedly.
     */
    virtual void loop(Callback *callback);

    /*
     * Get the current state of the keyboard.
     *
     * @param[out] modifiers
     *     The current modifier mask will be returned in the uint8_t pointed to
     *     by this argument.
     * @param[out] keys
     *     The currently pressed key codes will be stored in the uint8_t array
     *     pointed to by this argument.
     * @param[in,out] keys_len
     *     This parameter should point to a uint8_t specifying the length of
     *     the keys array.  No more than this many many key codes will be
     *     written to the keys array.  On return, this value will be updated
     *     to indicate the number of keys actually written to the keys array.
     */
    virtual void getState(uint8_t *modifiers,
                          uint8_t *keys,
                          uint8_t *keys_len) const = 0;
};

class KeyboardImpl : public Keyboard {
  public:
    virtual ~KeyboardImpl();

    virtual bool scanKeys() override;
    virtual void getState(uint8_t *modifiers,
                          uint8_t *keys,
                          uint8_t *keys_len) const override;

  protected:
    void keyPressed(uint8_t col, uint8_t row);

    void init(uint8_t num_cols,
              uint8_t num_rows,
              const pgm_ptr<uint8_t> &key_table,
              const pgm_ptr<uint8_t> &mod_table);

  private:
    virtual void scanImpl() = 0;
    void blockKeys();
    void clearRect(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2);
    void clearIf(uint8_t col, uint8_t row,
                 uint8_t diode_col, uint8_t diode_row);

    uint8_t getIndex(uint8_t col, uint8_t row) const {
        return (row * _numCols) + col;
    }
    uint16_t mapSize() const {
        return (_numCols * _numRows) * sizeof(uint8_t);
    }

    uint8_t _numCols{0};
    uint8_t _numRows{0};

    pgm_ptr<uint8_t> _keyTable{nullptr};
    pgm_ptr<uint8_t> _modifierTable{nullptr};

    /*
     * The keys that we detected were pressed
     */
    uint8_t *_detectedKeys{nullptr};
    /*
     * The keys that we report as pressed, after blocking/jamming.
     */
    uint8_t *_reportedKeys{nullptr};
    uint8_t *_prevKeys{nullptr};
    /*
     * Keys that have diodes.
     *
     * In any given rectangle of key switches, a diode prevents ghosting on the
     * opposite corner of the rectangle when the other 3 switches are on.
     */
    uint8_t *_diodes{nullptr};

    Callback* _callback{nullptr};
};
