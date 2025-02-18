#include <vector>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SPIFFS.h>           // Для работы с SPIFFS
#include <WiFi.h>         // Для Web API
#include <WebServer.h>    // Для HTTP-сервера
#include <LiquidCrystal.h> // Для LCD-дисплея
#include <ArduinoJson.h>

// Структура данных для клиентов
struct ClientData {
    BLEAddress address;            // MAC-адрес клиента
    String name;                   // Наименование клиента
    bool isEnabled;                // Включен или нет
    bool isConnected;              // Подключен или нет
    uint8_t gpioPins[4];           // Массив GPIO для управления
    float supportedTemp;           // Поддерживаемая температура
    float currentTemp;             // Текущая температура
    float currentHumidity;         // Текущая влажность
    unsigned long totalGpioOnTime; // Общее время включения GPIO
    uint8_t availableGpios[4];     // Доступные GPIO для выбора
};

// Глобальные переменные
std::vector<ClientData> clients;
BLEScan *pBLEScan;
WebServer server(80);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2); // Пины для LCD
const int SELECT_PIN = 13;
const int LEFT_PIN = 14;
const int UP_PIN = 15;
const int DOWN_PIN = 16;
const int RIGHT_PIN = 17;
const int RST_PIN = 18;

// UUID сервиса и характеристики
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Функция обратного вызова для сканирования
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
            Serial.printf("Найден клиент: %s\n", advertisedDevice.getAddress().toString().c_str());

            // Создаем новую запись о клиенте
            ClientData client={advertisedDevice.getAddress()};
            //client.address = advertisedDevice.getAddress();
            client.name = advertisedDevice.getName().c_str();
            client.isEnabled = false;
            client.isConnected = false;
            memset(client.gpioPins, 0, sizeof(client.gpioPins));
            client.supportedTemp = 20.0; // Значение по умолчанию
            client.currentTemp = 0.0;
            client.currentHumidity = 0.0;
            client.totalGpioOnTime = 0;
            
            //memcpy(client.availableGpios, (uint8_t[]){2, 4, 16, 17}, sizeof(client.availableGpios));

            clients.push_back(client);
        }
    }
};

static void temperatureNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    if (length >= 8) { // Данные должны содержать температуру и влажность
        float temp = *((float *)(pData + 0)); // Температура
        float humidity = *((float *)(pData + 4)); // Влажность

        Serial.printf("Получено уведомление: Температура %.2f, Влажность %.2f\n", temp, humidity);

        // Обновляем данные клиента
        for (auto &client : clients) {
            if (client.address == pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress()) {
                client.currentTemp = temp;
                client.currentHumidity = humidity;
                break;
            }
        }
    }
}

// Подписываемся на уведомления
void subscribeToNotifications(BLEClient *pClient) {
    BLERemoteService *pRemoteService = pClient->getService(SERVICE_UUID);
    if (!pRemoteService) {
        Serial.println("Сервис не найден");
        return;
    }

    BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (!pRemoteCharacteristic) {
        Serial.println("Характеристика не найдена");
        return;
    }

    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(temperatureNotifyCallback);
        Serial.println("Подписались на уведомления");
    } else {
        Serial.println("Уведомления недоступны");
    }
}

// Управление GPIO
void manageGpios() {
    std::vector<uint8_t> activeGpios;

    // Проверяем клиентов, у которых температура ниже установленной
    for (auto &client : clients) {
        if (client.isEnabled && client.isConnected && client.currentTemp < client.supportedTemp) {
            for (int i = 0; i < 4; ++i) {
                if (client.gpioPins[i] != 0) {
                    activeGpios.push_back(client.gpioPins[i]);
                }
            }
        }
    }

    // Уникализируем массив активных GPIO
    std::sort(activeGpios.begin(), activeGpios.end());
    activeGpios.erase(std::unique(activeGpios.begin(), activeGpios.end()), activeGpios.end());

    // Управляем GPIO
    for (int pin = 0; pin <= 39; ++pin) { // Перебираем все GPIO ESP32
        if (std::find(activeGpios.begin(), activeGpios.end(), pin) != activeGpios.end()) {
            digitalWrite(pin, HIGH); // Включаем
        } else {
            digitalWrite(pin, LOW); // Выключаем
        }
    }
}

// Сохранение клиентов в файл
void saveClientsToFile() {
    File file = SPIFFS.open("/clients.json", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    file.print("[");
    for (size_t i = 0; i < clients.size(); ++i) {
        file.print("{");
        file.print("\"address\":\"");
        file.print(clients[i].address.toString().c_str());
        file.print("\",\"name\":\"");
        file.print(clients[i].name);
        file.print("\",\"supportedTemp\":");
        file.print(clients[i].supportedTemp);
        file.print("}");
        if (i < clients.size() - 1) file.print(",");
    }
    file.println("]");
    file.close();
}

// Загрузка клиентов из файла
void loadClientsFromFile() {
    File file = SPIFFS.open("/clients.json", FILE_READ);
    if (!file) {
        Serial.println("File not found");
        return;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.println("Failed to parse JSON");
        return;
    }

    for (JsonObject obj : doc.as<JsonArray>()) {
        ClientData client ={BLEAddress(obj["address"].as<const char*>())};
        //client.address = BLEAddress(obj["address"].as<const char*>());
        client.name = obj["name"].as<String>();
        client.supportedTemp = obj["supportedTemp"];
        clients.push_back(client);
    }
    file.close();
}

// Обработка HTTP-запросов
void handleGetClients() {
    String response = "[";
    for (size_t i = 0; i < clients.size(); ++i) {
        response += "{";
        response += "\"name\":\"" + clients[i].name + "\",";
        response += "\"currentTemp\":" + String(clients[i].currentTemp) + ",";
        response += "\"supportedTemp\":" + String(clients[i].supportedTemp);
        response += "}";
        if (i < clients.size() - 1) response += ",";
    }
    response += "]";
    server.send(200, "application/json", response);
}

void handleSetClientName() {
    if (server.args() == 2 && server.hasArg("mac") && server.hasArg("name")) {
        String mac = server.arg("mac");
        String name = server.arg("name");

        for (auto &client : clients) {
            if (mac.c_str() == client.address.toString()) {
                client.name = name;
                saveClientsToFile(); // Сохраняем изменения
                server.send(200, "text/plain", "OK");
                return;
            }
        }
    }
    server.send(400, "text/plain", "Invalid request");
}

void setupWebServer() {
    server.on("/clients", handleGetClients);
    server.on("/set_name", handleSetClientName);
    server.begin();
    Serial.println("HTTP server started");
}

// Инициализация LCD
void setupLCD() {
    lcd.begin(16, 2);
    lcd.print("ESP32 Server");
}

// Отображение информации на LCD
void displayInfo() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Clients: ");
    lcd.print(clients.size());

    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    if (clients.size() > 0) {
        lcd.print(clients[0].currentTemp);
    } else {
        lcd.print("---");
    }
}

// Обработка кнопок
void handleButtons() {
    static int selectedClient = 0;

    if (digitalRead(SELECT_PIN) == LOW) {
        Serial.println("SELECT pressed");
        selectedClient = (selectedClient + 1) % clients.size();
        delay(200); // Антидрейф
    }

    if (digitalRead(UP_PIN) == LOW) {
        Serial.println("UP pressed");
        if (clients.size() > 0) {
            clients[selectedClient].supportedTemp += 0.5;
            saveClientsToFile();
        }
        delay(200);
    }

    if (digitalRead(DOWN_PIN) == LOW) {
        Serial.println("DOWN pressed");
        if (clients.size() > 0) {
            clients[selectedClient].supportedTemp -= 0.5;
            saveClientsToFile();
        }
        delay(200);
    }
}

// Настройка системы
void setup() {
    Serial.begin(115200);

    // Инициализация SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed");
        return;
    }

    // Загрузка клиентов из файла
    loadClientsFromFile();

    // Инициализация BLE
    BLEDevice::init("ESP32_Server");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349); // Интервал сканирования
    pBLEScan->setWindow(449);    // Окно сканирования

    // Инициализация WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");

    // Инициализация HTTP-сервера
    setupWebServer();

    // Инициализация LCD
    setupLCD();

    // Настройка кнопок
    pinMode(SELECT_PIN, INPUT_PULLUP);
    pinMode(LEFT_PIN, INPUT_PULLUP);
    pinMode(UP_PIN, INPUT_PULLUP);
    pinMode(DOWN_PIN, INPUT_PULLUP);
    pinMode(RIGHT_PIN, INPUT_PULLUP);
    pinMode(RST_PIN, INPUT_PULLUP);
}

// Основной цикл
void loop() {
    server.handleClient(); // Обработка HTTP-запросов
    pBLEScan->start(5);    // Сканирование клиентов каждые 5 секунд

    // Подключение к клиентам
    for (auto &client : clients) {
        if (!client.isConnected) {
            BLEAddress clientAddress = client.address;
            BLEClient *pClient = BLEDevice::createClient();
            if (pClient->connect(clientAddress)) {
                client.isConnected = true;
                subscribeToNotifications(pClient);
            } else {
                Serial.printf("Не удалось подключиться к клиенту %s\n", client.name.c_str());
            }
        }
    }

    manageGpios();         // Управление GPIO
    handleButtons();       // Обработка кнопок
    displayInfo();         // Обновление LCD
    delay(1000);           // Задержка для стабильности
}