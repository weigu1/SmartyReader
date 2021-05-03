/*
  SmartyReader_Ethernet.ino

  V1.0 25/08/2019
    
  read p1 port from luxemburgish smartmeter,
  decode and publish with MQTT over Ethernet
  using ESP8266 (LOLIN/WEMOS D1 mini pro) or ESP32 (MH ET LIVE ESP32MiniKit)


  Copyright (C) 2017 Guy WEILER www.weigu.lu
  
  With modifications by Bob FISCH www.fisch.lu

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.

  Hardware serial (Serial) on LOLIN/WEMOS D1 mini pro uses UART0 of ESP8266,
  which is mapped to pins TX (GPIO1) and RX (GPIO3). It is used to access the
  smartmeter.
  You have to remove the LOLIN/WEMOS from its socket to program it! because the
  hardware serial port is also used during programming.

  Serial1 uses UART1 which is a transmit-only UART. UART1 TX pin is D4 (GPIO2,
  LED!!). If you use serial (UART0) to communicate with hardware, you can't use
  the Arduino Serial Monitor at the same time to debug your program! The best
  way to debug is to use Serial1.println() and connect RX of an USB2Serial
  adapter (FTDI, Profilic, CP210, ch340/341) to D4 and use a terminal program
  like CuteCom or CleverTerm to listen to D4.

  https://arduinodiy.wordpress.com/2017/04/12/the-w5100-bug/
*/

// Comment or uncomment the following lines suiting your needs
#define DEBUG     // if debugging requested
#define STATIC    // if static IP needed (no DHCP)
//#define ESP32MK   // if MH ET LIVE ESP32MiniKit instead of LOLIN D1 mini pro

#ifdef ESP32MK
  #include <WiFi.h> // ESP32 MH ET LIVE ESP32MiniKit
#else
  #include <ESP8266WiFi.h> // ESP8266 LOLIN/WEMOS D1 mini pro
#endif // ifdef ESP32MK


#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Crypto.h>
#include <AES.h>
#include <GCM.h>
#include <string.h>

// network settings
byte network_mac[] = { 0x00, 0x00, 0xBE, 0xEF, 0xF3, 0x08 };
#ifdef STATIC
  IPAddress network_ip      (192,168,1,114); //static IP
  IPAddress network_mask    (255,255,255,0);
  IPAddress network_gateway (192,168,1,20);
  IPAddress network_dns     (192,168,1,20); 
#endif // ifdef STATIC

// MQTT settings
const short mqttPort = 1883; // clear text = 1883
EthernetClient espClient;
PubSubClient client(espClient);
const char *mqtt_server = "192.168.1.84";
const char *mqtt_client_Id = "smarty_lam_22";
const char *mqtt_topic = "lamsmarty2";

// Serial settings
// Data from Smarty on Serial (RxD on LOLIN, (!interchanged on ESP32! weigu.lu!)
// to debug use Serial1 
// LOLIN/WEMOS D1 mini pro: transmit-only UART TxD on D4 (LED!)
// MHEtLive ESP32MiniKit Serial1 TxD on SD3 (GPIO10)
#define SR_Serial Serial
#define Debug_Serial Serial1
// needed to adjust the buffers (without gas about 700 bytes (take 1024))
const short max_serial_data_length = 1024; 
byte telegram[max_serial_data_length];
char buffer[max_serial_data_length-30];

// other variables
char msg[128], filename[13];
byte return_value;
short serial_data_length;
const byte line_length_raw_serial = 50; 
long previous_millis = 0;

#ifdef ESP32MK
  const byte DATA_REQUEST_SM = 17; //active Low! 17  for MH ET LIVE ESP32MiniKit
#else
  const byte DATA_REQUEST_SM = D3; //active Low! D3 for LOLIN/WEMOS D1 mini pro
#endif // ifdef ESP32MK

// Keys and smarty variables
struct TestVector {
  const char *name;
  uint8_t key[16];
  uint8_t ciphertext[max_serial_data_length];
  uint8_t authdata[17];
  uint8_t iv[12];
  uint8_t tag[16];
  uint8_t authsize;
  uint16_t datasize;
  uint8_t tagsize;
  uint8_t ivsize;
};

TestVector Vector_SM;
GCM<AES128> *gcmaes128 = 0;

//Key for SAG1030700089067
byte key_SM_LAM_1[] = {0xAE, 0xBD, 0x21, 0xB7, 0x69, 0xA6, 0xD1, 0x3C,
                       0x0D, 0xF0, 0x64, 0xE3, 0x83, 0x68, 0x2E, 0xFF};

char *identification;
char *p1_version;
char *timestamp;
char *equipment_id;
char *energy_delivered_tariff1;
char *energy_returned_tariff1;
char *reactive_energy_delivered_tariff1;
char *reactive_energy_returned_tariff1;
char *power_delivered;
char *power_returned;
char *reactive_power_delivered;
char *reactive_power_returned;
char *electricity_threshold;
char *electricity_switch_position;
char *electricity_failures;
char *electricity_sags_l1;
char *electricity_sags_l2;
char *electricity_sags_l3;
char *electricity_swells_l1;
char *electricity_swells_l2;
char *electricity_swells_l3;
char *message_short;
char *message2_long;
char *message3_long;
char *message4_long;
char *message5_long;
char *current_l1;
char *current_l2;
char *current_l3;

// define fields (constants)
const char *id_p1_version = "1-3:0.2.8(";
const char *id_timestamp = "0-0:1.0.0(";
const char *id_equipment_id = "0-0:42.0.0(";
const char *id_energy_delivered_tariff1 = "1-0:1.8.0(";
const char *id_energy_returned_tariff1 = "1-0:2.8.0(";
const char *id_reactive_energy_delivered_tariff1 = "1-0:3.8.0(";
const char *id_reactive_energy_returned_tariff1 = "1-0:4.8.0(";
const char *id_power_delivered = "1-0:1.7.0(";
const char *id_power_returned = "1-0:2.7.0(";
const char *id_reactive_power_delivered = "1-0:3.7.0(";
const char *id_reactive_power_returned = "1-0:4.7.0(";
const char *id_electricity_threshold = "0-0:17.0.0(";
const char *id_electricity_switch_position = "0-0:96.3.10(";
const char *id_electricity_failures = "0-0:96.7.21(";
const char *id_electricity_sags_l1 = "1-0:32.32.0(";
const char *id_electricity_sags_l2 = "1-0:52.32.0(";
const char *id_electricity_sags_l3 = "1-0:72.32.0(";
const char *id_electricity_swells_l1 = "1-0:32.36.0(";
const char *id_electricity_swells_l2 = "1-0:52.36.0(";
const char *id_electricity_swells_l3 = "1-0:72.36.0(";
const char *id_message_short = "0-0:96.13.0(";
const char *id_message2_long = "0-0:96.13.2(";
const char *id_message3_long = "0-0:96.13.3(";
const char *id_message4_long = "0-0:96.13.4(";
const char *id_message5_long = "0-0:96.13.5(";
const char *id_current_l1 = "1-0:31.7.0(";
const char *id_current_l2 = "1-0:51.7.0(";
const char *id_current_l3 = "1-0:71.7.0(";

/********** SETUP *******************************************************BEGIN*/
void setup() {
  // start LED
  pinMode(15, OUTPUT);      // Initialize outputs
  digitalWrite(15,HIGH);     // On
  pinMode(LED_BUILTIN, OUTPUT);      // Initialize outputs
  digitalWrite(LED_BUILTIN,LOW);     // On
  // start debug output on Serial1
  #ifdef DEBUG
    #ifdef ESP32MK
      Debug_Serial.begin(115200,SERIAL_8N1, 21, 22); //TxD1 on 22
    #else
      Debug_Serial.begin(115200); 
    #endif //ESP32MK  
    delay(500);
    Debug_Serial.println("Serial is working!"); 
  #endif // #ifdef DEBUG  
  delay(500);
  init_eth();  // start ethernet  
  client.setServer(mqtt_server,mqttPort);// open connection to MQTT server
  mqtt_connect();
  digitalWrite(LED_BUILTIN,HIGH);   // Off
  // open serial connection to smartmeter on SR_Serial
  pinMode(DATA_REQUEST_SM, OUTPUT);
  delay(100);
  #ifdef ESP32MK
    SR_Serial.begin(115200,SERIAL_8N1, 1, 3); // change reversed pins of ESP32
  #else
    SR_Serial.begin(115200); // Hardware serial connected to smarty
  #endif //ESP32MK  
  SR_Serial.setRxBufferSize(max_serial_data_length);
  delay(1000); // give it some time 
  #ifndef DEBUG
    blink_LED_x_times(8); // show  setup ready
  #endif // ifndef DEBUG  
  WiFi.mode( WIFI_OFF );
  
}
/********** LOOP  ********************************************************/

void loop() {
  if (millis() - previous_millis > 60000) {   // publish every 60 seconds
    previous_millis = millis();    
    get_smarty_and_publish();  
  }  
  // connect client, if needed
  mqtt_connect();
  // make the MQTT live
  client.loop();  
}
/********** INIT functions ***********************************************/

// init eth
void init_eth() {  
  Debug_Serial.println("Init ETH0");
  delay(250); // needed for propper Reset?
  #ifdef STATIC    
    Ethernet.begin(network_mac,network_ip); 
    #ifdef DEBUG
      Debug_Serial.println("\nEthernet connected");
      Debug_Serial.print("IP address: ");
      Debug_Serial.println(Ethernet.localIP());
    #endif //DEBUG  
  #else
    if(!Ethernet.begin(network_mac)) {
      Debug_Serial.println("Failed to configure Ethernet using DHCP");      
    }    
    else {
      #ifdef DEBUG
        Debug_Serial.println("\nEthernet connected");
        Debug_Serial.print("IP address: ");
        Debug_Serial.println(Ethernet.localIP());
      #endif //DEBUG  
    }
    #endif // STATIC  
  randomSeed(micros()); 
}

// connect to MQTT server
void mqtt_connect() {
  while (!client.connected()) { // Loop until we're reconnected
    #ifdef DEBUG
      Debug_Serial.print("Attempting MQTT connection...");
    #endif // ifdef DEBUG
    if (client.connect(mqtt_client_Id)) { // Attempt to connect
      #ifdef DEBUG
        Debug_Serial.print("MQTT connected");
      #endif // ifdef DEBUG
    }
    else {
      #ifdef DEBUG
        Debug_Serial.print("MQTT connection failed, rc=");
        Debug_Serial.print(client.state());
        Debug_Serial.println(" try again in 5 seconds");
      #endif // ifdef DEBUG
      delay(5000);  // Wait 5 seconds before retrying
    }
  }
}

// blink LED_BUILTIN x times (LED was ON)
void blink_LED_x_times(byte x) {
  for(byte i=0; i<x; i++) { // Blink x times
    digitalWrite(LED_BUILTIN,HIGH);   // Off
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);    // On
    delay(100);
  }
}
/********** SMARTY functions *********************************************/

void get_smarty_and_publish() {
  #ifdef DEBUG
    Debug_Serial.println("------------------");
    Debug_Serial.println("Get data from Smarty and publish");
    Debug_Serial.println("------------------");
  #endif // ifdef DEBUG
  memset(telegram, '1', sizeof(telegram));
  // P1 port is activated by setting “Data Request” line high (+5V).
  // While receiving data, “Data Request” line stays activated (+5V).
  digitalWrite(DATA_REQUEST_SM,LOW); 
  delay(10);
  digitalWrite(DATA_REQUEST_SM,HIGH);   // Request serial data  
  serial_data_length = readTelegram();
  
  #ifdef DEBUG
      print_raw_data(serial_data_length);
      Debug_Serial.println("------------------");
  #endif // ifdef DEBUG
      
  init_vector(&Vector_SM,"Vector_SM",key_SM_LAM_1);
  if (Vector_SM.datasize != 1024) {
      decrypt_text(&Vector_SM);
      parse_dsmr_string(buffer);
      /*cut_units(energy_delivered_tariff1);
      cut_units(energy_returned_tariff1);
      cut_units(power_delivered);
      cut_units(power_returned);*/
      
      snprintf (msg, 128, "{\"dt\":\"%s\",\"id\":\"%c%c%c\",\"c1\":\"%s\",\""
              "p1\":\"%s\"}",
              timestamp,equipment_id[13],equipment_id[14],equipment_id[15],
              energy_delivered_tariff1, energy_returned_tariff1);
      return_value=client.publish(mqtt_topic, msg);
      
      #ifdef DEBUG
      Debug_Serial.println("Print Vector:");
      print_vector(&Vector_SM);
      Debug_Serial.println("------------------");
      Debug_Serial.println("Print DSMR:");
      print_dsmr();
      Debug_Serial.println("------------------");
      Debug_Serial.println("Published message:");
      Debug_Serial.println(msg);
      Debug_Serial.println("------------------");
      #endif // ifdef DEBUG
  }
  #ifndef DEBUG
      blink_LED_x_times(4);
      delay(150);
      digitalWrite(LED_BUILTIN,HIGH);      // Off
  #endif // ifndef DEBUG
}

int readTelegram() {
  int cnt = 0;
  while ((SR_Serial.available()) && (cnt!= max_serial_data_length)) {
    telegram[cnt] = SR_Serial.read();
    cnt++;
  }  
  return (cnt);
}

// Convert binary coded decimal to decimal number
byte bcdToDec(byte val) {
  return((val/16*10) + (val%16));
}

// Convert decimal number to binary coded decimal
byte decToBcd(byte val) {
  return((val/10*16) + (val%10));
}

void init_vector(TestVector *vect, const char *Vect_name, uint8_t *key_SM) {
  vect->name = Vect_name;  // vector name
  for (int i = 0; i < 16; i++) {
    vect->key[i] = key_SM[i];
  }  
  uint16_t Data_Length = uint16_t(telegram[11])*256 + uint16_t(telegram[12])-17; // get length of data
  if (Data_Length>max_serial_data_length) {
    Data_Length = max_serial_data_length;
  }
  for (int i = 0; i < Data_Length; i++) {
    vect->ciphertext[i] = telegram[i+18];
  }   
  uint8_t AuthData[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                        0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
                        0xFF};
  for (int i = 0; i < 17; i++) {
     vect->authdata[i] = AuthData[i];
  }   
  for (int i = 0; i < 8; i++) {
     vect->iv[i] = telegram[2+i];
  }
  for (int i = 8; i < 12; i++) {
    vect->iv[i] = telegram[6+i]; 
  }
  for (int i = 0; i < 12; i++) {
    vect->tag[i] = telegram[Data_Length+18+i];
  }   
  vect->authsize = 17;
  vect->datasize = Data_Length;
  vect->tagsize = 12;
  vect->ivsize  = 12;
}

void decrypt_text(TestVector *vect) {
  gcmaes128 = new GCM<AES128>();
  size_t posn, len;
  size_t inc = vect->datasize;
  memset(buffer, 0xBA, sizeof(buffer));
  gcmaes128->setKey(vect->key, gcmaes128->keySize());
  gcmaes128->setIV(vect->iv, vect->ivsize);
  for (posn = 0; posn < vect->datasize; posn += inc) {
    len = vect->datasize - posn;
    if (len > inc) len = inc;
    gcmaes128->decrypt((uint8_t*)buffer + posn, vect->ciphertext + posn, len);
  }
  delete gcmaes128;
}

void parse_dsmr_string(char *mystring) {
  bool result;
  char *field;  
  identification = strtok(mystring, "\n");       // get the first field
  while ((field[0] != '!') && (field != NULL)) { // walk through other fields
    // passing NULL as first arguments makes the function continue where it left
    // it's work
    field = strtok(NULL, "\n");
    result=test_field(field,id_p1_version);
    if (result) {
      get_field(field);
      p1_version = field;
    }
    result=test_field(field,id_timestamp);
    if (result) {
      get_field(field);
      timestamp = field;
    }
    result=test_field(field,id_equipment_id);
    if (result) {
      get_field(field);
      convert_equipment_id(field);
      equipment_id = field;
    }
    result=test_field(field,id_energy_delivered_tariff1);
    if (result) {
      get_field(field);
      energy_delivered_tariff1 = field;
    }
    result=test_field(field,id_energy_returned_tariff1);
    if (result) {
      get_field(field);
      energy_returned_tariff1 = field;
    }
    result=test_field(field,id_reactive_energy_delivered_tariff1);
    if (result) {
      get_field(field);
      reactive_energy_delivered_tariff1 = field;
    }
    result=test_field(field,id_reactive_energy_returned_tariff1);
    if (result) {
      get_field(field);
      reactive_energy_returned_tariff1 = field;
    }
    result=test_field(field,id_power_delivered);
    if (result) {
      get_field(field);
      power_delivered = field;
    }
    result=test_field(field,id_power_returned);
    if (result) {
      get_field(field);
      power_returned = field;
    }
    result=test_field(field,id_reactive_power_delivered);
    if (result) {
      get_field(field);
      reactive_power_delivered = field;
    }
    result=test_field(field,id_reactive_power_returned);
    if (result) {
      get_field(field);
      reactive_power_returned = field;
    }
    result=test_field(field,id_electricity_threshold);
    if (result) {
      get_field(field);
      electricity_threshold = field;
    }
    result=test_field(field,id_electricity_switch_position);
    if (result) {
      get_field(field);
      electricity_switch_position = field;
    }
    result=test_field(field,id_electricity_failures);
    if (result) {
      get_field(field);
      electricity_failures = field;
    }
    result=test_field(field,id_electricity_sags_l1);
    if (result) {
      get_field(field);
      electricity_sags_l1 = field;
    }
    result=test_field(field,id_electricity_sags_l2);
    if (result) {
      get_field(field);
      electricity_sags_l2 = field;
    }
    result=test_field(field,id_electricity_sags_l3);
    if (result) {
      get_field(field);
      electricity_sags_l3 = field;
    }
    result=test_field(field,id_electricity_swells_l1);
    if (result) {
      get_field(field);
      electricity_swells_l1 = field;
    }
    result=test_field(field,id_electricity_swells_l2);
    if (result) {
      get_field(field);
      electricity_swells_l2 = field;
    }
    result=test_field(field,id_electricity_swells_l3);
    if (result) {
      get_field(field);
      electricity_swells_l3 = field;
    }
    result=test_field(field,id_message_short);
    if (result) {
      get_field(field);
      message_short = field;
    }
    result=test_field(field,id_message2_long);
    if (result) {
      get_field(field);
      message2_long = field;
    }
    result=test_field(field,id_message3_long);
    if (result) {
      get_field(field);
      message3_long = field;
    }
    result=test_field(field,id_message4_long);
    if (result) {
      get_field(field);
      message4_long = field;
    }
    result=test_field(field,id_message5_long);
    if (result) {
      get_field(field);
      message5_long = field;
    }
    result=test_field(field,id_current_l1);
    if (result) {
      get_field(field);
      current_l1 = field;
    }
    result=test_field(field,id_current_l2);
    if (result) {
      get_field(field);
      current_l2 = field;
    }
    result=test_field(field,id_current_l3);
    if (result) {
      get_field(field);
      current_l3 = field;
    }
  }
}

void convert_equipment_id(char *mystring) { //coded in HEX
  int len =strlen(mystring);
  for (int i=0; i<len/2; i++) {
    mystring[i]=char(((int(mystring[i*2]))-48)*16+(int(mystring[i*2+1])-48));
  }
  mystring[(len/2)]= 0; // (NULL is reserved for pointers)
}

// get the value between parentheses
void get_field(char *mystring) {
  int a = strcspn (mystring,"(");
  int b = strcspn (mystring,")");
  int j = 0;
  for (int i=a+1; i<b; i++) {
    mystring[j] = mystring[i];
    j++;
  }
  mystring[j]= 0;
}

// check if the first 10 symbols are equal
bool test_field(char *field2, const char *dmsr_field_id) {
  for (int i=0; i<10; i++) {
    // we can quit as soon as we found a difference
    if(field2[i] != dmsr_field_id[i]) 
      return false;
  }
  return true;
}

// cut units (Thanks Bob :))
void cut_units(char *mystring) {
  int a = 0;
  int b = strcspn (mystring,"*");
  int j = 0;
  int started = 1;
  for (int i=a+1; i<b; i++) {
    if(mystring[i]!='0' || mystring[i+1]=='.' || started==0) {
      mystring[j] = mystring[i];
      j++;
      started=0;
    }
  }
  mystring[j]= 0;
}
/********** DEBUG functions **********************************************/

void print_raw_data(short serial_data_length) {
  Debug_Serial.print("Raw length in Byte: ");
  Debug_Serial.println(serial_data_length);
  Debug_Serial.println("Raw data: ");
  int mul = (serial_data_length/line_length_raw_serial);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<line_length_raw_serial;j++) {
      Debug_Serial.print(telegram[i*line_length_raw_serial+j],HEX); 
    }
    Debug_Serial.println();
  }
  for(int j=0; j<(serial_data_length%line_length_raw_serial);j++) {
    Debug_Serial.print(telegram[mul*line_length_raw_serial+j],HEX);
  }
  Debug_Serial.println();
}

void print_vector(TestVector *vect) {
  Debug_Serial.print("\nVector_Name: ");
  Debug_Serial.println(vect->name);
  Debug_Serial.print("Key: ");
  for(int cnt=0; cnt<16;cnt++) {
    Debug_Serial.print(vect->key[cnt],HEX);
  }    
  Debug_Serial.print("\nData (Text): ");
  int mul = (vect->datasize/line_length_raw_serial);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<line_length_raw_serial;j++) {
      Debug_Serial.print(vect->ciphertext[i*line_length_raw_serial+j],HEX); 
    }
    Debug_Serial.println();
  }
  for(int j=0; j<(serial_data_length%line_length_raw_serial);j++) {
    Debug_Serial.print(vect->ciphertext[mul*line_length_raw_serial+j],HEX);
  }
  Debug_Serial.println();
  Debug_Serial.print("\nAuth_Data: ");
  for(int cnt=0; cnt<17;cnt++) {
    Debug_Serial.print(vect->authdata[cnt],HEX);
  }    
  Debug_Serial.print("\nInit_Vect: ");
  for(int cnt=0; cnt<12;cnt++) {
    Debug_Serial.print(vect->iv[cnt],HEX);
  }
  Debug_Serial.print("\nAuth_Tag: ");
  for(int cnt=0; cnt<12;cnt++) {
    Debug_Serial.print(vect->tag[cnt],HEX);
  }    
  Debug_Serial.print("\nAuth_Data Size: ");
  Debug_Serial.println(vect->authsize);
  Debug_Serial.print("Data Size: ");
  Debug_Serial.println(vect->datasize);
  Debug_Serial.print("Auth_Tag Size: ");
  Debug_Serial.println(vect->tagsize);
  Debug_Serial.print("Init_Vect Size: ");
  Debug_Serial.println(vect->ivsize);
  Debug_Serial.println(); 
}

void print_dsmr() {
  Debug_Serial.println(identification);
  Debug_Serial.println(p1_version);
  Debug_Serial.println(timestamp);
  Debug_Serial.println(equipment_id);
  Debug_Serial.println(energy_delivered_tariff1);
  Debug_Serial.println(energy_returned_tariff1);
  Debug_Serial.println(reactive_energy_delivered_tariff1);
  Debug_Serial.println(reactive_energy_returned_tariff1);
  Debug_Serial.println(power_delivered);
  Debug_Serial.println(power_returned);
  Debug_Serial.println(reactive_power_delivered);
  Debug_Serial.println(reactive_power_returned);
  Debug_Serial.println(electricity_threshold);
  Debug_Serial.println(electricity_switch_position);
  Debug_Serial.println(electricity_failures);
  Debug_Serial.println(electricity_sags_l1);
  Debug_Serial.println(electricity_sags_l2);
  Debug_Serial.println(electricity_sags_l3);
  Debug_Serial.println(electricity_swells_l1);
  Debug_Serial.println(electricity_swells_l2);
  Debug_Serial.println(electricity_swells_l3);
  Debug_Serial.println(message_short);
  Debug_Serial.println(message2_long);
  Debug_Serial.println(message3_long);
  Debug_Serial.println(message4_long);
  Debug_Serial.println(message5_long);
  Debug_Serial.println(current_l1);
  Debug_Serial.println(current_l2);
  Debug_Serial.println(current_l3);
}
