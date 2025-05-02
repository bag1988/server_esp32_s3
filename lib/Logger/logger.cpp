#include "logger.h"
#include <stdarg.h>

// Текущий уровень логирования
LogLevel currentLogLevel = LOG_INFO;

// Функция логирования
void logMessage(LogLevel level, const char* format, ...) {
    // Проверяем, нужно ли выводить сообщение
    if (level > currentLogLevel) {
        return;
    }
    
    // Префиксы для разных уровней логирования
    const char* prefix;
    switch (level) {
        case LOG_ERROR:
            prefix = "[ERROR] ";
            break;
        case LOG_WARNING:
            prefix = "[WARN] ";
            break;
        case LOG_INFO:
            prefix = "[INFO] ";
            break;
        case LOG_DEBUG:
            prefix = "[DEBUG] ";
            break;
        default:
            prefix = "";
            break;
    }
    
    // Выводим префикс
    Serial.print(prefix);
    
    // Форматируем и выводим сообщение
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial.println(buffer);
}
