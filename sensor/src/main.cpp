// Мигаем встроенным светодиодом на ESP32
const int LED = 2;

void setup() {
  pinMode(LED, OUTPUT);
}

void loop() {
  delay(1000);
  digitalWrite(LED, HIGH);
  delay(1000);
  digitalWrite(LED, LOW);
}