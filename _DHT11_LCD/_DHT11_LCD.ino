#include <DHT11.h>
#include <LiquidCrystal.h>
DHT11 dht(2); // выход DAT подключен к цыфровому входу 2
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);// RS,E,D4,D5,D6,D7
 
void setup() {   
  lcd.begin(16,2);// LCD 16X2
}
  
void loop() {
  int temperature = 0;
    int humidity = 0;

    // Attempt to read the temperature and humidity values from the DHT11 sensor.
    int result = dht.readTemperatureHumidity(temperature, humidity);

if (result == 0) {
        
       // выводим данные на LCD
 lcd.setCursor(0,0);
 lcd.print("Temp   ");lcd.print(temperature);lcd.print(char(223));lcd.print("C  ");
 lcd.setCursor(0,1);
lcd.print("Hum    ");lcd.print(humidity);lcd.print(" %         ");
    } else {
        // Print error message based on the error code.
        Serial.println(DHT11::getErrorString(result));
    }
 delay(1000);
}