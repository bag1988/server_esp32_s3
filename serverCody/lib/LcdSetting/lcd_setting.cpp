#include <lcd_setting.h>
#include <LiquidCrystal_I2C.h>
#include <spiffs_setting.h>
#include <variables_info.h>
#include <WiFi.h>
#include <xiaomi_scanner.h>

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Прокрутка текста
std::string scrollText = "";
int scrollPosition = 0;
// Состояния для меню
enum MenuState {
  MAIN_SCREEN,       // Главный экран с информацией
  DEVICE_LIST,       // Список устройств
  DEVICE_MENU,       // Меню настроек устройства
  EDIT_TEMPERATURE,  // Редактирование целевой температуры
  EDIT_GPIO,         // Редактирование GPIO пинов
  EDIT_ENABLED       // Включение/выключение устройства
};

// Текущее состояние меню
MenuState currentMenu = MAIN_SCREEN;

// Индексы для навигации
int deviceListIndex = 0;      // Индекс в списке устройств
int deviceMenuIndex = 0;      // Индекс в меню устройства
int gpioSelectionIndex = 0;   // Индекс для выбора GPIO

// Опции меню устройства
const char* deviceMenuOptions[] = {"Температура", "GPIO пины", "Вкл/Выкл", "Назад"};
const int deviceMenuOptionsCount = 4;

// Флаг для обновления экрана
bool needLcdUpdate = true;

void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Инициализация...");
  
  // Инициализация кнопок
  initButtons();
}

void initButtons() {
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_RST, INPUT_PULLUP);
}

// Функция для отображения главного экрана
void showMainScreen() {
  lcd.clear();
  
  // Верхняя строка - статус системы
  lcd.setCursor(0, 0);
  
  // Отображаем статус WiFi
  if (wifiConnected) {
    lcd.print("WiFi: ");
    lcd.print(WiFi.localIP().toString());
  } else {
    lcd.print("WiFi: Отключен");
  }
  
  // Нижняя строка - информация о датчиках или прокручиваемый текст
  lcd.setCursor(0, 1);
  
  if (scrollText.length() > 0) {
    // Вычисляем, какую часть текста показать
    int endPos = scrollPosition + 16;
    if (endPos > scrollText.length()) {
      // Если достигли конца текста, показываем начало
      std::string textPart = scrollText.substr(scrollPosition);
      textPart += scrollText.substr(0, endPos - scrollText.length());
      lcd.print(textPart.c_str());
    } else {
      lcd.print(scrollText.substr(scrollPosition, 16).c_str());
    }
  } else {
    lcd.print("Нет данных");
  }
}

// Функция для отображения списка устройств
void showDeviceList() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Устройства:");
  
  lcd.setCursor(0, 1);
  if (devices.size() > 0) {
    // Показываем текущее устройство с индикатором выбора
    lcd.print("> ");
    
    // Проверяем, не выходит ли индекс за пределы
    if (deviceListIndex >= devices.size()) {
      deviceListIndex = 0;
    }
    
    // Отображаем имя устройства
    std::string deviceName = devices[deviceListIndex].name;
    // Ограничиваем длину имени, чтобы оно поместилось на экране
    if (deviceName.length() > 14) {
      deviceName = deviceName.substr(0, 14);
    }
    lcd.print(deviceName.c_str());
  } else {
    lcd.print("Нет устройств");
  }
}

// Функция для отображения меню устройства
void showDeviceMenu() {
  lcd.clear();
  
  // Показываем имя устройства
  lcd.setCursor(0, 0);
  std::string deviceName = devices[deviceListIndex].name;
  if (deviceName.length() > 16) {
    deviceName = deviceName.substr(0, 16);
  }
  lcd.print(deviceName.c_str());
  
  // Показываем текущий пункт меню
  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(deviceMenuOptions[deviceMenuIndex]);
}

// Функция для редактирования температуры
void showTemperatureEdit() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Температура:");
  
  lcd.setCursor(0, 1);
  lcd.print(devices[deviceListIndex].targetTemperature);
  lcd.print(" C  [+/-]");
}

// Функция для редактирования GPIO
void showGpioEdit() {
  lcd.clear();
  
  // Проверяем, есть ли доступные GPIO
  if (availableGpio.size() == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Нет доступных");
    lcd.setCursor(0, 1);
    lcd.print("GPIO пинов");
    return;
  }
  
  // Проверяем индекс
  if (gpioSelectionIndex >= availableGpio.size()) {
    gpioSelectionIndex = 0;
  }
  
  lcd.setCursor(0, 0);
  lcd.print("GPIO пин:");
  
  lcd.setCursor(0, 1);
  lcd.print("PIN ");
  lcd.print(availableGpio[gpioSelectionIndex].pin);
  
  // Проверяем, выбран ли этот GPIO для устройства
  bool isSelected = false;
  for (int gpio : devices[deviceListIndex].gpioPins) {
    if (gpio == availableGpio[gpioSelectionIndex].pin) {
      isSelected = true;
      break;
    }
  }
  
  lcd.setCursor(10, 1);
  lcd.print(isSelected ? "[X]" : "[ ]");
}

// Функция для включения/выключения устройства
void showEnabledEdit() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Устройство:");
  
  lcd.setCursor(0, 1);
  lcd.print(devices[deviceListIndex].enabled ? "Включено" : "Выключено");
  lcd.print(" [+/-]");
}

// Обновление LCD дисплея в зависимости от текущего состояния меню
void updateLCD() {
  if (!needLcdUpdate) {
    return;
  }
  
  switch (currentMenu) {
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
void updateScrollText() {
  scrollText = "";
  // Добавляем информацию о WiFi
  if (wifiConnected) {
    scrollText += "WiFi: %S | ", WiFi.localIP().toString().c_str();
  } else {
    scrollText += "WiFi: Отключен | ";
  }

  // Добавляем информацию о датчиках и устройствах
  for (const auto &device : devices) {
    if (device.isDataValid()) {
      scrollText += ("%S: %S C",device.name, String(device.currentTemperature, 1).c_str());

      if (device.enabled) {
        scrollText += ("/ %S C", String(device.targetTemperature, 1).c_str());

        // Добавляем статус обогрева
        if (device.heatingActive) {
          scrollText += "(Обогрев) ";
        } else {
          scrollText += "(OK) ";
        }
      }

      scrollText += ("Влаж: %S % Бат: % | ", String(device.humidity).c_str(), String(device.battery).c_str());      
    } else if (device.enabled) {
      // Показываем только включенные устройства, которые не в сети
      scrollText += device.name + ": Нет данных | ";
    }
  }

  // Если текст пустой, добавляем информационное сообщение
  if (scrollText.length() == 0) {
    scrollText = "Нет активных устройств | Добавьте устройства через веб-интерфейс | ";
  }

  // Сбрасываем позицию прокрутки
  scrollPosition = 0;
}

// Обработка нажатий кнопок
void handleButtons() {
  // Чтение состояния кнопок
  bool leftPressed = digitalRead(BUTTON_LEFT) == LOW;
  bool rightPressed = digitalRead(BUTTON_RIGHT) == LOW;
  bool upPressed = digitalRead(BUTTON_UP) == LOW;
  bool downPressed = digitalRead(BUTTON_DOWN) == LOW;
  bool selectPressed = digitalRead(BUTTON_SELECT) == LOW;
  bool resetPressed = digitalRead(BUTTON_RST) == LOW;

  // Обработка дребезга контактов
  static unsigned long lastButtonTime = 0;
  if (millis() - lastButtonTime < BUTTON_DEBOUNCE_DELAY) {
    return;
  }

  // Если ни одна кнопка не нажата, выходим
  if (!leftPressed && !rightPressed && !upPressed && !downPressed && !selectPressed && !resetPressed) {
    return;
  }

  // Обновляем время последнего нажатия
  lastButtonTime = millis();
  
  // Флаг для обновления экрана
  needLcdUpdate = true;

  // Обработка кнопки сброса - всегда возвращает на главный экран
  if (resetPressed) {
    currentMenu = MAIN_SCREEN;
    updateScrollText();
    return;
  }

  // Обработка нажатий в зависимости от текущего состояния меню
  switch (currentMenu) {
    case MAIN_SCREEN:
      // На главном экране
      if (selectPressed) {
        // Переход к списку устройств
        currentMenu = DEVICE_LIST;
      } else if (leftPressed || rightPressed) {
        // Прокрутка текста влево/вправо
        if (leftPressed) {
          scrollPosition = (scrollPosition + 1) % scrollText.length();
        } else {
          scrollPosition = (scrollPosition + scrollText.length() - 1) % scrollText.length();
        }
      }
      break;
      
    case DEVICE_LIST:
      // В списке устройств
      if (devices.size() > 0) {
        if (upPressed) {
          // Предыдущее устройство
          deviceListIndex = (deviceListIndex + devices.size() - 1) % devices.size();
        } else if (downPressed) {
          // Следующее устройство
          deviceListIndex = (deviceListIndex + 1) % devices.size();
        } else if (selectPressed) {
          // Выбор устройства - переход в меню устройства
          currentMenu = DEVICE_MENU;
          deviceMenuIndex = 0;
        } else if (leftPressed) {
          // Возврат на главный экран
          currentMenu = MAIN_SCREEN;
        }
      } else if (leftPressed) {
        // Возврат на главный экран
        currentMenu = MAIN_SCREEN;
      }
      break;
      
    case DEVICE_MENU:
      // В меню устройства
      if (upPressed) {
        // Предыдущий пункт меню
        deviceMenuIndex = (deviceMenuIndex + deviceMenuOptionsCount - 1) % deviceMenuOptionsCount;
      } else if (downPressed) {
        // Следующий пункт меню
        deviceMenuIndex = (deviceMenuIndex + 1) % deviceMenuOptionsCount;
      } else if (selectPressed) {
        // Выбор пункта меню
        switch (deviceMenuIndex) {
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
      } else if (leftPressed) {
        // Возврат к списку устройств
        currentMenu = DEVICE_LIST;
      }
      break;
      
    case EDIT_TEMPERATURE:
      // Редактирование температуры
      if (upPressed) {
        // Увеличение температуры
        devices[deviceListIndex].targetTemperature += 0.5;
      } else if (downPressed) {
        // Уменьшение температуры
        devices[deviceListIndex].targetTemperature -= 0.5;
        if (devices[deviceListIndex].targetTemperature < 0) {
          devices[deviceListIndex].targetTemperature = 0;
        }
      } else if (selectPressed || leftPressed) {
        // Сохранение и возврат в меню устройства
        saveClientsToFile();
        currentMenu = DEVICE_MENU;
      }
      break;
      
    case EDIT_GPIO:
      // Редактирование GPIO
      if (availableGpio.size() > 0) {
        if (upPressed) {
          // Предыдущий GPIO
          gpioSelectionIndex = (gpioSelectionIndex + availableGpio.size() - 1) % availableGpio.size();
        } else if (downPressed) {
          // Следующий GPIO
          gpioSelectionIndex = (gpioSelectionIndex + 1) % availableGpio.size();
        } else if (selectPressed) {
          // Выбор/отмена выбора текущего GPIO
          int selectedGpio = availableGpio[gpioSelectionIndex].pin;
          bool isSelected = false;
          
          // Проверяем, выбран ли уже этот GPIO
          for (size_t i = 0; i < devices[deviceListIndex].gpioPins.size(); i++) {
            if (devices[deviceListIndex].gpioPins[i] == selectedGpio) {
              // Если выбран, удаляем его
              devices[deviceListIndex].gpioPins.erase(devices[deviceListIndex].gpioPins.begin() + i);
              isSelected = true;
              break;
            }
          }
          
          // Если не был выбран, добавляем
          if (!isSelected) {
            devices[deviceListIndex].gpioPins.push_back(selectedGpio);
          }
          
          // Сохраняем изменения
          saveClientsToFile();
        } else if (leftPressed || rightPressed) {
          // Возврат в меню устройства
          currentMenu = DEVICE_MENU;
        }
      } else if (leftPressed || selectPressed) {
        // Если нет доступных GPIO, возвращаемся в меню
        currentMenu = DEVICE_MENU;
      }
      break;
      
    case EDIT_ENABLED:
      // Включение/выключение устройства
      if (upPressed || downPressed) {
        // Переключение состояния
        devices[deviceListIndex].enabled = !devices[deviceListIndex].enabled;
        
        // Если устройство выключено, деактивируем обогрев
        if (!devices[deviceListIndex].enabled) {
          devices[deviceListIndex].heatingActive = false;
        }
        
        // Сохраняем изменения
        saveClientsToFile();
      } else if (selectPressed || leftPressed) {
        // Возврат в меню устройства
        currentMenu = DEVICE_MENU;
      }
      break;
  }

  // Обновляем дисплей
  updateLCD();
}

// Функция для прокрутки текста на главном экране
void scrollMainScreenText() {
  static unsigned long lastScrollTime = 0;
  
  // Прокручиваем текст каждые 500 мс
  if (currentMenu == MAIN_SCREEN && millis() - lastScrollTime > SCROLL_DELAY) {
    scrollPosition = (scrollPosition + 1) % scrollText.length();
    lastScrollTime = millis();
    
    // Обновляем экран только если мы на главном экране
    if (currentMenu == MAIN_SCREEN) {
      needLcdUpdate = true;
    }
  }
}

// Функция для периодического обновления информации на экране
void updateLCDTask() {
  // Прокрутка текста на главном экране
  scrollMainScreenText();
  
  // Обновление экрана при необходимости
  if (needLcdUpdate) {
    updateLCD();
  }
}

// Функция для обновления данных на экране при изменении устройств
void refreshLCDData() {
  // Обновляем текст для прокрутки
  updateScrollText();
  
  // Устанавливаем флаг для обновления экрана
  needLcdUpdate = true;
}

// Функция для форматирования времени работы обогрева
String formatHeatingTime(unsigned long timeInMillis) {
  unsigned long seconds = timeInMillis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  char buffer[20];
  sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes % 60, seconds % 60);
  return String(buffer);
}

// Функция для отображения статистики обогрева
void showHeatingStats() {
  lcd.clear();
  
  if (devices.size() == 0 || deviceListIndex >= devices.size()) {
    lcd.setCursor(0, 0);
    lcd.print("Нет устройств");
    return;
  }
  
  const DeviceData& device = devices[deviceListIndex];
  
  // Показываем имя устройства
  lcd.setCursor(0, 0);
  std::string deviceName = device.name;
  if (deviceName.length() > 10) {
    deviceName = deviceName.substr(0, 10);
  }
  lcd.print(deviceName.c_str());
  
  // Показываем статус обогрева
  lcd.setCursor(11, 0);
  lcd.print(device.heatingActive ? "ВКЛ" : "ВЫКЛ");
  
  // Показываем общее время работы
  lcd.setCursor(0, 1);
  lcd.print("Время: ");
  lcd.print(formatHeatingTime(device.totalHeatingTime));
}

// Функция для отображения информации о GPIO
void showGpioInfo() {
  lcd.clear();
  
  if (devices.size() == 0 || deviceListIndex >= devices.size()) {
    lcd.setCursor(0, 0);
    lcd.print("Нет устройств");
    return;
  }
  
  const DeviceData& device = devices[deviceListIndex];
  
  // Показываем имя устройства
  lcd.setCursor(0, 0);
  std::string deviceName = device.name;
  if (deviceName.length() > 16) {
    deviceName = deviceName.substr(0, 16);
  }
  lcd.print(deviceName.c_str());
  
  // Показываем GPIO пины
  lcd.setCursor(0, 1);
  lcd.print("GPIO: ");
  
  if (device.gpioPins.size() == 0) {
    lcd.print("Не выбраны");
  } else {
    String gpioList = "";
    for (size_t i = 0; i < device.gpioPins.size() && i < 4; i++) {
      if (i > 0) {
        gpioList += ",";
      }
      gpioList += String(device.gpioPins[i]);
    }
    
    if (device.gpioPins.size() > 4) {
      gpioList += "...";
    }
    
    lcd.print(gpioList);
  }
}

// Функция для отображения информации о температуре
void showTemperatureInfo() {
  lcd.clear();
  
  if (devices.size() == 0 || deviceListIndex >= devices.size()) {
    lcd.setCursor(0, 0);
    lcd.print("Нет устройств");
    return;
  }
  
  const DeviceData& device = devices[deviceListIndex];
  
  // Показываем имя устройства
  lcd.setCursor(0, 0);
  std::string deviceName = device.name;
  if (deviceName.length() > 10) {
    deviceName = deviceName.substr(0, 10);
  }
  lcd.print(deviceName.c_str());
  
  // Показываем текущую температуру
  lcd.setCursor(11, 0);
  lcd.print(String(device.currentTemperature, 1).c_str());
  lcd.print("C");
  
  // Показываем целевую температуру и статус
  lcd.setCursor(0, 1);
  lcd.print("Цель: ");
  lcd.print(String(device.targetTemperature, 1));
  lcd.print("C ");
  
  // Статус обогрева
  lcd.setCursor(12, 1);
  if (device.enabled) {
    lcd.print(device.heatingActive ? "НАГР" : "ОК");
  } else {
    lcd.print("ВЫКЛ");
  }
}

// Функция для циклического переключения информационных экранов
void cycleInfoScreens() {
  static int infoScreenIndex = 0;
  static unsigned long lastScreenChangeTime = 0;
  
  // Меняем экран каждые 5 секунд, только если мы на главном экране
  if (currentMenu == MAIN_SCREEN && millis() - lastScreenChangeTime > 5000) {
    infoScreenIndex = (infoScreenIndex + 1) % 3;
    lastScreenChangeTime = millis();
    
    switch (infoScreenIndex) {
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

