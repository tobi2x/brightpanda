#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Global logger state */
static struct {
    LogLevel level;
    LogOutput output;
    FILE* file;
    bool timestamps;
    bool colors;
    bool initialized;
} logger_state = {
    .level = LOG_LEVEL_INFO,
    .output = LOG_OUTPUT_STDOUT,
    .file = NULL,
    .timestamps = true,
    .colors = true,
    .initialized = false
};

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_GRAY    "\033[90m"

static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static const char* level_to_color(LogLevel level) {
    if (!logger_state.colors) return "";
    
    switch (level) {
        case LOG_LEVEL_DEBUG: return COLOR_GRAY;
        case LOG_LEVEL_INFO:  return COLOR_BLUE;
        case LOG_LEVEL_WARN:  return COLOR_YELLOW;
        case LOG_LEVEL_ERROR: return COLOR_RED;
        default: return "";
    }
}

static FILE* get_output_stream(LogLevel level) {
    if (logger_state.output == LOG_OUTPUT_FILE && logger_state.file) {
        return logger_state.file;
    }
    
    if (logger_state.output == LOG_OUTPUT_STDERR || level >= LOG_LEVEL_WARN) {
        return stderr;
    }
    
    return stdout;
}

static void format_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static void log_message(LogLevel level, const char* file, int line, const char* format, va_list args) {
    if (!logger_state.initialized) {
        logger_init(LOG_LEVEL_INFO, LOG_OUTPUT_STDOUT, NULL);
    }
    
    if (level < logger_state.level) {
        return; // Message below current log level
    }
    
    FILE* out = get_output_stream(level);
    const char* color = level_to_color(level);
    const char* reset = (logger_state.colors && color[0]) ? COLOR_RESET : "";
    
    // Print timestamp if enabled
    if (logger_state.timestamps) {
        char timestamp[32];
        format_timestamp(timestamp, sizeof(timestamp));
        fprintf(out, "[%s] ", timestamp);
    }
    
    // Print log level with color
    fprintf(out, "%s%-5s%s ", color, level_to_string(level), reset);
    
    // Print file:line for debug builds or when available
    if (file && line > 0 && level >= LOG_LEVEL_DEBUG) {
        const char* filename = strrchr(file, '/');
        filename = filename ? filename + 1 : file;
        fprintf(out, "[%s:%d] ", filename, line);
    }
    
    // Print the actual message
    vfprintf(out, format, args);
    fprintf(out, "\n");
    
    // Flush immediately for errors
    if (level >= LOG_LEVEL_ERROR) {
        fflush(out);
    }
}

bool logger_init(LogLevel level, LogOutput output, const char* filepath) {
    logger_state.level = level;
    logger_state.output = output;
    logger_state.timestamps = true;
    
    // Check if output supports colors (TTY detection)
    if (output == LOG_OUTPUT_STDOUT) {
        logger_state.colors = isatty(fileno(stdout));
    } else if (output == LOG_OUTPUT_STDERR) {
        logger_state.colors = isatty(fileno(stderr));
    } else {
        logger_state.colors = false;
    }
    
    // Open log file if needed
    if (output == LOG_OUTPUT_FILE) {
        if (!filepath) {
            fprintf(stderr, "Error: Log file path required for LOG_OUTPUT_FILE\n");
            return false;
        }
        
        logger_state.file = fopen(filepath, "a");
        if (!logger_state.file) {
            fprintf(stderr, "Error: Failed to open log file: %s\n", filepath);
            return false;
        }
        logger_state.colors = false; // No colors in files
    }
    
    logger_state.initialized = true;
    return true;
}

void logger_set_level(LogLevel level) {
    logger_state.level = level;
}

LogLevel logger_get_level(void) {
    return logger_state.level;
}

void logger_set_timestamps(bool enabled) {
    logger_state.timestamps = enabled;
}

void logger_set_colors(bool enabled) {
    logger_state.colors = enabled;
}

void log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_DEBUG, NULL, 0, format, args);
    va_end(args);
}

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_INFO, NULL, 0, format, args);
    va_end(args);
}

void log_warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_WARN, NULL, 0, format, args);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_ERROR, NULL, 0, format, args);
    va_end(args);
}

void log_debug_at(const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_DEBUG, file, line, format, args);
    va_end(args);
}

void log_info_at(const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_INFO, file, line, format, args);
    va_end(args);
}

void log_warn_at(const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_WARN, file, line, format, args);
    va_end(args);
}

void log_error_at(const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_ERROR, file, line, format, args);
    va_end(args);
}

void logger_shutdown(void) {
    if (logger_state.file) {
        fclose(logger_state.file);
        logger_state.file = NULL;
    }
    logger_state.initialized = false;
}