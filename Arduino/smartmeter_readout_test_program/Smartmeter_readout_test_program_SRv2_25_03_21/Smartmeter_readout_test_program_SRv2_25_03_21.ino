// smartmeter_readout_test_program.ino
// Smarty P1 Test program (ESP8266 = LOLIN/WEMOS D1 mini pro)
// weigu.lu
// 5V on Pin2 RJ12 (Enable)
// 10k resistor from Pin 5 RJ12 (TX) to 3.3V ESP
// Pin 5 RJ12 (TX) to RX ESP
// Pin 6 RJ12 (GND) to GND ESP
// Serial1 (Debug) on D4 ESP (TTL/USB adapter to PC)
// Disconnect RX on ESP or remove ESP to be able to program the ESP
// The debug stream must start with 0xDB (smartmeter in Luxembourg)

void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_FULL, 1, true); // true inverts signal
  Serial1.begin(115200); // Pin D4 Debugging info
  Serial.setRxBufferSize(2048);
  delay(100);
}

void loop() {
  while (Serial.available()) {
    Serial1.print(Serial.read(),HEX);
  }
  Serial1.println();  
  while (Serial.available()) { // clear buffer
    Serial.read();
  }
  delay(2000);  
}
