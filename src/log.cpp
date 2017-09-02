// Copyright (c) 2013, Adam Simpkins
#include <avrpp/log.h>

uint8_t g_log_level = 1;

static LogPutcharFn f_log_putchar;
static void *f_log_arg;

void set_log_putchar(LogPutcharFn fn, void *arg) {
    f_log_putchar = fn;
    f_log_arg = arg;
}

static inline bool putchar_safe(uint8_t c) {
    if (!f_log_putchar) {
        return false;
    }
    return f_log_putchar(c, f_log_arg);
}

#if LOG_TYPE_SAFE

char const PROGMEM _invalid_format_str[] = "<invalid format>";

template<typename UintType>
void print_unsigned(UintType value, FormatFlags flags) {
    if (!f_log_putchar) {
        return;
    }

    if (flags & FLAG_HEX) {
        if (flags & FLAG_ALT) {
            f_log_putchar('0', f_log_arg);
            f_log_putchar('x', f_log_arg);
        }

        uint8_t const char_ten = ((flags & FLAG_UPPER) ? 'A' : 'a') - 10;
        for (int n = sizeof(UintType) * 8; n > 0; /**/) {
            n -= 4;
            uint8_t nibble = (value >> n) & 0xf;
            f_log_putchar((nibble < 10 ? '0' : char_ten) + nibble, f_log_arg);
        }
    } else {
        char buf[12];
        int idx = 0;
        if (value == 0) {
            f_log_putchar('0', f_log_arg);
            return;
        }
        while (value > 0) {
            uint8_t remainder = value % 10;
            value /= 10;
            buf[idx] = '0' + remainder;
            ++idx;
        }
        while (true) {
            --idx;
            f_log_putchar(buf[idx], f_log_arg);
            if (idx == 0) {
                break;
            }
        }
    }
}

template<typename T> struct unsigned_type;
template<> struct unsigned_type<int8_t> { typedef uint8_t type; };
template<> struct unsigned_type<int16_t> { typedef uint16_t type; };
template<> struct unsigned_type<int32_t> { typedef uint32_t type; };

template<typename IntType>
void print_signed(IntType value, FormatFlags flags) {
    if (value < 0) {
        putchar_safe('-');
        print_unsigned<typename unsigned_type<IntType>::type>(-value, flags);
    } else {
        print_unsigned<typename unsigned_type<IntType>::type>(value, flags);
    }
}

void LogPrinter<char>::print(char value, FormatFlags flags) {
    if (flags & FLAG_CHAR) {
        putchar_safe(value);
        return;
    }
    print_unsigned(value, flags);
}

void LogPrinter<const char*>::print(const char* value,
                                    FormatFlags /* flags */) {
    const char* p = value;
    while (*p) {
        putchar_safe(*p);
        ++p;
    }
}

void LogPrinter<uint8_t>::print(uint8_t value, FormatFlags flags) {
    if (flags & FLAG_CHAR) {
        putchar_safe(value);
        return;
    }
    print_unsigned(value, flags);
}

void LogPrinter<int8_t>::print(int8_t value, FormatFlags flags) {
    if (flags & FLAG_CHAR) {
        putchar_safe(value);
        return;
    }
    print_unsigned(value, flags);
}

void LogPrinter<uint16_t>::print(uint16_t value, FormatFlags flags) {
    print_unsigned(value, flags);
}

void LogPrinter<int16_t>::print(int16_t value, FormatFlags flags) {
    print_signed(value, flags);
}

void LogPrinter<uint32_t>::print(uint32_t value, FormatFlags flags) {
    print_unsigned(value, flags);
}

void LogPrinter<int32_t>::print(int32_t value, FormatFlags flags) {
    print_signed(value, flags);
}

void LogPrinter<pgm_ptr<char>>::print(pgm_ptr<char> value, FormatFlags flags) {
    if (!f_log_putchar) {
        return;
    }

    while (true) {
        char c = *value;
        if (c == '\0') {
            break;
        }
        f_log_putchar(c, f_log_arg);
        ++value;
    }
    (void)flags;
}

void LogPrinter<NoParam>::print(NoParam value, FormatFlags flags) {
    _print_value(_("<missing parameter>"), flags);
    (void)value;
}

char _parse_format(pgm_ptr<char>* fmt, FormatFlags* flags) {
    *flags = FLAG_NONE;
    while (true) {
        char c = **fmt;
        if (c == '\0') {
            // Invalid format specifier
            return '\0';
        }
        ++(*fmt);

        switch (c) {
            case '#':
                *flags = static_cast<FormatFlags>(*flags | FLAG_ALT);
                continue;
            case 'h':
            case 'l':
            case 'z':
                // Since we have full type info we just ignore these flags
                continue;
            case '%':
                return '%';
            case 'x':
                *flags = static_cast<FormatFlags>(*flags | FLAG_HEX);
                return 's';
            case 'X':
                *flags = static_cast<FormatFlags>(*flags | FLAG_HEX |
                                                  FLAG_UPPER);
                return 's';
            case 'c':
                *flags = static_cast<FormatFlags>(*flags | FLAG_CHAR);
                return 's';
            case 'd':
            case 's':
            case 'f':
            case 'g':
            case 'u':
                // We basically ignore the exact format specifier type,
                // and return 's' for everything.  We figure out the right type
                // to print based on the argument type.
                return 's';
            case '-':
            case '+':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.':
                // TODO: Handle range and precision specifiers
                continue;
        }
        // Anything else is invalid.
        return '\0';
    }
}

char _emit_to_format(pgm_ptr<char>* fmt, FormatFlags* flags) {
    while (true) {
        char c = **fmt;
        if (c == '\0') {
            return '\0';
        }
        ++*fmt;

        if (c != '%') {
            putchar_safe(c);
            continue;
        }

        c = _parse_format(fmt, flags);
        switch (c) {
            case '\0':
                _print_value(pgm_ptr<char>(_invalid_format_str), FLAG_NONE);
                continue;
            case '%':
                putchar_safe(c);
                continue;
            default:
                return c;
        }
    }
}

#else // !LOG_TYPE_SAFE

#include <stdio.h>

static int log_file_putchar(char c, FILE*) {
    return f_log_putchar(c, f_log_arg) ? 0 : 1;
}

// FDEV_SETUP_STREAM() unfortuately uses designated initializers,
// which don't work in C++.  It might be worth declaring _logf in a separate
// pure C file.
FILE _logf = {
    nullptr, false, _FDEV_SETUP_WRITE, 0, 0,
    log_file_putchar, nullptr,
    nullptr
};

void log_msg(pgm_ptr<char> fmt, ...) {
    if (!f_log_putchar) {
        return 1;
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf_P(&_logf, fmt.value(), ap);
    va_end(ap);
}
#endif
