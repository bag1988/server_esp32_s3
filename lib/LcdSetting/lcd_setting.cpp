#include <lcd_setting.h>
#include <LiquidCrystal.h> // Используем стандартную библиотеку LiquidCrystal вместо I2C
#include <spiffs_setting.h>
#include <variables_info.h>
#include <WiFi.h>
#include <xiaomi_scanner.h>

// LCD Keypad Shield использует следующие пины для подключения LCD
// RS, E, D4, D5, D6, D7
// LiquidCrystal lcd(8, 9, 4, 5, 6, 7); // Стандартные пины для LCD Keypad Shield
// Для ESP32-S3 UNO с LCD Keypad Shield
LiquidCrystal lcd(21, 46, 19, 20, 3, 14); // Пины для ESP32-S3 UNO
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
  // Serial.print("Нажата кнопка, значение: ");
  // Serial.println(adcValue);
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
  lcd.clear();
  lcd.print("Initialization...");

  delay(1000);

  // Информация о навигации
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Navigation info:");
  lcd.setCursor(0, 1);
  lcd.print("LONG RIGHT=SELECT");
  delay(2000);

  // Настройка пина для считывания кнопок
  pinMode(KEYPAD_PIN, INPUT);
}

// Функция для отображения главного экрана
void showMainScreen()
{
  lcd.clear();

  // Верхняя строка - статус системы
  lcd.setCursor(0, 0);

  // Отображаем статус WiFi
  if (wifiConnected)
  {
    lcd.print(WiFi.localIP().toString());
  }
  else
  {
    lcd.print("WiFi: Disabled");
  }

  // Нижняя строка - информация о датчиках или прокручиваемый текст
  lcd.setCursor(0, 1);

  if (scrollText.length() > 0)
  {
    // Вычисляем, какую часть текста показать
    int endPos = scrollPosition + 16;
    if (endPos > scrollText.length())
    {
      // Если достигли конца текста, показываем начало
      std::string textPart = scrollText.substr(scrollPosition);
      textPart += scrollText.substr(0, endPos - scrollText.length());
      lcd.print(textPart.c_str());
    }
    else
    {
      lcd.print(scrollText.substr(scrollPosition, 16).c_str());
    }
  }
  else
  {
    lcd.print("LONG RIGHT=SELECT");
  }
}

// Функция для отображения списка устройств
void showDeviceList()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Devices:");

  lcd.setCursor(0, 1);
  if (devices.size() > 0)
  {
    // Показываем текущее устройство с индикатором выбора
    lcd.print("> ");

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
    lcd.print(deviceName.c_str());
  }
  else
  {
    lcd.print("There are no devices");
  }
}

// Функция для отображения меню устройства
void showDeviceMenu()
{
  lcd.clear();

  // Показываем имя устройства
  lcd.setCursor(0, 0);
  std::string deviceName = devices[deviceListIndex].name;
  if (deviceName.length() > 16)
  {
    deviceName = deviceName.substr(0, 16);
  }
  lcd.print(deviceName.c_str());

  // Показываем текущий пункт меню
  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(deviceMenuOptions[deviceMenuIndex]);
}

// Функция для редактирования температуры
void showTemperatureEdit()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp:");

  lcd.setCursor(0, 1);
  lcd.print(devices[deviceListIndex].targetTemperature);
  lcd.print(" C  [+/-]");
}

// Функция для редактирования GPIO
void showGpioEdit()
{
  lcd.clear();

  // Проверяем, есть ли доступные GPIO
  if (availableGpio.size() == 0)
  {
    lcd.setCursor(0, 0);
    lcd.print("There are no available");
    lcd.setCursor(0, 1);
    lcd.print("GPIO pins");
    return;
  }

  // Проверяем индекс
  if (gpioSelectionIndex >= availableGpio.size())
  {
    gpioSelectionIndex = 0;
  }

  lcd.setCursor(0, 0);
  lcd.print("GPIO pins:");

  lcd.setCursor(0, 1);
  lcd.print("PIN ");
  lcd.print(availableGpio[gpioSelectionIndex].pin);

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

  lcd.setCursor(10, 1);
  lcd.print(isSelected ? "[X]" : "[ ]");
}

// Функция для включения/выключения устройства
void showEnabledEdit()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Device:");

  lcd.setCursor(0, 1);
  lcd.print(devices[deviceListIndex].enabled ? "Enabled" : "Disabled");
  lcd.print(" [+/-]");
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
void updateScrollText()
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
      currentTime - buttonPressStartTime > 1000)
  {                                // 1000 мс для длительного нажатия
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
      Serial.println("Нажата кнопка SELECT, сохраняем результаты");
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
        Serial.println("Нажата кнопка SELECT при редактироании GPIO, сохраняем результаты");
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
      Serial.println("Нажата кнопка BUTTON_UP при изменении доступности устройства, сохраняем результаты");
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
  updateScrollText();

  // Устанавливаем флаг для обновления экрана
  needLcdUpdate = true;
}

// Функция для форматирования времени работы обогрева
String formatHeatingTime(unsigned long timeInMillis)
{
  unsigned long seconds = timeInMillis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;

  char buffer[20];
  sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes % 60, seconds % 60);
  return String(buffer);
}

// Функция для отображения статистики обогрева
void showHeatingStats()
{
  lcd.clear();

  if (devices.size() == 0 || deviceListIndex >= devices.size())
  {
    lcd.setCursor(0, 0);
    lcd.print("There are no devices");
    return;
  }

  const DeviceData &device = devices[deviceListIndex];

  // Показываем имя устройства
  lcd.setCursor(0, 0);
  std::string deviceName = device.name;
  if (deviceName.length() > 10)
  {
    deviceName = deviceName.substr(0, 10);
  }
  lcd.print(deviceName.c_str());

  // Показываем статус обогрева
  lcd.setCursor(11, 0);
  lcd.print(device.heatingActive ? "ON" : "OFF");

  // Показываем общее время работы
  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  lcd.print(formatHeatingTime(device.totalHeatingTime));
}

// Функция для отображения информации о GPIO
void showGpioInfo()
{
  lcd.clear();

  if (devices.size() == 0 || deviceListIndex >= devices.size())
  {
    lcd.setCursor(0, 0);
    lcd.print("There are no devices");
    return;
  }

  const DeviceData &device = devices[deviceListIndex];

  // Показываем имя устройства
  lcd.setCursor(0, 0);
  std::string deviceName = device.name;
  if (deviceName.length() > 16)
  {
    deviceName = deviceName.substr(0, 16);
  }
  lcd.print(deviceName.c_str());

  // Показываем GPIO пины
  lcd.setCursor(0, 1);
  lcd.print("GPIO: ");

  if (device.gpioPins.size() == 0)
  {
    lcd.print("Not selected");
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

    lcd.print(gpioList);
  }
}

// Функция для отображения информации о температуре
void showTemperatureInfo()
{
  lcd.clear();

  if (devices.size() == 0 || deviceListIndex >= devices.size())
  {
    lcd.setCursor(0, 0);
    lcd.print("There are no devices");
    return;
  }

  const DeviceData &device = devices[deviceListIndex];

  // Показываем имя устройства
  lcd.setCursor(0, 0);
  std::string deviceName = device.name;
  if (deviceName.length() > 10)
  {
    deviceName = deviceName.substr(0, 10);
  }
  lcd.print(deviceName.c_str());

  // Показываем текущую температуру
  lcd.setCursor(11, 0);
  lcd.print(String(device.currentTemperature, 1).c_str());
  lcd.print("C");

  // Показываем целевую температуру и статус
  lcd.setCursor(0, 1);
  lcd.print("Target: ");
  lcd.print(String(device.targetTemperature, 1));
  lcd.print("C ");

  // Статус обогрева
  lcd.setCursor(12, 1);
  if (device.enabled)
  {
    lcd.print(device.heatingActive ? "Heat" : "ОК");
  }
  else
  {
    lcd.print("OFF");
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
