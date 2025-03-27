#include <lcd_setting.h>
#include <LiquidCrystal_I2C.h>
#include <spiffs_setting.h>
#include <variables_info.h>
#include <WiFi.h>
#include <xiaomi_ble_scanner.h>
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

  // Добавляем информацию о датчиках
  for (auto &pair : xiaomiSensors)
  {
    XiaomiSensorData &sensor = pair.second;
    if (sensor.isOnline && (millis() - sensor.lastUpdate < 300000))
    {
      scrollText += sensor.name + ": " + String(sensor.temperature) + "C " +
                    String(sensor.humidity) + "% Бат:" + String(sensor.battery) + "% | ";
    }
  }

  // Добавляем информацию о клиентах
  for (const auto &client : clients)
  {
    if (client.enabled)
    {
      // Находим соответствующий датчик
      bool sensorFound = false;
      for (auto &pair : xiaomiSensors)
      {
        XiaomiSensorData &sensor = pair.second;
        if (sensor.address.c_str() == client.address && sensor.isOnline)
        {
          scrollText += String(client.name.c_str()) + String(": ") + String(sensor.temperature) + "C/" +
                        String(client.targetTemperature) + "C ";

          // Добавляем статус обогрева
          if ((sensor.temperature + 2) < client.targetTemperature)
          {
            scrollText += "(Обогрев) | ";
          }
          else
          {
            scrollText += "(OK) | ";
          }

          sensorFound = true;
          break;
        }
      }

      // Если датчик не найден
      if (!sensorFound)
      {
        scrollText += (" %S: Нет данных | ", client.name).c_str();
      }
    }
  }

  // Если текст пустой, добавляем информационное сообщение
  if (scrollText.length() == 0)
  {
    scrollText = "Нет активных датчиков и клиентов | Добавьте датчики через веб-интерфейс | ";
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
    // Если в режиме редактирования, показываем текущий выбранный клиент
    if (clients.size() > 0 && selectedClientIndex < clients.size())
    {
      lcd.print(clients[selectedClientIndex].name.c_str());
      lcd.print(" ");

      // Показываем параметр, который редактируется
      switch (currentEditMode)
      {
      case EDIT_TEMPERATURE:
        lcd.print("Temp:");
        lcd.print(clients[selectedClientIndex].targetTemperature);
        lcd.print("C");
        break;

      case EDIT_GPIO:
        lcd.print("GPIO:");
        if (gpioSelectionIndex < availableGpio.size())
        {
          lcd.print(availableGpio[gpioSelectionIndex]);

          // Показываем, выбран ли этот GPIO
          bool isSelected = false;
          for (int gpio : clients[selectedClientIndex].gpioPins)
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
        lcd.print(clients[selectedClientIndex].enabled ? "Yes" : "No");
        break;
      }
    }
    else
    {
      lcd.print("Нет клиентов");
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
      case EDIT_TEMPERATURE:
        clients[selectedClientIndex].targetTemperature += 0.5;
        break;

      case EDIT_GPIO:
        gpioSelectionIndex = (gpioSelectionIndex + 1) % availableGpio.size();
        break;

      case EDIT_ENABLED:
        clients[selectedClientIndex].enabled = !clients[selectedClientIndex].enabled;
        break;
      }
    }
    else if (downPressed)
    {
      // Кнопка "Вниз"
      switch (currentEditMode)
      {
      case EDIT_TEMPERATURE:
        clients[selectedClientIndex].targetTemperature -= 0.5;
        if (clients[selectedClientIndex].targetTemperature < 0)
        {
          clients[selectedClientIndex].targetTemperature = 0;
        }
        break;

      case EDIT_GPIO:
        gpioSelectionIndex = (gpioSelectionIndex + availableGpio.size() - 1) % availableGpio.size();
        break;

      case EDIT_ENABLED:
        clients[selectedClientIndex].enabled = !clients[selectedClientIndex].enabled;
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
        for (size_t i = 0; i < clients[selectedClientIndex].gpioPins.size(); i++)
        {
          if (clients[selectedClientIndex].gpioPins[i] == selectedGpio)
          {
            // Если выбран, удаляем его
            clients[selectedClientIndex].gpioPins.erase(clients[selectedClientIndex].gpioPins.begin() + i);
            isSelected = true;
            break;
          }
        }

        // Если не был выбран, добавляем
        if (!isSelected)
        {
          clients[selectedClientIndex].gpioPins.push_back(selectedGpio);
        }

        // Переходим к следующему параметру
        currentEditMode = EDIT_ENABLED;
      }
      else if (currentEditMode == EDIT_ENABLED)
      {
        // Завершаем редактирование
        isEditing = false;
        saveClientsToFile();
        updateScrollText();
      }
      else
      {
        // Переходим к следующему параметру
        currentEditMode = static_cast<EditMode>((static_cast<int>(currentEditMode) + 1) % 3);
      }
    }
  }
  else
  {
    // Обычный режим просмотра
    if (upPressed)
    {
      // Переходим к предыдущему клиенту
      if (clients.size() > 0)
      {
        selectedClientIndex = (selectedClientIndex + clients.size() - 1) % clients.size();
        isEditing = true;
        currentEditMode = EDIT_TEMPERATURE;
      }
    }
    else if (downPressed)
    {
      // Переходим к следующему клиенту
      if (clients.size() > 0)
      {
        selectedClientIndex = (selectedClientIndex + 1) % clients.size();
        isEditing = true;
        currentEditMode = EDIT_TEMPERATURE;
      }
    }
    else if (selectPressed)
    {
      // Начинаем редактирование текущего клиента
      if (clients.size() > 0)
      {
        isEditing = true;
        currentEditMode = EDIT_TEMPERATURE;
      }
    }
  }

  // Обновляем дисплей
  updateLCD();
}
