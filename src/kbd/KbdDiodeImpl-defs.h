// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/kbd/KbdDiodeImpl.h>

template<uint8_t NC, uint8_t NR, typename ImplT>
void
KbdDiodeImpl<NC, NR, ImplT>::getState(uint8_t *modifiers,
                                      uint8_t *keys,
                                      uint8_t *keys_len) const {
    *modifiers = 0;

    FLOG(5, "getState():");
    uint8_t pressed_idx = 0;
    for (uint8_t col = 0; col < NUM_COLS; ++col) {
        for (uint8_t row = 0; row < NUM_ROWS; ++row) {
            auto idx = getIndex(col, row);
            if (_curMap->get(idx)) {
                FLOG(5, " (%d,%d)=%d", col, row, _keyTable[idx]);
                if (pressed_idx < *keys_len) {
                    keys[pressed_idx] = _keyTable[idx];
                }
                ++pressed_idx;
                uint8_t modifier = _modifierTable[idx];
                *modifiers |= modifier;
            }
        }
    }
    FLOG(5, " [%d pressed]\n", pressed_idx);

    *keys_len = pressed_idx;
}

template<uint8_t NC, uint8_t NR, typename ImplT>
bool
KbdDiodeImpl<NC, NR, ImplT>::scanKeys() {
    // Swap _prevMap and _curMap
    auto tmp = _prevMap;
    _prevMap = _curMap;
    _curMap = tmp;
    // Clear _curMap in preparation for this scan
    _curMap->clear();

    // Perform a basic scan over the keys
    uint8_t numPressed = 0;
    for (uint8_t col = 0; col < NUM_COLS; ++col) {
        _prepareColScan(col);
        _delay_us(5);

        RowMap rows;
        _readRows(&rows);
        for (uint8_t row = 0; row < NUM_ROWS; ++row) {
            if (rows[row]) {
                ++numPressed;
                _curMap->set(getIndex(col, row));
            }
        }
    }

    if (numPressed >= 4) {
        // Now look for possible ghosting, and attempt to resolve it,
        // or perform blocking if we cannot determing if a key press is
        // real or ghosting.
        resolveGhosting();
    }

    return (*_curMap != *_prevMap);
}

template<uint8_t NC, uint8_t NR, typename ImplT>
void
KbdDiodeImpl<NC, NR, ImplT>::resolveGhosting() {
    // Walk through the keys, and look for rectangles where all 4 corners
    // are pressed.  In these cases, 1 corner might not really be pressed,
    // but was simply detected due to ghosting.
    //
    // This process is O(N^2) in the worst case (where N is the total number of
    // keys).  However, in the common case where very few keys are pressed it
    // is close to O(N).  (It is O(N + K*N), where K is the number of keys
    // pressed.)

    // First find all rectangles.  We can't process rectangles as we find them,
    // since we want to make sure that there aren't overlapping rectangles.
    struct Rect {
        Rect() {}
        Rect(uint8_t ca, uint8_t cb, uint8_t ra, uint8_t rb)
            : col_a(ca), col_b(cb), row_a(ra), row_b(rb) {}

        uint8_t col_a{0xff};
        uint8_t col_b{0xff};
        uint8_t row_a{0xff};
        uint8_t row_b{0xff};
    };

    // Remember up to 4 rectangles.  If we find more than this, just perform
    // simple blocking on the additional rectangles.
    enum : uint8_t { NUM_RECTS = 4 };
    Rect rects[NUM_RECTS];
    uint8_t num_rects = 0;

    KeyMap in_rect;
    KeyMap overlaps;
    for (uint8_t col_a = 0; col_a < NUM_COLS; ++col_a) {
        for (uint8_t row_a = 0; row_a < NUM_ROWS; ++row_a) {
            auto idx_aa = getIndex(col_a, row_a);
            if (!_curMap->get(idx_aa)) {
                continue;
            }

            // Look for another row_a down on this column
            for (uint8_t row_b = row_a + 1; row_b < NUM_ROWS; ++row_b) {
                auto idx_ab = getIndex(col_a, row_b);
                if (!_curMap->get(idx_ab)) {
                    continue;
                }

                // aa and ab are both down.
                // Look for another column with both ba and bb down
                for (uint8_t col_b = col_a + 1; col_b < NUM_COLS; ++col_b) {
                    auto idx_ba = getIndex(col_b, row_a);
                    auto idx_bb = getIndex(col_b, row_b);
                    if (_curMap->get(idx_ba) && _curMap->get(idx_bb)) {
                        // Found a rectangle
                        FLOG(4, "Found rectangle (%d, %d) x (%d, %d)\n",
                             col_a, row_a, col_b, row_b);
                        // Track which keys are in multiple rectangles.
                        overlaps.set(idx_aa, in_rect.get(idx_aa));
                        overlaps.set(idx_ab, in_rect.get(idx_ab));
                        overlaps.set(idx_ba, in_rect.get(idx_ba));
                        overlaps.set(idx_bb, in_rect.get(idx_bb));
                        in_rect.set(idx_aa);
                        in_rect.set(idx_ab);
                        in_rect.set(idx_bb);
                        in_rect.set(idx_ba);

                        if (num_rects >= NUM_RECTS) {
                            // Just perform basic blocking for this rectangle.
                            FLOG(2, "Too many rectangles to process!  "
                                 "Falling back to simple blocking\n");
                            performBlocking(col_a, col_b, row_a, row_b);
                            continue;
                        }

                        rects[num_rects] = Rect(col_a, col_b, row_a, row_b);
                        ++num_rects;
                    }
                }
            }
        }
    }

    // Now perform ghosting resolution on each rectangle we found.
    for (uint8_t n = 0; n < num_rects; ++n) {
        const Rect& r = rects[n];
        // Check for overlaps
        if (overlaps.get(getIndex(r.col_a, r.row_a)) ||
            overlaps.get(getIndex(r.col_a, r.row_b)) ||
            overlaps.get(getIndex(r.col_b, r.row_a)) ||
            overlaps.get(getIndex(r.col_b, r.row_b))) {
            // This rectangle overlaps with another rectangle.  Trying to
            // actually resolve the ghosting would be complicated, so just
            // perform simple blocking.
            FLOG(2, "Found overlapping rectangles.  "
                 "Falling back to simple blocking\n");
            performBlocking(r.col_a, r.col_b, r.row_a, r.row_b);
            continue;
        }

        if (!resolveRect(r.col_a, r.col_b, r.row_a, r.row_b)) {
            performBlocking(r.col_a, r.col_b, r.row_a, r.row_b);
        }
    }
}

template<uint8_t NC, uint8_t NR, typename ImplT>
void
KbdDiodeImpl<NC, NR, ImplT>::performBlocking(uint8_t col_a, uint8_t col_b,
                                             uint8_t row_a, uint8_t row_b) {
    FLOG(3, "Performing blocking on rectangle (%d, %d) x (%d, %d)\n",
         col_a, row_a, col_b, row_b);

    // Some of the keys in this rectangle are likely not really down,
    // but only appear down due to ghosting.
    //
    // Reporting these ghosted key strokes is undesirable.  Only report keys in
    // this rectangle if they were reported in the previous scan iteration.
    // This means we might not report a key that is actually pressed, but it's
    // better than reporting a key that isn't pressed.
    auto idx_aa = getIndex(col_a, row_a);
    if (!_prevMap->get(idx_aa)) {
        _curMap->unset(idx_aa);
    }
    auto idx_ab = getIndex(col_a, row_b);
    if (!_prevMap->get(idx_ab)) {
        _curMap->unset(idx_ab);
    }
    auto idx_ba = getIndex(col_b, row_a);
    if (!_prevMap->get(idx_ba)) {
        _curMap->unset(idx_ba);
    }
    auto idx_bb = getIndex(col_b, row_b);
    if (!_prevMap->get(idx_bb)) {
        _curMap->unset(idx_bb);
    }
}

template<uint8_t NC, uint8_t NR, typename ImplT>
bool
KbdDiodeImpl<NC, NR, ImplT>::resolveRect(uint8_t col_a, uint8_t col_b,
                                         uint8_t row_a, uint8_t row_b) {
    FLOG(3, "attemping to resolve ghosting on rectangle (%d, %d) x (%d, %d)\n",
         col_a, row_a, col_b, row_b);

    // Check to see which keys have diodes.
    bool diode_aa = _diodes[getIndex(col_a, row_a)];
    bool diode_ab = _diodes[getIndex(col_a, row_b)];
    bool diode_ba = _diodes[getIndex(col_b, row_a)];
    bool diode_bb = _diodes[getIndex(col_b, row_b)];
    uint8_t num_diodes =
        static_cast<uint8_t>(diode_aa) +
        static_cast<uint8_t>(diode_ab) +
        static_cast<uint8_t>(diode_ba) +
        static_cast<uint8_t>(diode_bb);

    // If there are no diodes we can't resolve ghosting
    if (num_diodes == 0) {
        FLOG(3, "  No diodes: unable to resolve ghosting\n");
        return false;
    }

    // If there are diodes on all 4 keys then all 4 must actually be down.
    if (num_diodes == 4) {
        FLOG(3, "  4 diodes: all keys valid\n");
        return true;
    }

    // Single diode: we should be able to detect ghosting
    // on any key except for the key with the diode.
    if (num_diodes == 1) {
        // No matter where the diode is, we can reorder the arguments to
        // ghost1Diode() so that ghost1Diode always sees the diode on
        // (col_a, row_a)
        if (diode_aa) {
            return ghost1Diode(col_a, col_b, row_a, row_b);
        } else if (diode_ab) {
            return ghost1Diode(col_a, col_b, row_b, row_a);
        } else if (diode_ba) {
            return ghost1Diode(col_b, col_a, row_a, row_b);
        } else { // diode_bb
            return ghost1Diode(col_b, col_a, row_b, row_a);
        }
    }

    // 2 diodes
    if (num_diodes == 2) {
        // There are several cases to consider here:
        // - The diodes are both on the same column
        // - The diodes are both on the same row
        // - The diodes are on opposite diagonals
        if (diode_aa && diode_ab) {
            return ghost2DiodesCol(col_a, col_b, row_a, row_b);
        } else if (diode_ba && diode_bb) {
            return ghost2DiodesCol(col_b, col_a, row_a, row_b);
        } else if (diode_aa && diode_ba) {
            return ghost2DiodesRow(col_a, col_b, row_a, row_b);
        } else if (diode_ab && diode_bb) {
            return ghost2DiodesRow(col_a, col_b, row_b, row_a);
        } else if (diode_aa) {
            return ghost2DiodesDiag(col_a, col_b, row_a, row_b);
        } else {
            return ghost2DiodesDiag(col_b, col_a, row_a, row_b);
        }
        // Unreachable
        return false;
    }

    // 3 diodes
    if (diode_aa) {
        return ghost3Diodes(col_a, col_b, row_a, row_b);
    } else if (diode_ab) {
        return ghost3Diodes(col_a, col_b, row_b, row_a);
    } else if (diode_ba) {
        return ghost3Diodes(col_b, col_a, row_a, row_b);
    } else { // diode_bb
        return ghost3Diodes(col_b, col_a, row_b, row_a);
    }
    // Unreachable
    return false;
}

// Detect ghosting with a single diode at (col_a, row_a)
template<uint8_t NC, uint8_t NR, typename ImplT>
bool
KbdDiodeImpl<NC, NR, ImplT>::ghost1Diode(uint8_t col_a, uint8_t col_b,
                                         uint8_t row_a, uint8_t row_b) {
    FLOG(3, "  Resolving ghosting with 1 diode at (%d, %d)\n",
         col_a, row_a);

    // Re-scan both columns, to see if the ghosting is still present.
    // Some time has elapsed since the initial scan, and the user may have
    // lifted off a key already.  If this has occurred, we may make the wrong
    // ghosting decision if we don't detect it.
    RowMap rows;
    _prepareColScan(col_a);
    _delay_us(5);
    _readRows(&rows);
    if (!rows.get(row_a)) {
        FLOG(3, "  Key lifted off (%d, %d)\n", col_a, row_a);
        _curMap->unset(getIndex(col_a, row_a));
        return true;
    }
    if (!rows.get(row_b)) {
        FLOG(3, "  Key lifted off (%d, %d)\n", col_a, row_b);
        _curMap->unset(getIndex(col_a, row_b));
        return true;
    }

    _prepareColScan(col_b);
    _delay_us(5);
    _readRows(&rows);
    if (!rows.get(row_a)) {
        FLOG(3, "  Key lifted off (%d, %d)\n", col_b, row_a);
        _curMap->unset(getIndex(col_b, row_a));
        return true;
    }
    if (!rows.get(row_b)) {
        FLOG(3, "  Key lifted off (%d, %d)\n", col_b, row_b);
        _curMap->unset(getIndex(col_b, row_b));
        return true;
    }

    // Signal col_b, and check to see if we receive the signal on col_a.
    // If we detect a signal, then AB is pressed.  Otherwise, AB is not pressed
    // and everything else in the rectangle is.
    ColMap cols;
    _readCols(&cols);
    if (!cols.get(col_a)) {
        FLOG(3, "  Fixed ghosting on (%d, %d)\n", col_a, row_b);
        _curMap->unset(getIndex(col_a, row_b));
        return true;
    }

    // Scan row_a.  We should either see both col_a and col_b, or neither.
    // If we see neither, then BA is up.
    _prepareRowScan(row_a);
    _delay_us(5);
    _readCols(&cols);
    _finishRowScan();
    if (!cols.get(col_a)) {
        FLOG(3, "  Fixed ghosting on (%d, %d)\n", col_b, row_a);
        _curMap->unset(getIndex(col_b, row_a));
        return true;
    }

    // If we are still here, we can't tell if AA is up or down.
    // However, we know all other keys are down.  (BB is protected by the
    // diode, so wouldn't have shown up in the rectangle if it weren't down.
    // AB and BA are handled above.)
    //
    // Perform blocking only on AA.
    FLOG(3, "  Performing blocking on single diode (%d, %d)\n", col_a, row_a);
    auto idx_aa = getIndex(col_a, row_a);
    if (!_prevMap->get(idx_aa)) {
        _curMap->unset(idx_aa);
    }
    return true;
}

// Detect ghosting with diodes at (col_a, row_a) and (col_a, row_b)
template<uint8_t NC, uint8_t NR, typename ImplT>
bool
KbdDiodeImpl<NC, NR, ImplT>::ghost2DiodesCol(uint8_t col_a, uint8_t col_b,
                                             uint8_t row_a, uint8_t row_b) {
    FLOG(3, "  2 diodes in col %d\n", col_a);
    // The keys in col B are protected by the diodes, and are definitely down.
    // Unfortunately we can't tell about the keys in col A, so perform blocking
    // for them.
    auto idx_aa = getIndex(col_a, row_a);
    if (!_prevMap->get(idx_aa)) {
        _curMap->unset(idx_aa);
    }
    auto idx_ab = getIndex(col_a, row_b);
    if (!_prevMap->get(idx_ab)) {
        _curMap->unset(idx_ab);
    }
    return false;
    (void)col_b;
}

// Detect ghosting with diodes at (col_a, row_a) and (col_b, row_a)
template<uint8_t NC, uint8_t NR, typename ImplT>
bool
KbdDiodeImpl<NC, NR, ImplT>::ghost2DiodesRow(uint8_t col_a, uint8_t col_b,
                                             uint8_t row_a, uint8_t row_b) {
    FLOG(3, "  2 diodes in row %d\n", row_a);
    // The keys in row B are protected by the diodes, and are definitely down.
    // Unfortunately we can't tell about the keys in row A, so perform blocking
    // for them.
    auto idx_aa = getIndex(col_a, row_a);
    if (!_prevMap->get(idx_aa)) {
        _curMap->unset(idx_aa);
    }
    auto idx_ba = getIndex(col_b, row_a);
    if (!_prevMap->get(idx_ba)) {
        _curMap->unset(idx_ba);
    }
    return true;
    (void)row_b;
}

// Detect ghosting with diodes at (col_a, row_a) and (col_b, row_b)
template<uint8_t NC, uint8_t NR, typename ImplT>
bool
KbdDiodeImpl<NC, NR, ImplT>::ghost2DiodesDiag(uint8_t col_a, uint8_t col_b,
                                              uint8_t row_a, uint8_t row_b) {
    FLOG(3, "  2 diagonal diodes at (%d, %d) and (%d, %d) %d\n",
         col_a, row_a, col_b, row_b);
    // AA and BB are definitely down, we only need to worry about AB and BA.
    // Scan row_a.  We will see col_b if and only if BA is pressed.
    ColMap cols;
    _prepareRowScan(row_a);
    _delay_us(5);
    _readCols(&cols);
    _finishRowScan();
    // 3 possibilities:
    // - See CA and CB: both AB and BA down
    if (!cols.get(col_b)) {
        // BA is not pressed.  All other keys are.
        _curMap->unset(getIndex(col_b, row_a));
        return true;
    }
    // Given that BA is pressed, we will also see column A
    // only if AB is pressed.
    if (!cols.get(col_a)) {
        _curMap->unset(getIndex(col_a, row_b));
        return true;
    }

    // All 4 keys in the rectangle are actually pressed.
    return true;
}

// Detect ghosting with 3 diodes.
// Only (col_a, row_a) doesn't have a diode.
template<uint8_t NC, uint8_t NR, typename ImplT>
bool
KbdDiodeImpl<NC, NR, ImplT>::ghost3Diodes(uint8_t col_a, uint8_t col_b,
                                          uint8_t row_a, uint8_t row_b) {
    FLOG(3, "  Resolving ghosting with 3 diodes\n");
    // We only need to detect ghosting for (col_b, row_b).
    // All other keys are protected by the diode on the opposite corner.
    // Unfortunately we can't tell if BB is pressed, so perform blocking on it.
    auto idx_bb = getIndex(col_b, row_b);
    if (!_prevMap->get(idx_bb)) {
        _curMap->unset(idx_bb);
    }
    return false;
    (void)col_a;
    (void)row_a;
}
