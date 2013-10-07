// Copyright (c) 2013, Adam Simpkins
#pragma once

#define LIKELY(x)   (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))

void set_cpu_prescale();
