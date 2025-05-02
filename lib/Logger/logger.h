#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

enum LogLevel {
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
};

void logMessage(LogLevel level, const char* format, ...);

#define LOG_E(...) logMessage(LOG_ERROR, __VA_ARGS__)
#define LOG_W(...) logMessage(LOG_WARNING, __VA_ARGS__)
#define LOG_I(...) logMessage(LOG_INFO, __VA_ARGS__)
#define LOG_D(...) logMessage(LOG_DEBUG, __VA_ARGS__)

#endif // LOGGER_H
