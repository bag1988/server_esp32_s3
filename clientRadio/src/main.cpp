#include <Wire.h>               // Для работы с I2C
#include <AHT10.h>              // Для работы с AHT10
#include <LiquidCrystal_I2C.h>  // Для работы с сегментным LCD
#include <VirtualWire.h>        // Для работы с радиомодулем

// Конфигурация AHT10
AHT10 aht;

// Конфигурация LCD
#define LCD_ADDR 0x27           // Адрес I2C LCD (может быть 0x27 или 0x3F)
#define LCD_COLS 16             // Количество столбцов
#define LCD_ROWS 2              // Количество строк
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Конфигурация радиомодуля
#define RADIO_TX_PIN 2          // Пин для передатчика
#define RADIO_SPEED 2000        // Скорость передачи (2 кбит/с)

void setup() {
    // Инициализация Serial для отладки
    Serial.begin(115200);

    // Инициализация LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Initializing...");

    // Инициализация AHT10
    if (!aht.begin()) {
        Serial.println("Failed to initialize AHT10!");
        while (1) delay(1000); // Зацикливаемся при ошибке
    }

    // Инициализация радиомодуля
    vw_set_tx_pin(RADIO_TX_PIN); // Указываем пин для передатчика
    vw_setup(RADIO_SPEED);       // Настройка скорости передачи
    vw_set_ptt_inverted(true);   // Инверсия PTT для некоторых модулей
}

void loop() {
    static unsigned long lastReadTime = 0;
    const unsigned long interval = 60000; // Интервал чтения данных (1 минута)

    // Читаем данные с AHT10 каждую минуту
    if (millis() - lastReadTime >= interval) {
        lastReadTime = millis();

        // Чтение данных с AHT10
        float humidity = aht.getHumidity();
        float temperature = aht.getTemperature();

        // Проверка на ошибки чтения
        if (isnan(humidity) || isnan(temperature)) {
            Serial.println("Failed to read from AHT10!");
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
        lcd.print(humidity, 2); // Вывод с двумя знаками после запятой
        lcd.print(" %");

        // Подготовка данных для передачи
        char buffer[50];
        snprintf(buffer, sizeof(buffer), "T:%.2f,H:%.2f", temperature, humidity);

        // Передача данных по радиоканалу
        vw_send((uint8_t *)buffer, strlen(buffer)); // Отправляем строку
        vw_wait_tx();                              // Ждем завершения передачи

        // Вывод в Serial для отладки
        Serial.printf("Sent data: %s\n", buffer);

        // Переход в глубокий сон для экономии энергии
        delay(100); // Короткая задержка перед сном
        esp_deep_sleep_start(); // Глубокий сон до следующего измерения
    }
}

//приемник
// #include <VirtualWire.h>

// void setup() {
//     Serial.begin(115200);

//     // Инициализация радиомодуля
//     vw_set_rx_pin(4); // Указываем пин для приемника
//     vw_setup(2000);   // Настройка скорости передачи (2 кбит/с)
//     vw_rx_start();    // Начинаем прием данных
// }

// void loop() {
//     uint8_t buf[VW_MAX_MESSAGE_LEN];
//     uint8_t buflen = VW_MAX_MESSAGE_LEN;

//     // Проверяем наличие данных
//     if (vw_get_message(buf, &buflen)) {
//         // Преобразуем данные в строку
//         String message = "";
//         for (int i = 0; i < buflen; i++) {
//             message += (char)buf[i];
//         }

//         // Выводим данные в Serial
//         Serial.println(message);

//         // Можно разобрать строку для получения температуры и влажности
//         if (message.startsWith("T:")) {
//             float temperature = extractValue(message, 'T');
//             float humidity = extractValue(message, 'H');

//             Serial.printf("Temperature: %.2f C, Humidity: %.2f %%\n", temperature, humidity);
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