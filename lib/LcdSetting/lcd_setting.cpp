#include <lcd_setting.h>
#include <spiffs_setting.h>
#include <variables_info.h>
#include <WiFi.h>
#include <xiaomi_scanner.h>
#include <device_manager.h>
#include <LiquidCrystal.h>

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
  DEVICE_LIST,             // Главный экран со списком устройств
  DEVICE_MENU,             // Меню настроек устройства
  DEVICE_INFO,             // Отображение информации
  DEVICE_EDIT_TEMPERATURE, // Редактирование целевой температуры
  DEVICE_EDIT_GPIO,        // Редактирование GPIO пинов
  DEVICE_EDIT_ENABLED,     // Включение/выключение устройства
  VIEW_GPIO,
  EDIT_GPIO,
  EDIT_HYSTERESIS,
  OTA_UPDATE // Обновление по OTA
};

// Текущее состояние меню
MenuState currentMenu = DEVICE_LIST;

// Индексы для навигации
int deviceListIndex = 0;    // Индекс в списке устройств
int deviceMenuIndex = 0;    // Индекс в меню устройства
int gpioSelectionIndex = 0; // Индекс для выбора GPIO
int gpioMenuIndex = 0;
// Опции меню устройства
const char *deviceMenuOptions[] = {"Info", "Target temp", "GPIO", "On/Off"};
const int deviceMenuOptionsCount = 4;

const char *gpioMenuOptions[] = {"Auto", "On", "Off"};
const int gpioMenuOptionsCount = 3;

float editHysteresisTemp = 1.5;

// Функция для определения нажатой кнопки
int readKeypad()
{
  int adcValue = analogRead(KEYPAD_PIN);
  // logAndSend("Нажата кнопка, значение: " + String(adcValue));

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
bool turnOnBacklight()
{
  lastActivityTime = millis();
  if (!backlightState)
  {
    digitalWrite(BACKLIGHT_PIN, HIGH);
    backlightState = true;
    return true;
  }
  return false;
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
  DeviceResponse response;
  DeviceCmd cmd{};
  cmd.type = DeviceCmdType::CMD_GET_DEVICES;
  sendDeviceCmd(cmd, &response);

  if (wifiConnected)
  {
    displayText(WiFi.localIP().toString());
  }
  else
  {
    displayText("WiFi: Disabled");
  }

  if (response.success && response.devices.size() > 0)
  {
    if (deviceListIndex >= response.devices.size())
    {
      deviceListIndex = 0;
    }
    std::string deviceName = response.devices[deviceListIndex].name;

    if (!response.devices[deviceListIndex].isDataValid())
    {
      deviceName = "?" + deviceName;
    }
    else if (response.devices[deviceListIndex].heatingActive)
    {
      deviceName = "*" + deviceName;
    }

    if (deviceName.length() > 10)
    {
      deviceName = deviceName.substr(0, 10);
    }

    deviceName += "-";
    deviceName += String(response.devices[deviceListIndex].currentTemperature, 1).c_str();
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
  DeviceResponse response;
  DeviceCmd cmd{};
  cmd.type = DeviceCmdType::CMD_GET_DEVICES;
  sendDeviceCmd(cmd, &response);

  if (response.success && response.devices.size() > 0)
  {
    if (deviceListIndex >= response.devices.size())
    {
      return;
    }
    std::string deviceName = response.devices[deviceListIndex].name;
    if (deviceName.length() > 16)
    {
      deviceName = deviceName.substr(0, 16);
    }
    displayText(deviceName.c_str());
    displayText("> " + String(deviceMenuOptions[deviceMenuIndex]), 0, 1);
  }
}

void showInfoDevice()
{
  DeviceResponse response;
  DeviceCmd cmd{};
  cmd.type = DeviceCmdType::CMD_GET_DEVICES;
  sendDeviceCmd(cmd, &response);

  displayText("Info:");
  if (response.success && response.devices.size() > 0)
  {
    if (deviceListIndex >= response.devices.size())
    {
      return;
    }
    std::string deviceInfo = String(response.devices[deviceListIndex].currentTemperature, 1).c_str();
    deviceInfo += "C/";
    deviceInfo += String(response.devices[deviceListIndex].targetTemperature, 1).c_str();
    deviceInfo += "C  ";
    deviceInfo += String(response.devices[deviceListIndex].humidity).c_str();
    deviceInfo += "%";
    if (deviceInfo.length() > 10)
    {
      deviceInfo = deviceInfo.substr(0, 10);
    }
    displayText(deviceInfo.c_str(), 0, 1);
  }
}

// Функция для редактирования температуры
void showDeviceTemperatureEdit()
{
  DeviceResponse response;
  DeviceCmd cmd{};
  cmd.type = DeviceCmdType::CMD_GET_DEVICES;
  sendDeviceCmd(cmd, &response);

  if (!response.success || deviceListIndex >= response.devices.size())
  {
    displayText("No devices");
    displayText("available", 0, 1);
    return;
  }
  displayText("Target temp:");
  displayText(String(response.devices[deviceListIndex].targetTemperature) + " C  [+/-]", 0, 1);
}

// Функция для редактирования GPIO
void showDeviceGpioEdit()
{
  DeviceResponse gpioResponse;
  DeviceCmd gpioCmd{};
  gpioCmd.type = DeviceCmdType::CMD_GET_GPIO_STATS;
  sendDeviceCmd(gpioCmd, &gpioResponse);

  DeviceResponse deviceResponse;
  DeviceCmd deviceCmd{};
  deviceCmd.type = DeviceCmdType::CMD_GET_DEVICES;
  sendDeviceCmd(deviceCmd, &deviceResponse);

  if (!gpioResponse.success || gpioResponse.gpioPins.size() == 0)
  {
    displayText("There are no available");
    displayText("GPIO pins", 0, 1);
    return;
  }

  if (gpioSelectionIndex >= gpioResponse.gpioPins.size())
  {
    gpioSelectionIndex = 0;
  }

  displayText("GPIO pins:");

  displayText("PIN " + String(gpioResponse.gpioPins[gpioSelectionIndex].pin), 0, 1);

  bool isSelected = false;
  if (deviceResponse.success && deviceListIndex < deviceResponse.devices.size()) {
    for (uint8_t gpio : deviceResponse.devices[deviceListIndex].gpioPins)
    {
      if (gpio == gpioResponse.gpioPins[gpioSelectionIndex].pin)
      {
        isSelected = true;
        break;
      }
    }
  }
  displayText(isSelected ? "[X]" : "[ ]", 10, 1, false);
}

void showViewEdit()
{
  DeviceResponse response;
  DeviceCmd cmd{};
  cmd.type = DeviceCmdType::CMD_GET_GPIO_STATS;
  sendDeviceCmd(cmd, &response);

  displayText("Select GPIO");
  if (response.success && response.gpioPins.size() > 0)
  {
    if (gpioSelectionIndex >= response.gpioPins.size())
    {
      return;
    }
    auto &gpio = response.gpioPins[gpioSelectionIndex];
    gpioMenuIndex = gpio.state;
    std::string gpioName = gpio.name;
    if (gpioName.length() > 16)
    {
      gpioName = gpioName.substr(0, 16);
    }
    displayText(gpioName.c_str(), 0, 1);
    displayText(gpioMenuOptions[gpioMenuIndex], 11, 1, false);
  }
  else
  {
    displayText("No gpio", 0, 1);
  }
}

void showGpioEdit()
{
  DeviceResponse response;
  DeviceCmd cmd{};
  cmd.type = DeviceCmdType::CMD_GET_GPIO_STATS;
  sendDeviceCmd(cmd, &response);

  if (response.success && response.gpioPins.size() > 0)
  {
    if (gpioSelectionIndex >= response.gpioPins.size())
    {
      return;
    }
    std::string gpioName = response.gpioPins[gpioSelectionIndex].name;
    if (gpioName.length() > 16)
    {
      gpioName = gpioName.substr(0, 16);
    }
    displayText(gpioName.c_str());
    displayText("> " + String(gpioMenuOptions[gpioMenuIndex]), 0, 1);
  }
}

void showEditHysteresis()
{
  displayText("Edit Hysteresis:");
  displayText(">" + String(editHysteresisTemp) + " [+/-]", 0, 1);
}

// Функция для включения/выключения устройства
void showDeviceEnabledEdit()
{
  DeviceResponse response;
  DeviceCmd cmd{};
  cmd.type = DeviceCmdType::CMD_GET_DEVICES;
  sendDeviceCmd(cmd, &response);

  if (!response.success || deviceListIndex >= response.devices.size())
  {
    displayText("No devices");
    displayText("available", 0, 1);
    return;
  }
  displayText("Device:");
  displayText((response.devices[deviceListIndex].enabled ? "Enabled" : "Disabled") + String(" [+/-]"), 0, 1);
}

// Обновление LCD дисплея в зависимости от текущего состояния меню
void updateMainScreenLCD()
{
  switch (currentMenu)
  {
  case OTA_UPDATE:
    break;
  case DEVICE_LIST:
    showDeviceList();
    break;
  case DEVICE_MENU:
    showDeviceMenu();
    break;
  case DEVICE_INFO:
    showInfoDevice();
    break;
  case DEVICE_EDIT_TEMPERATURE:
    showDeviceTemperatureEdit();
    break;
  case DEVICE_EDIT_GPIO:
    showDeviceGpioEdit();
    break;
  case VIEW_GPIO:
    showViewEdit();
    break;
  case EDIT_GPIO:
    showGpioEdit();
    break;
  case EDIT_HYSTERESIS:
    showEditHysteresis();
    break;
  case DEVICE_EDIT_ENABLED:
    showDeviceEnabledEdit();
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
    currentMenu = DEVICE_LIST;
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
  if (turnOnBacklight())
  {
    updateMainScreenLCD();
    return;
  }

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
case DEVICE_LIST:
    // В списке устройств
    {
      DeviceResponse response;
      DeviceCmd getCmd{};
      getCmd.type = DeviceCmdType::CMD_GET_DEVICES;
      sendDeviceCmd(getCmd, &response);

      if (response.success && response.devices.size() > 0)
      {
        if (pressedButton == BUTTON_UP)
        {
          deviceListIndex = (deviceListIndex + response.devices.size() - 1) % response.devices.size();
        }
        else if (pressedButton == BUTTON_DOWN)
        {
          deviceListIndex = (deviceListIndex + 1) % response.devices.size();
        }
        else if (pressedButton == BUTTON_RIGHT)
        {
          currentMenu = DEVICE_MENU;
          deviceMenuIndex = 0;
        }
        else if (pressedButton == BUTTON_LEFT)
        {
          currentMenu = VIEW_GPIO;
          gpioSelectionIndex = 0;
        }
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
        currentMenu = DEVICE_INFO;
        break;
      case 1: // Температура
        currentMenu = DEVICE_EDIT_TEMPERATURE;
        break;
      case 2: // GPIO пины
        currentMenu = DEVICE_EDIT_GPIO;
        gpioSelectionIndex = 0;
        break;
      case 3: // Вкл/Выкл
        currentMenu = DEVICE_EDIT_ENABLED;
        break;
      }
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      // Возврат к списку устройств
      currentMenu = DEVICE_LIST;
    }
    break;

  case DEVICE_INFO:
    if (pressedButton == BUTTON_LEFT || pressedButton == BUTTON_RIGHT)
    {
      // Возврат на главный экран
      currentMenu = DEVICE_MENU;
    }
    break;

  case DEVICE_EDIT_TEMPERATURE:
    // Редактирование температуры
    {
      DeviceResponse response;
      DeviceCmd getCmd{};
      getCmd.type = DeviceCmdType::CMD_GET_DEVICES;
      sendDeviceCmd(getCmd, &response);

      if (!response.success || deviceListIndex >= response.devices.size())
      {
        currentMenu = DEVICE_LIST;
        break;
      }
      if (pressedButton == BUTTON_UP)
      {
        float newTemp = response.devices[deviceListIndex].targetTemperature + 0.5;
        DeviceCmd setCmd{};
        setCmd.type = DeviceCmdType::CMD_UPDATE_DEVICE_CONFIG;
        setCmd.macAddress = response.devices[deviceListIndex].macAddress;
        setCmd.targetTemperature = newTemp;
        sendDeviceCmdAsync(setCmd);
      }
      else if (pressedButton == BUTTON_DOWN)
      {
        float newTemp = response.devices[deviceListIndex].targetTemperature - 0.5;
        if (newTemp < 0) newTemp = 0;
        DeviceCmd setCmd{};
        setCmd.type = DeviceCmdType::CMD_UPDATE_DEVICE_CONFIG;
        setCmd.macAddress = response.devices[deviceListIndex].macAddress;
        setCmd.targetTemperature = newTemp;
        sendDeviceCmdAsync(setCmd);
      }
      else if (pressedButton == BUTTON_RIGHT)
      {
        logAndSend("Нажата кнопка SELECT, сохраняем изменения температуры");
        currentMenu = DEVICE_MENU;
      }
      else if (pressedButton == BUTTON_LEFT)
      {
        currentMenu = DEVICE_MENU;
      }
    }
    break;

  case DEVICE_EDIT_GPIO:
    // Редактирование GPIO
    {
      DeviceResponse gpioResponse;
      DeviceCmd gpioCmd{};
      gpioCmd.type = DeviceCmdType::CMD_GET_GPIO_STATS;
      sendDeviceCmd(gpioCmd, &gpioResponse);

      DeviceResponse deviceResponse;
      DeviceCmd deviceCmd{};
      deviceCmd.type = DeviceCmdType::CMD_GET_DEVICES;
      sendDeviceCmd(deviceCmd, &deviceResponse);

      if (!deviceResponse.success || deviceListIndex >= deviceResponse.devices.size())
      {
        currentMenu = DEVICE_LIST;
        break;
      }

      if (gpioResponse.success && gpioResponse.gpioPins.size() > 0)
      {
        if (pressedButton == BUTTON_UP)
        {
          gpioSelectionIndex = (gpioSelectionIndex + gpioResponse.gpioPins.size() - 1) % gpioResponse.gpioPins.size();
        }
        else if (pressedButton == BUTTON_DOWN)
        {
          gpioSelectionIndex = (gpioSelectionIndex + 1) % gpioResponse.gpioPins.size();
        }
        else if (pressedButton == BUTTON_RIGHT)
        {
          uint8_t selectedGpio = gpioResponse.gpioPins[gpioSelectionIndex].pin;
          bool isSelected = false;

          for (size_t i = 0; i < deviceResponse.devices[deviceListIndex].gpioPins.size(); i++)
          {
            if (deviceResponse.devices[deviceListIndex].gpioPins[i] == selectedGpio)
            {
              isSelected = true;
              break;
            }
          }

          DeviceCmd setCmd{};
          setCmd.type = DeviceCmdType::CMD_UPDATE_DEVICE_CONFIG;
          setCmd.macAddress = deviceResponse.devices[deviceListIndex].macAddress;
          setCmd.gpioPins = deviceResponse.devices[deviceListIndex].gpioPins;

          if (!isSelected) {
            setCmd.gpioPins.push_back(selectedGpio);
          } else {
            setCmd.gpioPins.erase(
              std::remove(setCmd.gpioPins.begin(), setCmd.gpioPins.end(), selectedGpio),
              setCmd.gpioPins.end()
            );
          }

          sendDeviceCmdAsync(setCmd);
          logAndSend("Нажата кнопка SELECT при редактировании GPIO, сохраняем результаты");
        }
        else if (pressedButton == BUTTON_LEFT)
        {
          currentMenu = DEVICE_MENU;
        }
      }
      else
      {
        currentMenu = DEVICE_MENU;
      }
    }
    break;

  case DEVICE_EDIT_ENABLED:
    // Включение/выключение устройства
    {
      DeviceResponse response;
      DeviceCmd getCmd{};
      getCmd.type = DeviceCmdType::CMD_GET_DEVICES;
      sendDeviceCmd(getCmd, &response);

      if (!response.success || deviceListIndex >= response.devices.size())
      {
        currentMenu = DEVICE_LIST;
        break;
      }

      if (pressedButton == BUTTON_UP || pressedButton == BUTTON_DOWN)
      {
        DeviceCmd setCmd{};
        setCmd.type = DeviceCmdType::CMD_UPDATE_DEVICE_CONFIG;
        setCmd.macAddress = response.devices[deviceListIndex].macAddress;
        setCmd.enabled = !response.devices[deviceListIndex].enabled;
        sendDeviceCmdAsync(setCmd);
      }
      else if (pressedButton == BUTTON_RIGHT)
      {
        logAndSend("Нажата кнопка BUTTON_RIGHT при изменении доступности устройства, сохраняем результаты");
        currentMenu = DEVICE_MENU;
      }
      else if (pressedButton == BUTTON_LEFT)
      {
        currentMenu = DEVICE_MENU;
      }
    }
    break;

  case VIEW_GPIO:
    {
      DeviceResponse response;
      DeviceCmd getCmd{};
      getCmd.type = DeviceCmdType::CMD_GET_GPIO_STATS;
      sendDeviceCmd(getCmd, &response);

      if (response.success && response.gpioPins.size() > 0)
      {
        if (pressedButton == BUTTON_UP)
        {
          gpioSelectionIndex = (gpioSelectionIndex + response.gpioPins.size() - 1) % response.gpioPins.size();
        }
        else if (pressedButton == BUTTON_DOWN)
        {
          gpioSelectionIndex = (gpioSelectionIndex + 1) % response.gpioPins.size();
        }
        else if (pressedButton == BUTTON_RIGHT)
        {
          currentMenu = EDIT_GPIO;
        }
        else if (pressedButton == BUTTON_LEFT)
        {
          editHysteresisTemp = hysteresisTemp;
          currentMenu = EDIT_HYSTERESIS;
        }
      }
      else
      {
        editHysteresisTemp = hysteresisTemp;
        currentMenu = EDIT_HYSTERESIS;
      }
    }
    break;

  case EDIT_GPIO:
    // В меню устройства
    if (pressedButton == BUTTON_UP)
    {
      // Предыдущий пункт меню
      gpioMenuIndex = (gpioMenuIndex + gpioMenuOptionsCount - 1) % gpioMenuOptionsCount;
    }
    else if (pressedButton == BUTTON_DOWN)
    {
      // Следующий пункт меню
      gpioMenuIndex = (gpioMenuIndex + 1) % gpioMenuOptionsCount;
    }
    else if (pressedButton == BUTTON_RIGHT)
    {
      DeviceResponse response;
      DeviceCmd getCmd{};
      getCmd.type = DeviceCmdType::CMD_GET_GPIO_STATS;
      sendDeviceCmd(getCmd, &response);
      
      if (response.success && gpioSelectionIndex < response.gpioPins.size()) {
        if (response.gpioPins[gpioSelectionIndex].state != gpioMenuIndex)
        {
          DeviceCmd setCmd{};
          setCmd.type = DeviceCmdType::CMD_UPDATE_GPIO_CONFIG;
          setCmd.gpioPin = response.gpioPins[gpioSelectionIndex].pin;
          setCmd.gpioState = gpioMenuIndex;
          sendDeviceCmdAsync(setCmd);
        }
      }
      currentMenu = VIEW_GPIO;
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      currentMenu = VIEW_GPIO;
    }
    break;

  case EDIT_HYSTERESIS:

    if (pressedButton == BUTTON_UP)
    {
      // Увеличение температуры
      editHysteresisTemp += 0.1;
    }
    else if (pressedButton == BUTTON_DOWN)
    {
      // Уменьшение температуры
      editHysteresisTemp -= 0.1;
      if (editHysteresisTemp < 0)
      {
        editHysteresisTemp = 0;
      }
    }
    else if (pressedButton == BUTTON_RIGHT)
    {
      logAndSend("Нажата кнопка SELECT, сохраняем гестерезис температуры");
      hysteresisTemp = editHysteresisTemp;
      SpiffsSetting::saveServerSetting();
    }
    else if (pressedButton == BUTTON_LEFT)
    {
      currentMenu = DEVICE_LIST;
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
    logAndSend("Ошибка: Некорректный номер строки. Допустимые значения: 0 или 1");
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