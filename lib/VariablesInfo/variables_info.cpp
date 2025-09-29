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