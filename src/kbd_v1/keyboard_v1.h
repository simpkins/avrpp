// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/kbd/KbdDiodeImpl.h>

class KeyboardV1 : public KbdDiodeImpl<8, 16, KeyboardV1> {
  public:
    KeyboardV1();

    virtual void prepare() override;

    // Methods invoked by KbdDiodeImpl
    void prepareColScan(uint8_t col);
    void prepareRowScan(uint8_t row);
    void finishRowScan();
    void readRows(RowMap *rows);
    void readCols(ColMap *cols);
};
