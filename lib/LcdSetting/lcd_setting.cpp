#include <lcd_setting.h>
#include <LiquidCrystal.h> // Используем стандартную библиотеку LiquidCrystal вместо I2C
#include <spiffs_setting.h>
#include <variables_info.h>
#include <WiFi.h>
#include <xiaomi_scanner.h>
#include <lcd_utils.h>
#include "logger.h"
// LCD Keypad Shield использует следующие пины для подключения LCD
// RS, E, D4, D5, D6, D7
// LiquidCrystal lcd(8, 9, 4, 5, 6, 7); // Стандартные пины для LCD Keypad Shield
// Для ESP32-S3 UNO с LCD Keypad Shield
LiquidCrystal lcd(21, 46, 19, 20, 3, 14); // Пины для ESP32-S3 UNO

// Переменные для управления подсветкой
bool backlightState = false;
unsigned long lastActivityTime = 0;

// Прокрутка текста
std::string scrollText = "";
int scrollPosition = 0;

// Состояния для меню
enum MenuState
{
  MAIN_SCREEN,      // Главный экран с информацией
  DEVICE_LIST,      // Список устройств
  DEVICE_MENU,      // Меню настроек устройства
  EDIT_TEMPERATURE, // Редактирование целевой температуры
  EDIT_GPIO,        // Редактирование GPIO пинов
  EDIT_ENABLED      // Включение/выключение устройства
};

// Текущее состояние меню
MenuState currentMenu = MAIN_SCREEN;

// Индексы для навигации
int deviceListIndex = 0;    // Индекс в списке устройств
int deviceMenuIndex = 0;    // Индекс в меню устройства
int gpioSelectionIndex = 0; // Индекс для выбора GPIO

// Опции меню устройства
const char *deviceMenuOptions[] = {"Temperature", "GPIO", "On/Off", "Back"};
const int deviceMenuOptionsCount = 5;

// Флаг для обновления экрана
bool needLcdUpdate = true;

// Функция для определения нажатой кнопки
int readKeypad()
{
  int adcValue = analogRead(KEYPAD_PIN);
  // LOG_I("Нажата кнопка, значение: %s", adcValue);
  if (adcValue == 0)
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

void initLCD()
{
  // Инициализация LCD
  lcd.begin(16, 2);

// Инициализация пина подсветки
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW); // По умолчанию подсветка выключена

  displayText("Initialization...");

  delay(1000);

  // Информация о навигации
  displayText("Navigation info:");
  displayText("LONG RIGHT=SELECT", 0, 1);
  delay(2000);

  // Настройка пина для считывания кнопок
  pinMode(KEYPAD_PIN, INPUT);
}

// Функция для включения подсветки
void turnOnBacklight() {
  if (!backlightState) {
    digitalWrite(BACKLIGHT_PIN, HIGH);
    backlightState = true;
  }
  lastActivityTime = millis();
}

// Функция для выключения подсветки
void turnOffBacklight() {
  if (backlightState) {
    digitalWrite(BACKLIGHT_PIN, LOW);
    backlightState = false;
  }
}

// Функция для автоматического управления подсветкой
void handleBacklight() {
  // Если подсветка включена и прошло больше времени, чем BACKLIGHT_TIMEOUT, выключаем её
  if (backlightState && (millis() - lastActivityTime > BACKLIGHT_TIMEOUT)) {
    turnOffBacklight();
  }
}

// Функция для отображения главного экрана
void showMainScreen()
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

  // Нижняя строка - информация о датчиках или прокручиваемый текст
  if (scrollText.length() > 0)
  {
    // Вычисляем, какую часть текста показать
    int endPos = scrollPosition + 16;
    if (endPos > scrollText.length())
    {
      // Если достигли конца текста, показываем начало
      std::string textPart = scrollText.substr(scrollPosition);
      textPart += scrollText.substr(0, endPos - scrollText.length());
      displayText(textPart.c_str(), 0, 1);
    }
    else
    {
      displayText(scrollText.substr(scrollPosition, 16).c_str(), 0, 1);
    }
  }
  else
  {
    displayText("LONG RIGHT=SELECT", 0, 1);
  }
}

// Функция для отображения списка устройств
void showDeviceList()
{
  displayText("Devices:");

  if (devices.size() > 0)
  {
    // Показываем текущее устройство с индикатором выбора
    displayText("> ", 0, 1);

    // Проверяем, не выходит ли индекс за пределы
    if (deviceListIndex >= devices.size())
    {
      deviceListIndex = 0;
    }

    // Отображаем имя устройства
    std::string deviceName = devices[deviceListIndex].name;
    // Ограничиваем длину имени, чтобы оно поместилось на экране
    if (deviceName.length() > 14)
    {
      deviceName = deviceName.substr(0, 14);
    }
    displayText(deviceName.c_str(), 0, 1);
  }
  else
  {
    displayText("There are no devices", 0, 1);
  }
}

// Функция для отображения меню устройства
void showDeviceMenu()
{
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

// Функция для редактирования температуры
void showTemperatureEdit()
{
  displayText("Temp:");
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
  for (int gpio : devices[deviceListIndex].gpioPins)
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
void updateLCD()
{
  if (!needLcdUpdate)
  {
    return;
  }

  switch (currentMenu)
  {
  case MAIN_SCREEN:
    showMainScreen();
    break;
  case DEVICE_LIST:
    showDeviceList();
    break;
  case DEVICE_MENU:
    showDeviceMenu();
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

  needLcdUpdate = false;
}

// Обновление текста для прокрутки
void initScrollText()
{
  scrollText = "";

  // Добавляем информацию о датчиках и устройствах
  for (const auto &device : devices)
  {
    if (device.isDataValid())
    {
      scrollText += (device.name + ": " + String(device.currentTemperature, 1).c_str() + "C").c_str();

      if (device.enabled)
      {
        scrollText += ("/" + String(device.targetTemperature, 1) + "C").c_str();

        // Добавляем статус обогрева
        if (device.heatingActive)
        {
          scrollText += "-(heating)";
        }
        else
        {
          scrollText += "-(OK)";
        }
      }

      scrollText += (" Hum: " + String(device.humidity, 1) + "% Battery: " + String(device.battery) + "% | ").c_str();
    }
    else if (device.enabled)
    {
      // Показываем только включенные устройства, которые не в сети
      scrollText += device.name + ": Not data | ";
    }
  }

  // Если текст пустой, добавляем информационное сообщение
  if (scrollText.length() == 0)
  {
    scrollText = "No active devices | Add devices via the web interface | ";
  }

  // Сбрасываем позицию прокрутки
  scrollPosition = 0;
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
  static unsigned long buttonPressStartTime = 0;
  static bool longPressHandled = false;

  unsigned long currentTime = millis();

  // Если нажата новая кнопка, сбрасываем таймер длительного нажатия
  if (pressedButton != lastButton)
  {
    buttonPressStartTime = currentTime;
    longPressHandled = false;
  }

  // Проверяем, не та же ли кнопка нажата и прошло ли достаточно времени (защита от дребезга)
  if (pressedButton == lastButton && currentTime - lastButtonTime < BUTTON_DEBOUNCE_DELAY)
  {
    return;
  }

  // Проверка длительного нажатия RIGHT (замена SELECT)
  if (pressedButton == BUTTON_RIGHT && !longPressHandled &&
      currentTime - buttonPressStartTime > 2000)
  {                                // 2000 мс для длительного нажатия
    pressedButton = BUTTON_SELECT; // Заменяем на SELECT
    longPressHandled = true;
  }

  // Обновляем время последнего нажатия и последнюю нажатую кнопку
  lastButtonTime = currentTime;
  lastButton = pressedButton;

  // Флаг для обновления экрана
  needLcdUpdate = true;

  // Обработка нажатий в зависимости от текущего состояния меню
  switch (currentMenu)
  {
  case MAIN_SCREEN:
    // На главном экране
    if (pressedButton == BUTTON_SELECT)
    {
      // Переход к списку устройств
      currentMenu = DEVICE_LIST;
    }
    else if (pressedButton == BUTTON_LEFT || pressedButton == BUTTON_RIGHT)
    {
      // Прокрутка текста влево/вправо
      if (pressedButton == BUTTON_LEFT)
      {
        scrollPosition = (scrollPosition + 1) % scrollText.length();
      }
      else if (pressedButton == BUTTON_RIGHT && !longPressHandled) // Проверяем, что это не длительное нажатие
      {
        scrollPosition = (scrollPosition + scrollText.length() - 1) % scrollText.length();
      }
    }
    break;

  case DEVICE_LIST:
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
      else if (pressedButton == BUTTON_SELECT)
      {
        // Выбор устройства - переход в меню устройства
        currentMenu = DEVICE_MENU;
        deviceMenuIndex = 0;
      }
      else if (pressedButton == BUTTON_LEFT)
      {
        // Возврат на главный экран
        currentMenu = MAIN_SCREEN;
      }
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      // Возврат на главный экран
      currentMenu = MAIN_SCREEN;
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
    else if (pressedButton == BUTTON_SELECT)
    {
      // Выбор пункта меню
      switch (deviceMenuIndex)
      {
      case 0: // Температура
        currentMenu = EDIT_TEMPERATURE;
        break;
      case 1: // GPIO пины
        currentMenu = EDIT_GPIO;
        gpioSelectionIndex = 0;
        break;
      case 2: // Вкл/Выкл
        currentMenu = EDIT_ENABLED;
        break;
      case 3: // Назад
        currentMenu = DEVICE_LIST;
        break;
      }
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      // Возврат к списку устройств
      currentMenu = DEVICE_LIST;
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
    else if (pressedButton == BUTTON_SELECT)
    {
      LOG_I("Нажата кнопка SELECT, сохраняем результаты");
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
      else if (pressedButton == BUTTON_SELECT)
      {
        // Выбор/отмена выбора текущего GPIO
        int selectedGpio = availableGpio[gpioSelectionIndex].pin;
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
        LOG_I("Нажата кнопка SELECT при редактироании GPIO, сохраняем результаты");
        // Сохраняем изменения
        saveClientsToFile();
      }
      else if (pressedButton == BUTTON_LEFT || pressedButton == BUTTON_RIGHT)
      {
        // Возврат в меню устройства
        currentMenu = DEVICE_MENU;
      }
    }
    else if (pressedButton == BUTTON_LEFT || pressedButton == BUTTON_SELECT)
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

      // Если устройство выключено, деактивируем обогрев
      if (!devices[deviceListIndex].enabled)
      {
        devices[deviceListIndex].heatingActive = false;
      }
      LOG_I("Нажата кнопка BUTTON_UP при изменении доступности устройства, сохраняем результаты");
      // Сохраняем изменения
      saveClientsToFile();
    }
    else if (pressedButton == BUTTON_SELECT || pressedButton == BUTTON_LEFT)
    {
      // Возврат в меню устройства
      currentMenu = DEVICE_MENU;
    }
    break;
  }

  // Обновляем дисплей
  updateLCD();
}

// Функция для прокрутки текста на главном экране
void scrollMainScreenText()
{
  static unsigned long lastScrollTime = 0;

  // Прокручиваем текст каждые 500 мс
  if (currentMenu == MAIN_SCREEN && millis() - lastScrollTime > SCROLL_DELAY)
  {
    scrollPosition = (scrollPosition + 1) % scrollText.length();
    lastScrollTime = millis();

    // Обновляем экран только если мы на главном экране
    if (currentMenu == MAIN_SCREEN)
    {
      needLcdUpdate = true;
    }
  }
}

// Функция для периодического обновления информации на экране
void updateLCDTask()
{
  // Прокрутка текста на главном экране
  scrollMainScreenText();

// Управление подсветкой
  handleBacklight();

  // Обновление экрана при необходимости
  if (needLcdUpdate)
  {
    updateLCD();
  }
}

// Функция для обновления данных на экране при изменении устройств
void refreshLCDData()
{
  // Обновляем текст для прокрутки
  initScrollText();

  // Устанавливаем флаг для обновления экрана
  needLcdUpdate = true;
}

// Обновление статуса устройств
void updateDevicesInformation()
{
  for (auto &device : devices)
  {
    // Проверяем, не устарели ли данные
    if (device.isDataValid())
    {
      device.isOnline = false;

      LOG_I("Устройство %s", device.name.c_str());
      LOG_I(" перешло в оффлайн (нет данных более 5 минут)");
    }
  }
  refreshLCDData();
}

// Функция для форматирования времени работы обогрева
String formatHeatingTime(unsigned long timeInMillis)
{
  unsigned long totalSeconds = timeInMillis / 1000;
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;

  char buffer[20];
  sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes % 60, seconds % 60);
  return String(buffer);
}

// Функция для отображения статистики обогрева
void showHeatingStats()
{
  if (devices.size() == 0 || deviceListIndex >= devices.size())
  {
    displayText("There are no devices");
    return;
  }

  const DeviceData &device = devices[deviceListIndex];

  // Показываем имя устройства
  std::string deviceName = device.name;
  if (deviceName.length() > 10)
  {
    deviceName = deviceName.substr(0, 10);
  }
  displayText(deviceName.c_str());

  // Показываем статус обогрева
  displayText(device.heatingActive ? "ON" : "OFF", 11, 0, false);

  // Показываем общее время работы
  displayText("Time: " + formatHeatingTime(device.totalHeatingTime), 0, 1);
}

// Функция для отображения информации о GPIO
void showGpioInfo()
{
  if (devices.size() == 0 || deviceListIndex >= devices.size())
  {
    displayText("There are no devices");
    return;
  }

  const DeviceData &device = devices[deviceListIndex];

  // Показываем имя устройства
  std::string deviceName = device.name;
  if (deviceName.length() > 16)
  {
    deviceName = deviceName.substr(0, 16);
  }
  displayText(deviceName.c_str());

  // Показываем GPIO пины
  if (device.gpioPins.size() == 0)
  {
    displayText("GPIO: Not selected", 0, 1);
  }
  else
  {
    String gpioList = "";
    for (size_t i = 0; i < device.gpioPins.size() && i < 4; i++)
    {
      if (i > 0)
      {
        gpioList += ",";
      }
      gpioList += String(device.gpioPins[i]);
    }

    if (device.gpioPins.size() > 4)
    {
      gpioList += "...";
    }
    displayText("GPIO: " + gpioList, 0, 1);
  }
}

// Функция для отображения информации о температуре
void showTemperatureInfo()
{
  lcd.clear();

  if (devices.size() == 0 || deviceListIndex >= devices.size())
  {
    displayText("There are no devices");
    return;
  }

  const DeviceData &device = devices[deviceListIndex];

  // Показываем имя устройства
  std::string deviceName = device.name;
  if (deviceName.length() > 10)
  {
    deviceName = deviceName.substr(0, 10);
  }
  displayText(deviceName.c_str());

  // Показываем текущую температуру
  displayText(String(device.currentTemperature, 1).c_str() + String("C"), 11, 0, false);

  // Показываем целевую температуру и статус
  displayText("Target: " + String(device.targetTemperature, 1) + "C ", 0, 1);

  // Статус обогрева
  if (device.enabled)
  {
    displayText(device.heatingActive ? "Heat" : "ОК", 12, 1, false);
  }
  else
  {
    displayText("OFF", 12, 1, false);
  }
}

// Функция для циклического переключения информационных экранов
void cycleInfoScreens()
{
  static int infoScreenIndex = 0;
  static unsigned long lastScreenChangeTime = 0;

  // Меняем экран каждые 5 секунд, только если мы на главном экране
  if (currentMenu == MAIN_SCREEN && millis() - lastScreenChangeTime > 5000)
  {
    infoScreenIndex = (infoScreenIndex + 1) % 3;
    lastScreenChangeTime = millis();

    switch (infoScreenIndex)
    {
    case 0:
      showMainScreen();
      break;
    case 1:
      showTemperatureInfo();
      break;
    case 2:
      showHeatingStats();
      break;
    }
  }
}

// Метод для отображения текста с указанием столбца и строки
void displayText(const String &text, int column, int row, bool clearLine, bool center)
{
  // Проверка корректности параметров
  if (row < 0 || row > 1)
  {
    LOG_I("Ошибка: Некорректный номер строки. Допустимые значения: 0 или 1");
    return;
  }

  // Если нужно очистить всю строку перед выводом
  if (clearLine)
  {
    lcd.setCursor(0, row);
    lcd.print("                "); // 16 пробелов для очистки строки
  }

  // Преобразуем текст в кодировку A02 для корректного отображения кириллицы
  String a02Text = utf8ToA02(text);

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
  lcd.print(a02Text);
}
