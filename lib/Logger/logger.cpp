#include "logger.h"
#include <stdarg.h>
#include <web_server_setting.h> // Для доступа к events
// Текущий уровень логирования
LogLevel currentLogLevel = LOG_INFO;

// Внешняя ссылка на events из web_server_setting.cpp
extern AsyncEventSource events;

// Функция логирования
void logMessage(LogLevel level, const char *format, ...)
{
    // Проверяем, нужно ли выводить сообщение
    if (level > currentLogLevel)
    {
        return;
    }

    // Префиксы для разных уровней логирования
    const char *prefix;
    switch (level)
    {
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

    // Отправляем через SSE если клиент подключен
    if (events.count() > 0)
    {
        events.send((String(prefix)+ " "+ buffer).c_str(), "log");
    }
}
