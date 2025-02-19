#include <lcd_setting.h>
#include <LiquidCrystal_I2C.h>
#include <spiffs_setting.h>
#include <variables_info.h>
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

// Scrolling Text
void updateScrollText()
{
  if (clients.empty())
  {
    scrollText = "No clients found";
  }
  else
  {
    const ClientData &currentClient = clients[selectedClientIndex];
    // scrollText = currentClient.name + " Temp:" + String(currentClient.currentTemperature) + "C Tgt:" + String(currentClient.targetTemperature) + "C";
  }
  scrollPosition = 0;
}

// LCD Output
void updateLCD()
{
  lcd.clear();

  if (clients.empty())
  {
    lcd.setCursor(0, 0);
    lcd.print("No clients found");
    return;
  }

  const ClientData &currentClient = clients[selectedClientIndex];

  // Отображаем имя клиента в первой строке
  lcd.setCursor(0, 0);
  lcd.print(currentClient.name.c_str());
  if (isEditing)
  {
    lcd.print("*"); // Индикация общего режима редактирования
  }

  lcd.setCursor(0, 1);
  if (isEditing)
  {
    // Режим редактирования: Отображаем GPIO или температуру
    if (currentEditMode == EDIT_GPIO)
    {
      lcd.print("GPIO: ");
      lcd.print(availableGpio[currentGpioIndex]);
      lcd.setCursor(8, 1); // Второй столбец второй строки
      // Проверяем добавлен ли GPIO в список для текущего клиента
      bool found = false;
      for (int gpio : currentClient.gpioPins)
      {
        if (gpio == availableGpio[currentGpioIndex])
        {
          found = true;
          break;
        }
      }
      if (found)
      {
        lcd.print("IN");
      }
      else
      {
        lcd.print("OUT");
      }
    }
    else
    {
      // Режим EDIT_TEMPERATURE
      lcd.print("Temp: ");
      lcd.print(currentClient.currentTemperature);
      lcd.print("C");

      lcd.setCursor(8, 1);
      lcd.print("Tgt: ");
      lcd.print(currentClient.targetTemperature);
      lcd.print("C");
    }
  }
  else
  {
    // Режим бегущей строки
    String displayText = scrollText.substring(scrollPosition);
    if (displayText.length() < 16)
    {
      displayText += scrollText.substring(0, 16 - displayText.length());
    }
    displayText = displayText.substring(0, 16);
    lcd.print(displayText);
  }
  if (wifiConnected)
  {
    lcd.setCursor(15, 0); // Rightmost position of the first line
    lcd.print("C");       // "C" for connected
  }
  else
  {
    lcd.setCursor(15, 0);
    lcd.print("D"); // "D" for disconnected
  }
}

// Button Handling
void handleButtons()
{
  // SELECT: Переключение между режимами (Температура <-> GPIO) или включение/выключение редактирования
  if (digitalRead(BUTTON_SELECT) == LOW)
  {
    if (!isEditing)
    {
      // Переключение между режимами редактирования (Температура/GPIO)
      currentEditMode = (currentEditMode == EDIT_TEMPERATURE) ? EDIT_GPIO : EDIT_TEMPERATURE;
    }
    else
    {
      // Выход из режима редактирования
      isEditing = false;
      saveClientsToFile();
    }
    updateLCD();
    delay(200);
  }

  // Двойное условие
  if (digitalRead(BUTTON_SELECT) == LOW && !isEditing)
  {
    isEditing = true;
    currentGpioIndex = 0; // сбрасываем текущий gpio для выбора
    updateLCD();
    delay(200);
  }

  if (isEditing)
  {
    if (currentEditMode == EDIT_TEMPERATURE)
    {
      // Редактирование температуры
      if (digitalRead(BUTTON_UP) == LOW)
      {
        clients[selectedClientIndex].targetTemperature += TEMPERATURE_STEP;
        updateLCD();
        delay(200);
      }
      if (digitalRead(BUTTON_DOWN) == LOW)
      {
        clients[selectedClientIndex].targetTemperature -= TEMPERATURE_STEP;
        updateLCD();
        delay(200);
      }
    }
    else if (currentEditMode == EDIT_GPIO)
    {
      // Редактирование GPIO

      // Сдвиг индекса влево
      if (digitalRead(BUTTON_LEFT) == LOW)
      {
        if (currentGpioIndex > 0)
        {
          currentGpioIndex--;
          updateLCD();
          delay(200);
        }
      }

      // Сдвиг индекса вправо
      if (digitalRead(BUTTON_RIGHT) == LOW)
      {
        if (currentGpioIndex < availableGpio.size() - 1)
        {
          currentGpioIndex++;
          updateLCD();
          delay(200);
        }
      }

      // UP: Добавить GPIO в список для клиента (если его там нет)
      if (digitalRead(BUTTON_UP) == LOW)
      {
        int selectedGpio = availableGpio[currentGpioIndex];
        bool found = false;
        for (int gpio : clients[selectedClientIndex].gpioPins)
        {
          if (gpio == selectedGpio)
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
          clients[selectedClientIndex].gpioPins.push_back(selectedGpio);
          sort(clients[selectedClientIndex].gpioPins.begin(), clients[selectedClientIndex].gpioPins.end()); // Сортируем
          updateLCD();
          delay(200);
        }
      }

      // DOWN: Удалить GPIO из списка для клиента (если он там есть)
      if (digitalRead(BUTTON_DOWN) == LOW)
      {
        int selectedGpio = availableGpio[currentGpioIndex];
        for (size_t i = 0; i < clients[selectedClientIndex].gpioPins.size(); ++i)
        {
          if (clients[selectedClientIndex].gpioPins[i] == selectedGpio)
          {
            clients[selectedClientIndex].gpioPins.erase(clients[selectedClientIndex].gpioPins.begin() + i);
            updateLCD();
            delay(200);
            break;
          }
        }
      }
    }
  }
  else
  {
    // Переключение клиентов
    if (digitalRead(BUTTON_SELECT) == LOW)
    {
      selectedClientIndex = (selectedClientIndex + 1) % clients.size();
      updateScrollText(); // Обновляем текст бегущей строки при смене клиента
      updateLCD();
      delay(200);
    }
  }

  // Rst: reset esp32
  if (digitalRead(BUTTON_RST) == LOW)
  {
    ESP.restart();
  }
}


