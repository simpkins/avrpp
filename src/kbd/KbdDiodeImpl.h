// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/bitmap.h>
#include <avrpp/kbd/Keyboard.h>

/*
 * A Keyboard implementation for keyboards that have diodes on a subset of
 * their keys.
 *
 * This performs intelligent ghosting resolution based on which keys have
 * diodes.
 *
 * (Keyboards that have diodes on all keys don't experience ghosting, and
 * therefore could use a simpler implementation which skips the
 * resolveGhosting() function.  Keyboards that don't have any diodes always
 * have to perform blocking to avoid ghosting.)
 */
template<uint8_t NUM_COLS_T, uint8_t NUM_ROWS_T, typename ImplT>
class KbdDiodeImpl : public Keyboard {
  public:
    enum : uint8_t {
        NUM_COLS = NUM_COLS_T,
        NUM_ROWS = NUM_ROWS_T,
    };

    KbdDiodeImpl() {}

    virtual bool scanKeys() override;
    virtual void getState(uint8_t *modifiers,
                          uint8_t *keys,
                          uint8_t *keys_len) const override;

  protected:
    typedef Bitmap<NUM_ROWS> RowMap;
    typedef Bitmap<NUM_COLS> ColMap;
    typedef Bitmap<NUM_COLS * NUM_ROWS> KeyMap;

    uint8_t getIndex(uint8_t col, uint8_t row) const {
        return (row * NUM_COLS) + col;
    }

    // The following functions must be implemented by the ImplT subclass.
    // These do not need to be virtual, as they are not called virtually.
    //
    // void prepareColScan(uint8_t col);
    //   - Signal the specified column, and set all other rows and columns to
    //     inputs.
    //
    // void prepareRowScan(uint8_t row);
    //   - Signal the specified row, and set all other rows and columns to
    //     inputs.
    //
    // void finishRowScan();
    //   - Clean up after a row scan, and reset all rows to inputs.
    //     After prepareRowScan() is called, finishRowScan() will always be
    //     before any more calls to prepareColScan().  This way
    //     prepareColScan() can assume that all rows are already set to inputs.
    //     (We generally scan column by column, so prepareColScan() used much
    //     more frequently than prepareRowScan(), so we optimize for the
    //     prepareColScan() behavior.)
    //
    // void readRows(RowMap *rows);
    //   - Read the current row values.
    //
    // void readCols(ColMap *rows);
    //   - Read the current column values.

    pgm_ptr<uint8_t> _keyTable;
    pgm_ptr<uint8_t> _modifierTable;
    KeyMap _diodes;

  private:
    // Forbidden copy constructor and assignment operator
    KbdDiodeImpl(KbdDiodeImpl const &) = delete;
    KbdDiodeImpl& operator=(KbdDiodeImpl const &) = delete;

    // Helper functions so we can invoke ImplT methods without
    // using virtual function calls.
    void _prepareColScan(uint8_t col) {
        return static_cast<ImplT*>(this)->prepareColScan(col);
    }
    void _prepareRowScan(uint8_t row) {
        return static_cast<ImplT*>(this)->prepareRowScan(row);
    }
    void _finishRowScan() {
        return static_cast<ImplT*>(this)->finishRowScan();
    }
    void _readRows(RowMap *rows) {
        return static_cast<ImplT*>(this)->readRows(rows);
    }
    void _readCols(ColMap *cols) {
        return static_cast<ImplT*>(this)->readCols(cols);
    }

    void resolveGhosting();
    void performBlocking(uint8_t col_a, uint8_t col_b,
                         uint8_t row_a, uint8_t row_b);
    bool resolveRect(uint8_t col_a, uint8_t col_b,
                     uint8_t row_a, uint8_t row_b);
    bool ghost1Diode(uint8_t col_a, uint8_t col_b,
                     uint8_t row_a, uint8_t row_b);
    bool ghost2DiodesCol(uint8_t col_a, uint8_t col_b,
                         uint8_t row_a, uint8_t row_b);
    bool ghost2DiodesRow(uint8_t col_a, uint8_t col_b,
                         uint8_t row_a, uint8_t row_b);
    bool ghost2DiodesDiag(uint8_t col_a, uint8_t col_b,
                          uint8_t row_a, uint8_t row_b);
    bool ghost3Diodes(uint8_t col_a, uint8_t col_b,
                      uint8_t row_a, uint8_t row_b);

    KeyMap _mapA;
    KeyMap _mapB;
    KeyMap* _curMap{&_mapA};
    KeyMap* _prevMap{&_mapB};
};
