// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/bitmap.h>
#include <avrpp/kbd/KbdDiodeImpl.h>
#include <string.h>

class KeyboardV2 : public KbdDiodeImpl<8, 18, KeyboardV2> {
  public:
    enum : uint8_t {
        NUM_COLS = 8,
        NUM_ROWS = 10,
    };

    KeyboardV2();

    virtual void prepare() override;

    // Methods invoked by KbdDiodeImpl
    void prepareColScan(uint8_t col);
    void prepareRowScan(uint8_t row);
    void finishRowScan();
    void readRows(RowMap *rows);
    void readCols(ColMap *cols);
};
