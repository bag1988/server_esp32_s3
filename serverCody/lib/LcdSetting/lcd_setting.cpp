#include <lcd_setting.h>
#include <LiquidCrystal_I2C.h>
#include <spiffs_setting.h>
#include <variables_info.h>
#include <WiFi.h>
#include <xiaomi_scanner.h>
// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

void initLCD()
{
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");
}

void initButtons()
{
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_RST, INPUT_PULLUP);
}

// Обновление текста для прокрутки
void updateScrollText()
{
  scrollText = "";

  // Добавляем информацию о WiFi
  if (wifiConnected)
  {
    scrollText += "WiFi: " + WiFi.localIP().toString() + " | ";
  }
  else
  {
    scrollText += "WiFi: Отключен | ";
  }

  // Добавляем информацию о датчиках и устройствах
  for (const auto &device : devices)
  {
    if (device.isDataValid())
    {
      scrollText += device.name + ": " + String(device.currentTemperature) + "C ";

      if (device.enabled)
      {
        scrollText += "/ " + String(device.targetTemperature) + "C ";

        // Добавляем статус обогрева
        if (device.needsHeating())
        {
          scrollText += "(Обогрев) ";
        }
        else
        {
          scrollText += "(OK) ";
        }
      }

      scrollText += "Влаж: " + String(device.humidity) + "% ";
      scrollText += "Бат: " + String(device.battery) + "% | ";
    }
    else if (device.enabled)
    {
      // Показываем только включенные устройства, которые не в сети
      scrollText += device.name + ": Нет данных | ";
    }
  }

  // Если текст пустой, добавляем информационное сообщение
  if (scrollText.length() == 0)
  {
    scrollText = "Нет активных устройств | Добавьте устройства через веб-интерфейс | ";
  }

  // Сбрасываем позицию прокрутки
  scrollPosition = 0;
}

// Обновление LCD дисплея
void updateLCD()
{
  lcd.clear();

  // Верхняя строка - статус системы
  lcd.setCursor(0, 0);

  // Отображаем статус WiFi
  if (wifiConnected)
  {
    lcd.print("WiFi: ");
    lcd.print(WiFi.localIP().toString());
  }
  else
  {
    lcd.print("WiFi: Отключен");
  }

  // Нижняя строка - информация о датчиках или прокручиваемый текст
  lcd.setCursor(0, 1);

  if (isEditing)
  {
    // Если в режиме редактирования, показываем текущее выбранное устройство
    if (devices.size() > 0 && selectedDeviceIndex < devices.size())
    {
      lcd.print(devices[selectedDeviceIndex].name);
      lcd.print(" ");

      // Показываем параметр, который редактируется
      switch (currentEditMode)
      {
      case EDIT_NAME:
        lcd.print("Имя");
        break;

      case EDIT_TEMPERATURE:
        lcd.print("Temp:");
        lcd.print(devices[selectedDeviceIndex].targetTemperature);
        lcd.print("C");
        break;

      case EDIT_GPIO:
        lcd.print("GPIO:");
        if (gpioSelectionIndex < availableGpio.size())
        {
          lcd.print(availableGpio[gpioSelectionIndex]);

          // Показываем, выбран ли этот GPIO
          bool isSelected = false;
          for (int gpio : devices[selectedDeviceIndex].gpioPins)
          {
            if (gpio == availableGpio[gpioSelectionIndex])
            {
              isSelected = true;
              break;
            }
          }
          lcd.print(isSelected ? " [X]" : " [ ]");
        }
        break;

      case EDIT_ENABLED:
        lcd.print("Enabled: ");
        lcd.print(devices[selectedDeviceIndex].enabled ? "Yes" : "No");
        break;
      }
    }
    else
    {
      lcd.print("Нет устройств");
    }
  }
  else
  {
    // В обычном режиме показываем прокручиваемый текст
    if (scrollText.length() > 0)
    {
      // Вычисляем, какую часть текста показать
      int endPos = scrollPosition + 16;
      if (endPos > scrollText.length())
      {
        // Если достигли конца текста, показываем начало
        String textPart = scrollText.substring(scrollPosition);
        textPart += scrollText.substring(0, endPos - scrollText.length());
        lcd.print(textPart);
      }
      else
      {
        lcd.print(scrollText.substring(scrollPosition, endPos));
      }
    }
    else
    {
      lcd.print("Нет данных");
    }
  }
}

// Обработка нажатий кнопок
void handleButtons()
{
  // Чтение состояния кнопок
  bool upPressed = digitalRead(BUTTON_UP) == LOW;
  bool downPressed = digitalRead(BUTTON_DOWN) == LOW;
  bool selectPressed = digitalRead(BUTTON_SELECT) == LOW;

  // Обработка дребезга контактов
  static unsigned long lastButtonTime = 0;
  if (millis() - lastButtonTime < BUTTON_DEBOUNCE_DELAY)
  {
    return;
  }

  // Если ни одна кнопка не нажата, выходим
  if (!upPressed && !downPressed && !selectPressed)
  {
    return;
  }

  // Обновляем время последнего нажатия
  lastButtonTime = millis();

  // Обработка нажатий в зависимости от режима
  if (isEditing)
  {
    // Режим редактирования
    if (upPressed)
    {
      // Кнопка "Вверх"
      switch (currentEditMode)
      {
      case EDIT_NAME:
        // Имя можно редактировать только через веб-интерфейс
        break;

      case EDIT_TEMPERATURE:
        devices[selectedDeviceIndex].targetTemperature += 0.5;
        break;

      case EDIT_GPIO:
        gpioSelectionIndex = (gpioSelectionIndex + 1) % availableGpio.size();
        break;

      case EDIT_ENABLED:
        devices[selectedDeviceIndex].enabled = !devices[selectedDeviceIndex].enabled;
        break;
      }
    }
    else if (downPressed)
    {
      // Кнопка "Вниз"
      switch (currentEditMode)
      {
      case EDIT_NAME:
        // Имя можно редактировать только через веб-интерфейс
        break;

      case EDIT_TEMPERATURE:
        devices[selectedDeviceIndex].targetTemperature -= 0.5;
        if (devices[selectedDeviceIndex].targetTemperature < 0)
        {
          devices[selectedDeviceIndex].targetTemperature = 0;
        }
        break;

      case EDIT_GPIO:
        gpioSelectionIndex = (gpioSelectionIndex + availableGpio.size() - 1) % availableGpio.size();
        break;

      case EDIT_ENABLED:
        devices[selectedDeviceIndex].enabled = !devices[selectedDeviceIndex].enabled;
        break;
      }
    }
    else if (selectPressed)
    {
      // Кнопка "Выбор"
      if (currentEditMode == EDIT_GPIO)
      {
        // Переключаем выбор GPIO
        int selectedGpio = availableGpio[gpioSelectionIndex];
        bool isSelected = false;

        // Проверяем, выбран ли уже этот GPIO
        for (size_t i = 0; i < devices[selectedDeviceIndex].gpioPins.size(); i++)
        {
          if (devices[selectedDeviceIndex].gpioPins[i] == selectedGpio)
          {
            // Если выбран, удаляем его
            devices[selectedDeviceIndex].gpioPins.erase(devices[selectedDeviceIndex].gpioPins.begin() + i);
            isSelected = true;
            break;
          }
        }

        // Если не был выбран, добавляем
        if (!isSelected)
        {
          devices[selectedDeviceIndex].gpioPins.push_back(selectedGpio);
        }

        // Переходим к следующему параметру
        currentEditMode = EDIT_ENABLED;
      }
      else if (currentEditMode == EDIT_ENABLED)
      {
        // Завершаем редактирование
        isEditing = false;
        saveDevicesToFile();
        updateScrollText();
      }
      else
      {
        // Переходим к следующему параметру
        currentEditMode = static_cast<EditMode>((static_cast<int>(currentEditMode) + 1) % 4);
      }
    }
  }
  else
  {
    // Обычный режим просмотра
    if (upPressed)
    {
      // Переходим к предыдущему устройству
      if (devices.size() > 0)
      {
        selectedDeviceIndex = (selectedDeviceIndex + devices.size() - 1) % devices.size();
        isEditing = true;
        currentEditMode = EDIT_TEMPERATURE;
      }
    }
    else if (downPressed)
    {
      // Переходим к следующему устройству
      if (devices.size() > 0)
      {
        selectedDeviceIndex = (selectedDeviceIndex + 1) % devices.size();
        isEditing = true;
        currentEditMode = EDIT_TEMPERATURE;
      }
    }
    else if (selectPressed)
    {
      // Начинаем редактирование текущего устройства
      if (devices.size() > 0)
      {
        isEditing = true;
        currentEditMode = EDIT_TEMPERATURE;
      }
    }
  }

  // Обновляем дисплей
  updateLCD();
}
