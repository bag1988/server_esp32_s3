#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <LiquidCrystal_I2C.h> // Для работы с сегментным LCD
#include <AHT10.h>             // Для работы с AHT10

// Конфигурация BLE
#define BLE_SERVER_NAME "AHT10_ESP32C3"
#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMP_CHARACT_SERVICE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define HUM_CHARACT_SERVICE_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"

// Конфигурация LCD
#define LCD_ADDR 0x27 // Адрес I2C LCD (может быть 0x27 или 0x3F)
#define LCD_COLS 16   // Количество столбцов
#define LCD_ROWS 2    // Количество строк

// Объявление объектов
//LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
AHT10 aht;
uint8_t readStatus = 0;

// Переменные для хранения данных
float temperature = 0.0;
float humidity = 0.0;
bool deviceConnected = false;
// Функция для отправки данных по BLE
// class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
// {
//   void onWrite(BLECharacteristic *pCharacteristic)
//   {
//     std::string value = pCharacteristic->getValue();
//     if (value.length() > 0)
//     {
//       //Serial.println(("Received Data: %s", value.c_str()));
//     }
//   }
// };

// Temperature Characteristic and Descriptor
BLECharacteristic bmeTemperatureCelsiusCharacteristics(TEMP_CHARACT_SERVICE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeTemperatureCelsiusDescriptor(BLEUUID((uint16_t)0x2902)); // https://bitbucket.org/bluetooth-SIG/public/src/b012e68be90070f06ca43a9e24463cb86ca817c6/assigned_numbers/uuids/descriptors.yaml?at=main#descriptors.yaml-41

// Humidity Characteristic and Descriptor
BLECharacteristic bmeHumidityCharacteristics(HUM_CHARACT_SERVICE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeHumidityDescriptor(BLEUUID((uint16_t)0x2902)); // было 0x2903 //https://bitbucket.org/bluetooth-SIG/public/src/b012e68be90070f06ca43a9e24463cb86ca817c6/assigned_numbers/uuids/descriptors.yaml?at=main#descriptors.yaml-41

// Setup callbacks onConnect and onDisconnect
class CallbacksClientConnect : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.printf("Device connected, count device connected %d", pServer->getConnectedCount());
    deviceConnected = true;
  };
  void onDisconnect(BLEServer *pServer)
  {
    Serial.printf("Device disconnected, count device connected %d", pServer->getConnectedCount());
    deviceConnected = false;
  }
};

void setup()
{
  // Инициализация Serial для отладки
  Serial.begin(115200);

  // Инициализация LCD
  // lcd.init();
  // lcd.backlight();
  // lcd.setCursor(0, 0);
  // lcd.print("Initializing...");

  // Инициализация AHT10
  if (!aht.begin())
  {
    Serial.println("Failed to initialize AHT10!");
    while (1)
      delay(1000); // Зацикливаемся при ошибке
  }

  // Инициализация BLE
  BLEDevice::init(BLE_SERVER_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new CallbacksClientConnect());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics and Create a BLE Descriptor
  // Temperature
  pService->addCharacteristic(&bmeTemperatureCelsiusCharacteristics);
  bmeTemperatureCelsiusDescriptor.setValue("AHT temperature Celsius");
  bmeTemperatureCelsiusCharacteristics.addDescriptor(&bmeTemperatureCelsiusDescriptor);

  // Humidity
  pService->addCharacteristic(&bmeHumidityCharacteristics);
  bmeHumidityDescriptor.setValue("AHT humidity");
  bmeHumidityCharacteristics.addDescriptor(&bmeHumidityDescriptor);

  // Запуск службы
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // интервал рекламы
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE Device started!");
  //lcd.clear();
  //lcd.print("BLE Initialized");
}

void updateLcd()
{
  // Отображение данных на LCD
  // lcd.clear();
  // lcd.setCursor(0, 0);
  // lcd.print("Temp: ");
  // lcd.print(temperature);
  // lcd.print(" C");

  // lcd.setCursor(0, 1);
  // lcd.print("Hum: ");
  // lcd.print(humidity);
  // lcd.print(" %");
}

void loop()
{
  static unsigned long lastReadTime = 0;
  const unsigned long interval = 60000; // Интервал чтения данных (1 минута)

  // Читаем данные с AHT10 каждую минуту
  if (millis() - lastReadTime >= interval)
  {
    lastReadTime = millis();

    // readStatus = aht.readRawData(); // read 6 bytes from AHT10 over I2C

    // if (readStatus != AHT10_ERROR)
    // {
    //   Serial.print(F("Temperature: "));
    //   Serial.print(aht.readTemperature(AHT10_USE_READ_DATA));
    //   Serial.println(F(" +-0.3C"));
    //   Serial.print(F("Humidity...: "));
    //   Serial.print(aht.readHumidity(AHT10_USE_READ_DATA));
    //   Serial.println(F(" +-2%"));
    // }
    // else
    // {
    //   Serial.print(F("Failed to read - reset: "));
    //   Serial.println(aht.softReset()); // reset 1-success, 0-failed
    // }

    // Чтение данных с AHT10
    float h = aht.readHumidity();
    float t = aht.readTemperature();

    // Проверка на ошибки чтения
    if (isnan(h) || isnan(t))
    {
      Serial.println("Failed to read from AHT10!");
      return;
    }
    if (humidity != h || temperature != t)
    {
      if (humidity != h)
      {
        if (deviceConnected)
        {

          bmeHumidityCharacteristics.setValue(h);
          if (deviceConnected)
          {
            bmeHumidityCharacteristics.notify();
          }
        }
      }
      if (temperature != t)
      {
        if (deviceConnected)
        {

          bmeHumidityCharacteristics.setValue(h);
          if (deviceConnected)
          {
            bmeHumidityCharacteristics.notify();
          }
        }
      }

      humidity = h;
      temperature = t;

      //updateLcd();

      // Отправка данных по BLE
      char buffer[50];
      snprintf(buffer, sizeof(buffer), "Temp: %.2f C, Hum: %.2f %%", temperature, humidity);

      Serial.printf("Sent data: %s\n", buffer);
    }
  }
  delay(100); // Короткая задержка для стабильности
}