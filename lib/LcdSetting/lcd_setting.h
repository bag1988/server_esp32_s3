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
#define KEYPAD_PIN 2 // GPIO2 соответствует A0 на arduino UNO

// Значения для ESP32-S3 (12-битный ADC, 0-4095)
// Эти значения требуют калибровки для конкретного устройства
#define KEY_RIGHT_VAL 0   // Значение около 0//0
#define KEY_UP_VAL 700    // Примерные значения//790
#define KEY_DOWN_VAL 1800 // Требуют калибровки//1860
#define KEY_LEFT_VAL 2800 // 2937
#define KEY_SELECT_VAL 3000
#define KEY_NONE_VAL 4095 // Ни одна кнопка не нажата

// Допустимое отклонение для значений кнопок
#define KEY_THRESHOLD 200

// Добавляем пин для управления подсветкой
#define BACKLIGHT_PIN 10 // Пин для управления подсветкой (нужно выбрать подходящий пин)

// Таймер для автоматического отключения подсветки
#define BACKLIGHT_TIMEOUT 20000 // 20 секунд бездействия

// Инициализация LCD дисплея
void initLCD();

void disabledButtonForOta(bool isUpdate);

// Чтение состояния кнопок LCD Keypad Shield
int readKeypad();

// Обновление LCD дисплея
void updateMainScreenLCD();

// Обработка нажатий кнопок
void handleButtons();

// Функция для отображения списка устройств
void showDeviceList();

// Функция для отображения меню устройства
void showDeviceMenu();

void showInfoDevice();

// Функция для редактирования температуры
void showDeviceTemperatureEdit();

// Функция для редактирования GPIO
void showDeviceGpioEdit();

// Функция для включения/выключения устройства
void showDeviceEnabledEdit();

// Метод для отображения текста с указанием столбца и строки
void displayText(const String &text, int column = 0, int row = 0, bool clearLine = true, bool center = false);

#endif // LCD_SETTING_H
