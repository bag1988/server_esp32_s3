#include <variables_info.h>
String formatHeatingTime(unsigned long timeInMillis)
{
    unsigned long totalSeconds = timeInMillis / 1000;
    unsigned long days = totalSeconds / 86400;
    unsigned long hours = (totalSeconds % 86400) / 3600;
    unsigned long minutes = (totalSeconds % 3600) / 60;
    unsigned long seconds = totalSeconds % 60;
    char buffer[30];
    sprintf(buffer, "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
    return String(buffer);
}

// Function to send data to both Serial and SSE
void logAndSend(const String& message)
{
    Serial.println(message); // Выводим в Serial как обычно
    if (serialEvents.count() > 0) {
        // Если есть подключенные клиенты, отправляем им сообщение
        serialEvents.send(message.c_str(), "log", millis());
    }
}

// Function to send formatted data to both Serial and SSE
void logAndSendf(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    String message = String(buffer);
    Serial.println(message); // Выводим в Serial как обычно
    if (serialEvents.count() > 0) {
        // Если есть подключенные клиенты, отправляем им сообщение
        serialEvents.send(message.c_str(), "log", millis());
    }
}

