#include <Wire.h>               // Для работы с I2C
#include <SPI.h>                // Для работы с SPI (RFM69)
#include <RFM69.h>              // Для работы с радиомодулем RFM69
#include <Adafruit_AHTX0.h>      // Для работы с датчиком AHT10/AHT20
#include <LiquidCrystal_I2C.h>  // Для работы с сегментным LCD

// Конфигурация RFM69
#define NODE_ADDRESS 1          // Адрес отправителя
#define NETWORK_ID 100          // ID сети
#define FREQUENCY RF69_868MHZ   // Частота работы (RF69_433MHZ или RF69_868MHZ)
#define ENCRYPTION_KEY "sampleEncryptKey1" // Ключ шифрования

RFM69 radio;                    // Объект для работы с радиомодулем

// Конфигурация AHT10/AHT20
Adafruit_AHTX0 aht;

// Конфигурация LCD
#define LCD_ADDR 0x27           // Адрес I2C LCD (может быть 0x27 или 0x3F)
#define LCD_COLS 16             // Количество столбцов
#define LCD_ROWS 2              // Количество строк
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

void setup() {
    // Инициализация Serial для отладки
    Serial.begin(115200);

    // Инициализация LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Initializing...");

    // Инициализация AHT10/AHT20
    if (!aht.begin()) {
        Serial.println("Failed to initialize AHT10/AHT20!");
        while (1) delay(1000); // Зацикливаемся при ошибке
    }

    // Инициализация RFM69
    radio.initialize(FREQUENCY, NODE_ADDRESS, NETWORK_ID);
    radio.setHighPower(true);   // Включаем высокую мощность передатчика
    radio.encrypt(ENCRYPTION_KEY);
    Serial.println("RFM69 initialized!");
}

void loop() {
    static unsigned long lastReadTime = 0;
    const unsigned long interval = 60000; // Интервал чтения данных (1 минута)

    // Читаем данные с AHT10/AHT20 каждую минуту
    if (millis() - lastReadTime >= interval) {
        lastReadTime = millis();

        sensors_event_t humidity, temp;
  
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

        // Чтение данных с AHT10/AHT20
        float hum = humidity.relative_humidity;
        float temperature = temp.temperature;

        // Проверка на ошибки чтения
        if (isnan(hum) || isnan(temperature)) {
            Serial.println("Failed to read from AHT10/AHT20!");
            return;
        }

        // Отображение данных на LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(temperature, 2); // Вывод с двумя знаками после запятой
        lcd.print(" C");

        lcd.setCursor(0, 1);
        lcd.print("Hum: ");
        lcd.print(hum, 2); // Вывод с двумя знаками после запятой
        lcd.print(" %");

        // Подготовка данных для передачи
        char buffer[50];
        snprintf(buffer, sizeof(buffer), "T:%.2f,H:%.2f", temperature, hum);

        // Передача данных по радиоканалу
        radio.sendWithRetry(2, buffer, strlen(buffer)); // Отправляем на адрес 2
        radio.sleep(); // Переводим радиомодуль в режим сна

        // Вывод в Serial для отладки
        Serial.printf("Sent data: %s\n", buffer);

        // Переход в глубокий сон для экономии энергии
        delay(100); // Короткая задержка перед сном
        // Добавьте код для глубокого сна, если необходимо
    }
}

// приемник
// #include <SPI.h>                // Для работы с SPI
// #include <RFM69.h>              // Для работы с радиомодулем RFM69

// // Конфигурация RFM69
// #define NODE_ADDRESS 2          // Адрес приемника
// #define NETWORK_ID 100          // ID сети
// #define FREQUENCY RF69_868MHZ   // Частота работы (RF69_433MHZ или RF69_868MHZ)
// #define ENCRYPTION_KEY "sampleEncryptKey1" // Ключ шифрования

// RFM69 radio;                    // Объект для работы с радиомодулем

// void setup() {
//     Serial.begin(115200);

//     // Инициализация RFM69
//     radio.initialize(FREQUENCY, NODE_ADDRESS, NETWORK_ID);
//     radio.setHighPower(true);   // Включаем высокую мощность передатчика
//     radio.encrypt(ENCRYPTION_KEY);
//     radio.promiscuousMode(false); // Режим обычного приема
//     Serial.println("RFM69 receiver initialized!");
// }

// void loop() {
//     // Проверяем наличие входящих данных
//     if (radio.available()) {
//         uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
//         uint8_t len = sizeof(buf);

//         // Получаем данные
//         if (radio.recv(buf, &len)) {
//             String message = "";
//             for (int i = 0; i < len; i++) {
//                 message += (char)buf[i];
//             }

//             // Выводим данные в Serial
//             Serial.println(message);

//             // Разбор данных
//             if (message.startsWith("T:")) {
//                 float temperature = extractValue(message, 'T');
//                 float humidity = extractValue(message, 'H');

//                 Serial.printf("Temperature: %.2f C, Humidity: %.2f %%\n", temperature, humidity);
//             }
//         }
//     }
// }

// // Функция для извлечения значения из строки
// float extractValue(String message, char prefix) {
//     int start = message.indexOf(prefix) + 2; // Пропускаем символы "T:" или "H:"
//     int end = message.indexOf(',', start);
//     if (end == -1) end = message.length();
//     return message.substring(start, end).toFloat();
// }