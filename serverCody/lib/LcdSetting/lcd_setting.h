#ifndef LCD_SETTING_H
#define LCD_SETTING_H

#include <Arduino.h>

// Определение пинов для кнопок
#define BUTTON_SELECT 13  // Кнопка выбора
#define BUTTON_LEFT   12  // Кнопка влево
#define BUTTON_UP     14  // Кнопка вверх
#define BUTTON_DOWN   27  // Кнопка вниз
#define BUTTON_RIGHT  26  // Кнопка вправо
#define BUTTON_RST    25  // Кнопка сброса

// Задержка для устранения дребезга контактов (мс)
#define BUTTON_DEBOUNCE_DELAY 200

// Переменные для прокрутки текста
extern std::string scrollText;
extern int scrollPosition;

// Инициализация LCD дисплея
void initLCD();

// Инициализация кнопок
void initButtons();

// Обновление текста для прокрутки
void updateScrollText();

// Обновление LCD дисплея
void updateLCD();

// Обработка нажатий кнопок
void handleButtons();

// Функция для периодического обновления информации на экране
void updateLCDTask();

// Функция для обновления данных на экране при изменении устройств
void refreshLCDData();

// Функция для отображения главного экрана
void showMainScreen();

// Функция для отображения списка устройств
void showDeviceList();

// Функция для отображения меню устройства
void showDeviceMenu();

// Функция для редактирования температуры
void showTemperatureEdit();

// Функция для редактирования GPIO
void showGpioEdit();

// Функция для включения/выключения устройства
void showEnabledEdit();

// Функция для отображения статистики обогрева
void showHeatingStats();

// Функция для отображения информации о GPIO
void showGpioInfo();

// Функция для отображения информации о температуре
void showTemperatureInfo();

// Функция для циклического переключения информационных экранов
void cycleInfoScreens();

// Функция для форматирования времени работы обогрева
String formatHeatingTime(unsigned long timeInMillis);

#endif // LCD_SETTING_H
