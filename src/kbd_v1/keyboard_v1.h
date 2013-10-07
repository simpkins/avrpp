// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/keyboard.h>

class KeyboardV1 : public Keyboard {
  public:
    virtual void prepareLoop() override;

  private:
    virtual void scanImpl() override;
};
