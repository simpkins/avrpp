// Copyright (c) 2013, Adam Simpkins
//
// Logging utility code.
//
// These logging functions provide a convenient API, although the template
// instantiations can end up generating quite a fair amount of code.  If you
// are tight on program space you probably should be wary of this.
#pragma once

#include <avrpp/atomic.h>
#include <avrpp/progmem.h>

/*
 * If LOG_TYPE_SAFE is 1, log_msg() uses C++ variadic templates,
 * and is fully type-safe.  If LOG_TYPE_SAFE is 0 (or undefined),
 * stdio's printf() implementation is used, using older C-style variadic
 * arguments.
 *
 * When LOG_TYPE_SAFE is enabled the log_msg() arguments are always processed
 * using their correct type, even if you screw up the format string specifier.
 * However, this results in a larger final binary size.
 */
#define LOG_TYPE_SAFE 1

/*
 * The global log level which controls the LOG() function.
 */
extern uint8_t g_log_level;

/*
 * Set the logging level for the current file.
 *
 * This macro should be used once at the top of each file that uses FLOG().
 *
 * (The log level is marked unused to avoid compiler warnings if your file
 * doesn't actually use FLOG().  It's convenient to be able to use
 * F_LOG_LEVEL() even if the file does not have any log statements at the
 * moment.  During development new files may not have FLOG() statements yet,
 * or the last FLOG() statement in a file may have been temporarily removed
 * during refactoring.)
 */
#define F_LOG_LEVEL(level) \
    static constexpr uint8_t f_log_level __attribute__((unused)) = (level);

typedef bool (*LogPutcharFn)(uint8_t c, void* arg);

/*
 * Set the putchar function used to emit log data.
 *
 * This must be called for logs to be printed anywhere.
 */
void set_log_putchar(LogPutcharFn fn, void* arg);

#define LOG(level, fmt, ...) \
    do { \
        if ((level) <= g_log_level) { \
            log_msg(_(fmt), ## __VA_ARGS__); \
        } \
    } while (0)

/*
 * File-specific log macro.
 *
 * This checks the specified log level against f_log_level, which is intended
 * to be a file-static log level variable.  This allows log levels to be
 * controlled separately in each .cpp file.
 */
#define FLOG(level, fmt, ...) \
    do { \
        if ((level) <= f_log_level) { \
            log_msg(_(fmt), ## __VA_ARGS__); \
        } \
    } while (0)

#if LOG_TYPE_SAFE

enum FormatFlags : uint8_t {
    FLAG_NONE = 0x0,
    FLAG_ALT = 0x1,
    FLAG_HEX = 0x2,
    FLAG_CHAR = 0x4,
    FLAG_UPPER = 0x8,
};

char _parse_format(pgm_ptr<char>* fmt, FormatFlags* flags);
char _emit_to_format(pgm_ptr<char>* fmt, FormatFlags* flags);

template<typename T> struct LogPrinter;

// _print_value()'s primary purpose is to decay the type T,
// since we don't have access to std::decay() in AVR's libstdc++.
template<typename T>
void _print_value(const T& value, FormatFlags flags) {
    LogPrinter<T> printer;
    printer.print(value, flags);
}

// This string is used inside the log_msg() template.
// Define it separately, otherwise the PSTR() macro will emit a separate
// definition for each template instantiation.
extern char const PROGMEM _invalid_format_str[];

template<typename Arg>
void _log_one(pgm_ptr<char> *fmt, const Arg& arg) {
    FormatFlags flags;
    char c = _emit_to_format(fmt, &flags);
    if (c != '\0') {
        _print_value(arg, flags);
    }
}

enum class NoParam { NO_PARAM };
static inline void _log_msg_locked(pgm_ptr<char> fmt) {
    _log_one(&fmt, NoParam::NO_PARAM);
}

template<typename Arg1, typename... Args>
void _log_msg_locked(pgm_ptr<char> fmt, const Arg1& arg, const Args&... args) {
    // Many different versions of log_msg() get instantiated, so try to
    // minimize the amount of code here to avoid bloating the final binary.
    //
    // (Even so, this can still emit a fair amount of code.  For small embedded
    // systems the type safety of variadic templates might not always be a good
    // trade-off compared to the smaller printf implementation.)
    _log_one(&fmt, arg);
    _log_msg_locked(fmt, args...);
}

/**
 * Log a message.
 *
 * This takes a printf-style format string, followed by a list of arguments
 * to use in the printf expansions.
 *
 * Not all printf features are supported.  (In particular, field widths and
 * precisions are currently unimplemented.)
 */
template<typename... Args>
void log_msg(pgm_ptr<char> fmt, const Args&... args) {
    // Use an AtomicGuard to make sure that we aren't interrupted in the middle
    // of a log_msg() call.
    //
    // This isn't strictly necessary for program correctness.  This simply
    // ensures that log messages get emitted one at a time, which makes
    // everything easier to read.
    //
    // Without this, an interrupt can occur in the middle of a log_msg() call,
    // causing log messages from the interrupt context to appear in the middle
    // of the main program's log message.
    AtomicGuard ag;
    _log_msg_locked(fmt, args...);
}

#define DECLARE_PRINTER(type) \
    template<> struct LogPrinter<type> { \
        void print(type value, FormatFlags flags); \
    }

DECLARE_PRINTER(char);
DECLARE_PRINTER(uint8_t);
DECLARE_PRINTER(int8_t);
DECLARE_PRINTER(uint16_t);
DECLARE_PRINTER(int16_t);
DECLARE_PRINTER(uint32_t);
DECLARE_PRINTER(int32_t);
DECLARE_PRINTER(pgm_ptr<char>);
DECLARE_PRINTER(NoParam);
DECLARE_PRINTER(const char*);

#else // !LOG_TYPE_SAFE

void log_msg(pgm_ptr<char> fmt, ...) __attribute__((format (printf, 1, 2)));

#endif // LOG_TYPE_SAFE
