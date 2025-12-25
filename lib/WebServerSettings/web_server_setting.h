#ifndef WEB_SERVER_SETTING_H
#define WEB_SERVER_SETTING_H
#include <AsyncEventSource.h>
//extern AsyncEventSource events; // Добавляем внешнее объявление events

extern AsyncEventSource serialEvents; // Добавляем внешнее объявление serialEvents

void connectWiFi();
void initWebServer();

#endif