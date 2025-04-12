#ifndef LCD_SETTING_H
#define LCD_SETTING_H

#include <Arduino.h>

// Определение кодов кнопок для LCD Keypad Shield
#define BUTTON_RIGHT 0
#define BUTTON_UP 1
#define BUTTON_DOWN 2
#define BUTTON_LEFT 3
#define BUTTON_SELECT 4
#define BUTTON_NONE 5

// Задержка для устранения дребезга контактов (мс)
#define BUTTON_DEBOUNCE_DELAY 200

// Определение кнопок LCD Keypad Shield
// Аналоговый пин для кнопок на ESP32-S3 UNO
#define KEYPAD_PIN 2  // GPIO2 соответствует A0 на arduino UNO

// Значения для ESP32-S3 (12-битный ADC, 0-4095)
// Эти значения требуют калибровки для конкретного устройства
#define KEY_RIGHT_VAL  0     // Значение около 0//0
#define KEY_UP_VAL     700   // Примерные значения//790
#define KEY_DOWN_VAL   1700  // Требуют калибровки//1860
#define KEY_LEFT_VAL   2800 //2937
#define KEY_SELECT_VAL 3000
#define KEY_NONE_VAL   4095  // Ни одна кнопка не нажата

// Допустимое отклонение для значений кнопок
#define KEY_THRESHOLD 200

// Переменные для прокрутки текста
extern std::string scrollText;
extern int scrollPosition;

// Инициализация LCD дисплея
void initLCD();

// Чтение состояния кнопок LCD Keypad Shield
int readKeypad();

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

// Метод для отображения текста с указанием столбца и строки
void displayText(const String& text, int column, int row, bool clearLine = false, bool center = false);

#endif // LCD_SETTING_H
