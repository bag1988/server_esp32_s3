#include <lcd_setting.h>
#include <spiffs_setting.h>
#include <variables_info.h>
#include <WiFi.h>
#include <xiaomi_scanner.h>
#include <LiquidCrystal.h> // Используем стандартную библиотеку LiquidCrystal вместо I2C

// LCD Keypad Shield использует следующие пины для подключения LCD
// RS, E, D4, D5, D6, D7
// LiquidCrystal lcd(8, 9, 4, 5, 6, 7); // Стандартные пины для LCD Keypad Shield
// Для ESP32-S3 UNO с LCD Keypad Shield
LiquidCrystal lcd(21, 46, 19, 20, 3, 14); // Пины для ESP32-S3 UNO

// Переменные для управления подсветкой
bool backlightState = false;
unsigned long lastActivityTime = 0;

// Состояния для меню
enum MenuState
{
  MAIN_SCREEN,      // Главный экран с информацией
  DEVICE_MENU,      // Меню настроек устройства
  INFO_DEVICE,      // Отображение информации
  EDIT_TEMPERATURE, // Редактирование целевой температуры
  EDIT_GPIO,        // Редактирование GPIO пинов
  EDIT_ENABLED,     // Включение/выключение устройства
  OTA_UPDATE        // Обновление по OTA
};

// Текущее состояние меню
MenuState currentMenu = MAIN_SCREEN;

// Индексы для навигации
int deviceListIndex = 0;    // Индекс в списке устройств
int deviceMenuIndex = 0;    // Индекс в меню устройства
int gpioSelectionIndex = 0; // Индекс для выбора GPIO

// Опции меню устройства
const char *deviceMenuOptions[] = {"Info", "Target temp", "GPIO", "On/Off"};
const int deviceMenuOptionsCount = 4;

// Функция для определения нажатой кнопки
int readKeypad()
{
  int adcValue = analogRead(KEYPAD_PIN);
  if (adcValue > 10 && adcValue < KEY_UP_VAL)
  {
    return BUTTON_NONE;
  }
  else if (adcValue < KEY_RIGHT_VAL + KEY_THRESHOLD)
  {
    return BUTTON_RIGHT;
  }
  else if (adcValue < KEY_UP_VAL + KEY_THRESHOLD)
  {
    return BUTTON_UP;
  }
  else if (adcValue < KEY_DOWN_VAL + KEY_THRESHOLD)
  {
    return BUTTON_DOWN;
  }
  else if (adcValue < KEY_LEFT_VAL + KEY_THRESHOLD)
  {
    return BUTTON_LEFT;
  }

  return BUTTON_NONE; // Ни одна кнопка не нажата
}

// Функция для включения подсветки
void turnOnBacklight()
{
  if (!backlightState)
  {
    digitalWrite(BACKLIGHT_PIN, HIGH);
    backlightState = true;
  }
  lastActivityTime = millis();
}

// Функция для выключения подсветки
void turnOffBacklight()
{
  if (backlightState)
  {
    digitalWrite(BACKLIGHT_PIN, LOW);
    backlightState = false;
  }
}

// Функция для автоматического управления подсветкой
void handleBacklight()
{
  // Если подсветка включена и прошло больше времени, чем BACKLIGHT_TIMEOUT, выключаем её
  if (backlightState && (millis() - lastActivityTime > BACKLIGHT_TIMEOUT))
  {
    turnOffBacklight();
  }
}

void handleButtonsTaskFunction(void *parameter)
{
  for (;;)
  {
    handleButtons();
    handleBacklight();
    vTaskDelay(100 / portTICK_PERIOD_MS); // Небольшая задержка для предотвращения перегрузки CPU
  }
}

void initLCD()
{
  // Инициализация LCD
  lcd.begin(16, 2);

  // Инициализация пина подсветки
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW); // По умолчанию подсветка выключена

  displayText("Initialization...");

  vTaskDelay(1000 / portTICK_PERIOD_MS);

  // Настройка пина для считывания кнопок
  pinMode(KEYPAD_PIN, INPUT);

  xTaskCreate(
      handleButtonsTaskFunction,   // Функция
      "handleButtonsTaskFunction", // Имя
      2048,                        // Стек: 2048 слов = 8192 байт
      NULL,                        // Параметры
      1,                           // Приоритет
      NULL                         // Хэндл (не нужен)
  );
}

// Функция для отображения списка устройств
void showDeviceList()
{
  // Верхняя строка - статус системы
  // Отображаем статус WiFi
  if (wifiConnected)
  {
    displayText(WiFi.localIP().toString());
  }
  else
  {
    displayText("WiFi: Disabled");
  }

  if (devices.size() > 0)
  {
    // Проверяем, не выходит ли индекс за пределы
    if (deviceListIndex >= devices.size())
    {
      deviceListIndex = 0;
    }
    // Отображаем имя устройства
    std::string deviceName = devices[deviceListIndex].name;
    // Ограничиваем длину имени, чтобы оно поместилось на экране
    if (deviceName.length() > 9)
    {
      deviceName = deviceName.substr(0, 9);
    }
    if (devices[deviceListIndex].isDataValid())
    {
      deviceName += "*";
    }
    else
    {
      deviceName += "?";
    }
    deviceName += "-";
    deviceName += String(devices[deviceListIndex].currentTemperature, 1).c_str();
    deviceName += "C";
    displayText(deviceName.c_str(), 0, 1);
  }
  else
  {
    displayText("No devices", 0, 1);
  }
}

// Функция для отображения меню устройства
void showDeviceMenu()
{
  if (devices.size() > 0)
  {
    // Проверяем, не выходит ли индекс за пределы
    if (deviceListIndex >= devices.size())
    {
      return;
    }
    // Показываем имя устройства
    std::string deviceName = devices[deviceListIndex].name;
    if (deviceName.length() > 16)
    {
      deviceName = deviceName.substr(0, 16);
    }
    displayText(deviceName.c_str());
    // Показываем текущий пункт меню
    displayText("> " + String(deviceMenuOptions[deviceMenuIndex]), 0, 1);
  }
}

void showInfoDevice()
{
  displayText("Info:");
  if (devices.size() > 0)
  {
    // Проверяем, не выходит ли индекс за пределы
    if (deviceListIndex >= devices.size())
    {
      return;
    }
    // Отображаем имя устройства
    std::string deviceInfo = String(devices[deviceListIndex].currentTemperature, 1).c_str(); // 4 symbols
    deviceInfo += "C/";
    deviceInfo += String(devices[deviceListIndex].targetTemperature, 1).c_str();
    deviceInfo += "C  ";
    deviceInfo += String(devices[deviceListIndex].humidity).c_str();
    deviceInfo += "%";
    // Ограничиваем длину имени, чтобы оно поместилось на экране
    if (deviceInfo.length() > 10)
    {
      deviceInfo = deviceInfo.substr(0, 10);
    }
    displayText(deviceInfo.c_str(), 0, 1);
  }
}

// Функция для редактирования температуры
void showTemperatureEdit()
{
  displayText("Target temp:");
  displayText(String(devices[deviceListIndex].targetTemperature) + " C  [+/-]", 0, 1);
}

// Функция для редактирования GPIO
void showGpioEdit()
{
  // Проверяем, есть ли доступные GPIO
  if (availableGpio.size() == 0)
  {
    displayText("There are no available");
    displayText("GPIO pins", 0, 1);
    return;
  }

  // Проверяем индекс
  if (gpioSelectionIndex >= availableGpio.size())
  {
    gpioSelectionIndex = 0;
  }

  displayText("GPIO pins:");

  displayText("PIN " + String(availableGpio[gpioSelectionIndex].pin), 0, 1);

  // Проверяем, выбран ли этот GPIO для устройства
  bool isSelected = false;
  for (uint8_t gpio : devices[deviceListIndex].gpioPins)
  {
    if (gpio == availableGpio[gpioSelectionIndex].pin)
    {
      isSelected = true;
      break;
    }
  }
  displayText(isSelected ? "[X]" : "[ ]", 10, 1, false);
}

// Функция для включения/выключения устройства
void showEnabledEdit()
{
  displayText("Device:");
  displayText((devices[deviceListIndex].enabled ? "Enabled" : "Disabled") + String(" [+/-]"), 0, 1);
}

// Обновление LCD дисплея в зависимости от текущего состояния меню
void updateMainScreenLCD()
{
  switch (currentMenu)
  {
  case OTA_UPDATE:
    break;
  case MAIN_SCREEN:
    showDeviceList();
    break;
  case DEVICE_MENU:
    showDeviceMenu();
    break;
  case INFO_DEVICE:
    showInfoDevice();
    break;
  case EDIT_TEMPERATURE:
    showTemperatureEdit();
    break;
  case EDIT_GPIO:
    showGpioEdit();
    break;
  case EDIT_ENABLED:
    showEnabledEdit();
    break;
  }
}

void disabledButtonForOta(bool isUpdate)
{
  if (isUpdate)
  {
    currentMenu = OTA_UPDATE;
  }
  else
  {
    currentMenu = MAIN_SCREEN;
  }
  updateMainScreenLCD();
}

// Обработка нажатий кнопок
void handleButtons()
{
  // Чтение состояния кнопок
  int pressedButton = readKeypad();

  // Если ни одна кнопка не нажата, выходим
  if (pressedButton == BUTTON_NONE)
  {
    return;
  }

  // При любом нажатии включаем подсветку
  turnOnBacklight();

  // Обработка дребезга контактов и длительного нажатия
  static unsigned long lastButtonTime = 0;
  static int lastButton = BUTTON_NONE;

  unsigned long currentTime = millis();

  // Проверяем, не та же ли кнопка нажата и прошло ли достаточно времени (защита от дребезга)
  if (pressedButton == lastButton && currentTime - lastButtonTime < BUTTON_DEBOUNCE_DELAY)
  {
    return;
  }

  // Обновляем время последнего нажатия и последнюю нажатую кнопку
  lastButtonTime = currentTime;
  lastButton = pressedButton;

  // Обработка нажатий в зависимости от текущего состояния меню
  switch (currentMenu)
  {
  case OTA_UPDATE:
    break;
  case MAIN_SCREEN:
    // В списке устройств
    if (devices.size() > 0)
    {
      if (pressedButton == BUTTON_UP)
      {
        // Предыдущее устройство
        deviceListIndex = (deviceListIndex + devices.size() - 1) % devices.size();
      }
      else if (pressedButton == BUTTON_DOWN)
      {
        // Следующее устройство
        deviceListIndex = (deviceListIndex + 1) % devices.size();
      }
      else if (pressedButton == BUTTON_RIGHT)
      {
        // Выбор устройства - переход в меню устройства
        currentMenu = DEVICE_MENU;
        deviceMenuIndex = 0;
      }
    }
    break;

  case DEVICE_MENU:
    // В меню устройства
    if (pressedButton == BUTTON_UP)
    {
      // Предыдущий пункт меню
      deviceMenuIndex = (deviceMenuIndex + deviceMenuOptionsCount - 1) % deviceMenuOptionsCount;
    }
    else if (pressedButton == BUTTON_DOWN)
    {
      // Следующий пункт меню
      deviceMenuIndex = (deviceMenuIndex + 1) % deviceMenuOptionsCount;
    }
    else if (pressedButton == BUTTON_RIGHT)
    {
      // Выбор пункта меню
      switch (deviceMenuIndex)
      {
      case 0: // Info
        currentMenu = INFO_DEVICE;
        break;
      case 1: // Температура
        currentMenu = EDIT_TEMPERATURE;
        break;
      case 2: // GPIO пины
        currentMenu = EDIT_GPIO;
        gpioSelectionIndex = 0;
        break;
      case 3: // Вкл/Выкл
        currentMenu = EDIT_ENABLED;
        break;
      }
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      // Возврат к списку устройств
      currentMenu = MAIN_SCREEN;
    }
    break;

  case INFO_DEVICE:
    if (pressedButton == BUTTON_LEFT)
    {
      // Возврат на главный экран
      currentMenu = DEVICE_MENU;
    }
    break;

  case EDIT_TEMPERATURE:
    // Редактирование температуры
    if (pressedButton == BUTTON_UP)
    {
      // Увеличение температуры
      devices[deviceListIndex].targetTemperature += 0.5;
    }
    else if (pressedButton == BUTTON_DOWN)
    {
      // Уменьшение температуры
      devices[deviceListIndex].targetTemperature -= 0.5;
      if (devices[deviceListIndex].targetTemperature < 0)
      {
        devices[deviceListIndex].targetTemperature = 0;
      }
    }
    else if (pressedButton == BUTTON_RIGHT)
    {
      Serial.println("Нажата кнопка SELECT, сохраняем изменения температуры");
      // Сохранение и возврат в меню устройства
      saveClientsToFile();
      currentMenu = DEVICE_MENU;
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      // Отмена и возврат в меню устройства без сохранения
      currentMenu = DEVICE_MENU;
    }
    break;

  case EDIT_GPIO:
    // Редактирование GPIO
    if (availableGpio.size() > 0)
    {
      if (pressedButton == BUTTON_UP)
      {
        // Предыдущий GPIO
        gpioSelectionIndex = (gpioSelectionIndex + availableGpio.size() - 1) % availableGpio.size();
      }
      else if (pressedButton == BUTTON_DOWN)
      {
        // Следующий GPIO
        gpioSelectionIndex = (gpioSelectionIndex + 1) % availableGpio.size();
      }
      else if (pressedButton == BUTTON_RIGHT)
      {
        // Выбор/отмена выбора текущего GPIO
        uint8_t selectedGpio = availableGpio[gpioSelectionIndex].pin;
        bool isSelected = false;

        // Проверяем, выбран ли уже этот GPIO
        for (size_t i = 0; i < devices[deviceListIndex].gpioPins.size(); i++)
        {
          if (devices[deviceListIndex].gpioPins[i] == selectedGpio)
          {
            // Если выбран, удаляем его
            devices[deviceListIndex].gpioPins.erase(devices[deviceListIndex].gpioPins.begin() + i);
            isSelected = true;
            break;
          }
        }
        // Если не был выбран, добавляем
        if (!isSelected)
        {
          devices[deviceListIndex].gpioPins.push_back(selectedGpio);
        }
        Serial.println("Нажата кнопка SELECT при редактироании GPIO, сохраняем результаты");
        // Сохраняем изменения
        saveClientsToFile();
      }
      else if (pressedButton == BUTTON_LEFT)
      {
        // Возврат в меню устройства
        currentMenu = DEVICE_MENU;
      }
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      // Если нет доступных GPIO, возвращаемся в меню
      currentMenu = DEVICE_MENU;
    }
    break;

  case EDIT_ENABLED:
    // Включение/выключение устройства
    if (pressedButton == BUTTON_UP || pressedButton == BUTTON_DOWN)
    {
      // Переключение состояния
      devices[deviceListIndex].enabled = !devices[deviceListIndex].enabled;
    }
    else if (pressedButton == BUTTON_RIGHT)
    {
      Serial.println("Нажата кнопка BUTTON_RIGHT при изменении доступности устройства, сохраняем результаты");
      // Сохраняем изменения
      saveClientsToFile();
      currentMenu = DEVICE_MENU;
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      // Возврат в меню устройства
      currentMenu = DEVICE_MENU;
    }
    break;
  }
  // Обновляем дисплей
  updateMainScreenLCD();
}

// Метод для отображения текста с указанием столбца и строки
void displayText(const String &text, int column, int row, bool clearLine, bool center)
{
  // Проверка корректности параметров
  if (row < 0 || row > 1)
  {
    Serial.println("Ошибка: Некорректный номер строки. Допустимые значения: 0 или 1");
    return;
  }

  // Если нужно очистить всю строку перед выводом
  if (clearLine)
  {
    lcd.setCursor(0, row);
    lcd.print("                "); // 16 пробелов для очистки строки
  }

  // Если нужно центрировать текст
  if (center)
  {
    int textLength = text.length();
    if (textLength < 16)
    {
      column = (16 - textLength) / 2;
    }
    else
    {
      column = 0;
    }
  }

  // Ограничиваем столбец допустимыми значениями
  column = constrain(column, 0, 15);

  // Устанавливаем курсор и выводим текст
  lcd.setCursor(column, row);
  lcd.print(text);
}