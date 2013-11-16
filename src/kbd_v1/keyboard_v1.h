// Copyright (c) 2013, Adam Simpkins
#pragma once

#include <avrpp/keyboard.h>

class KeyboardV1 : public KeyboardImpl {
  public:
    virtual void prepare() override;

  private:
    virtual void scanImpl() override;
};
