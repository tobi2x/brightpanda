#ifndef BRIGHTPANDA_LOGGER_H
#define BRIGHTPANDA_LOGGER_H

#include <stdbool.h>
#include <stdarg.h>

/*
 * Simple logging system for Brightpanda.
 * Supports multiple log levels and can output to stdout/stderr or files.
 */

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_SILENT = 4
} LogLevel;

typedef enum {
    LOG_OUTPUT_STDOUT,
    LOG_OUTPUT_STDERR,
    LOG_OUTPUT_FILE
} LogOutput;

/* Initialize the logger */
bool logger_init(LogLevel level, LogOutput output, const char* filepath);

/* Set the current log level */
void logger_set_level(LogLevel level);

/* Get the current log level */
LogLevel logger_get_level(void);

/* Enable/disable timestamps */
void logger_set_timestamps(bool enabled);

/* Enable/disable colors (for terminal output) */
void logger_set_colors(bool enabled);

/* Core logging functions */
void log_debug(const char* format, ...);
void log_info(const char* format, ...);
void log_warn(const char* format, ...);
void log_error(const char* format, ...);

/* Logging with file/line context */
void log_debug_at(const char* file, int line, const char* format, ...);
void log_info_at(const char* file, int line, const char* format, ...);
void log_warn_at(const char* file, int line, const char* format, ...);
void log_error_at(const char* file, int line, const char* format, ...);

/* Convenience macros for logging with context */
#define LOG_DEBUG(...) log_debug_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) log_info_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) log_warn_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_error_at(__FILE__, __LINE__, __VA_ARGS__)

/* Cleanup */
void logger_shutdown(void);

#endif // BRIGHTPANDA_LOGGER_H