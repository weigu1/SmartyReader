/*
  SmartyReader.ino

  Read P1 port from Luxemburgian smartmeter.
  Decode and publish with MQTT over Wifi or Ethernet
  using ESP8266 (LOLIN/WEMOS D1 mini pro) or ESP32 (MH ET LIVE ESP32MiniKit)

  V1.0 04/05/2019
  V1.1 05/05/2019 OTA added
  V1.2 09/12/2019 now using my new helper lib: ESPBacker
  V1.3 23/06/2020 major changes in code, working with with Strings, publish 
                  all data (json or string) 
  ---------------------------------------------------------------------------
  Copyright (C) 2017 Guy WEILER www.weigu.lu
  
  With ideas from Sam Grimee, Bob Fisch (www.fisch.lu), and Serge Wagner  
  Alternative Software: https://github.com/sgrimee 

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
  ---------------------------------------------------------------------------

  Install the ESPBacker lib by downloading the ESPBacker.zip abd add it by using:
  Sketch -> Include Library -> Add .ZIP librarary ... in Arduino IDE

  More information on AES128-GCM can be found on:
  http://weigu.lu/tutorials/sensors2bus/04_encryption/index.html.
  More information on MQTT can be found on:
  http://weigu.lu/tutorials/sensors2bus/06_mqtt/index.html.
  
  Data from Smarty are on Serial (Serial0) RxD on ESP.
  If you want to debug over Serial, use Serial1 for LOLIN/WEMOS D1 mini pro:
  Transmit-only UART TxD on D4 (LED!)
  For MHEtLive ESP32MiniKit use Serial1 TxD or Serial2 TxD

  If using OTA (not MQTTSECURE!):
  Port defaults to 8266,  Hostname defaults to esp3232-[MAC], 
  We set the password here with it's md5 value
  To get md5 on Ubuntu use "md5sum": md5sum, enter, type pw, 2x Ctrl D
  
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
  
  Another possibility to debug is to use UDP.
  You can listen on Linux PC (UDP_LOG_PC_IP) with netcat command: nc -kulw 0 6666

  You can also use an MH ET LIVE ESP32MiniKit board. Add a capacitor
  1000µF,10V (better 4700µF,10V) to the 5V header (look on weigu.lu)
  
  ESP8266: LOLIN/WEMOS D1 mini pro
  ESP32:   MH ET LIVE ESP32-MINI-KIT 
  
  MHET    | MHET    - LOLIN        |---| LOLIN      - MHET    | MHET
  
  GND     | RST     - RST          |---| TxD        - RxD(3)  | GND
   NC     | SVP(36) -  A0          |---| RxD        - TxD(1)  | 27 
  SVN(39) | 26      -  D0(16)      |---|  D1(5,SCL) -  22     | 25
   35     | 18      -  D5(14,SCK)  |---|  D2(4,SDA) -  21     | 32
   33     | 19      -  D6(12,MISO) |---|  D3(0)     -  17     | TDI(12)
   34     | 23      -  D7(13,MOSI) |---|  D4(2,LED) -  16     | 4
  TMS(14) | 5       -  D8(15,SS)   |---| GND        - GND     | 0   
   NC     | 3V3     - 3V3          |---|  5V        -  5V     | 2
  SD2(9)  | TCK(13)                |---|              TD0(15) | SD1(8)
  CMD(11) | SD3(10)                |---|              SD0(7)  | CLK(6)

*/

// Publishes every in milliseconds
const long PUBLISH_TIME = 10000;

// Comment or uncomment the following lines suiting your needs
//#define MQTTSECURE    // if you want a secure connection over MQTT (recommended!!)
//#define STATIC        // if static IP needed (no DHCP)
//#define ETHERNET      // if Ethernet with Funduino (W5100) instead of WiFi
#define OTA           // if Over The Air update needed (security risk!)

#include "ESPBacker.h"  // ESP helper lib (more on weigu.lu)
#include <PubSubClient.h>
#include <Crypto.h>
#include <AES.h>
#include <GCM.h>
//#include <string.h>

// WiFi and network settings
const char *WIFI_SSID = "";                 // SSID
const char *WIFI_PASSWORD = "";   // password
const char *NET_MDNSNAME = "SmartyReader2";      // optional (access with myESP.local)
const char *NET_HOSTNAME = "SmartyReader2";      // optional

//Key for SAG1030700089067
byte KEY_SMARTY[] = {0xAE, 0xBD, 0x21, 0xB7, 0x69, 0xA6, 0xD1, 0x3C,
                     0x0D, 0xF0, 0x64, 0xE3, 0x83, 0x68, 0x2E, 0xFF};
/*byte KEY_SMARTY[] = {0x19, 0xE5, 0xFE, 0x03, 0xC4, 0xB0, 0x65, 0xB4, // Consumtion
                     0x25, 0x77, 0xCC, 0x9B, 0xB7, 0x4C, 0x0F, 0x9B};// 430*/
                     

#ifdef STATIC
  IPAddress NET_LOCAL_IP (192,168,1,181);    // 3x optional for static IP
  IPAddress NET_GATEWAY (192,168,1,20);
  IPAddress NET_MASK (255,255,255,0);  
#endif // ifdef STATIC*/
#ifdef ETHERNET
  byte NET_MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //only for ethernet
#endif //#ifdef ETHERNET*/

// UDP settings
const word UDP_LOG_PORT = 6666;
IPAddress UDP_LOG_PC_IP(192,168,1,50);

// Over The Air update settings
#ifdef OTA
  const char *OTA_NAME = "SmartyReader2";
  const char *OTA_PASS_HASH = "c3be31f8c0878e2a4f007200220ce2ba"; //LetMeOTA!
#endif // #ifdef OTA  

// MQTT settings
const char *MQTT_SERVER = "192.168.1.60";
const char *MQTT_CLIENT_ID = "smarty_lam1_p12";
String MQTT_TOPIC = "lamsmarty";
#ifdef MQTTSECURE // http://weigu.lu/tutorials/sensors2bus/06_mqtt/index.html
  const short MQTT_PORT = 8883;  // port for secure communication
  const char *MQTT_USER = "me";
  const char *MQTT_PASS = "myMqttPass12!";
  const char *MQTT_PSK_IDENTITY = "btsiot1";
  const char *MQTT_PSK_KEY = "0123456789abcdef0123"; // hex string without 0x
  WiFiClientMQTTSECURE espClient;
#else
  const short MQTT_PORT = 1883; // clear text = 1883
  WiFiClient espClient;
#endif

ESPBacker B;

PubSubClient mqttClient(espClient);

// needed to adjust the buffers (without gas about 700 bytes (take 1024))
const unsigned short MAX_SERIAL_STREAM_LENGTH = 1280; 
byte telegram[MAX_SERIAL_STREAM_LENGTH];
char buffer[MAX_SERIAL_STREAM_LENGTH];

// other variables
char msg[128], filename[13];
byte return_value;
unsigned short serial_data_length;

#ifdef ESP8266
  const byte DATA_REQUEST_SM = D3; //active Low! D3 for LOLIN/WEMOS D1 mini pro
#else
  const byte DATA_REQUEST_SM = 17; //active Low! 17  for MH ET LIVE ESP32MiniKit
#endif // #ifdef ESP8266

// Crypto and Smarty variables
struct Vector_GCM {
  const char *name;
  byte key[16];
  byte ciphertext[MAX_SERIAL_STREAM_LENGTH];
  byte authdata[17];
  byte iv[12];
  byte tag[16];
  size_t authsize;
  size_t datasize;
  size_t tagsize;
  size_t ivsize;
};

struct dsmr_field_t {
  const String name;
  const String id;
  const String unit;  
  String value;
  char type;  // string s, float f, integer i
};

// https://electris.lu/files/Dokumente_und_Formulare/P1PortSpecification.pdf
struct dsmr_field_t dsmr[] = {
  {"identification", "", "","",'s'},
  {"p1_version", "1-3:0.2.8", "","",'s'},
  {"timestamp", "0-0:1.0.0", "","",'s'},
  {"equipment_id", "0-0:42.0.0", "","",'s'},
  {"act_energy_imported_p_plus", "1-0:1.8.0", "kWh","",'f'},
  {"act_energy_exported_p_minus", "1-0:2.8.0", "kWh","",'f'},
  {"react_energy_imported_q_plus", "1-0:3.8.0", "kvarh","",'f'},
  {"react_energy_exported_q_minus", "1-0:4.8.0", "kvarh","",'f'},
  {"act_pwr_imported_p_plus", "1-0:1.7.0", "kW","",'f'},
  {"act_pwr_exported_p_minus", "1-0:2.7.0", "kW","",'f'},
  {"react_pwr_imported_q_plus", "1-0:3.7.0", "kvar","",'f'},
  {"react_pwr_exported_q_minus", "1-0:4.7.0", "kvar","",'f'},
  {"active_threshold_smax", "0-0:17.0.0", "kVA","",'f'},
  {"breaker_ctrl_state_0", "0-0:96.3.10", "","",'i'},
  {"num_pwr_failures", "0-0:96.7.21", "","",'i'},
  {"num_volt_sags_l1", "1-0:32.32.0", "","",'i'},
  {"num_volt_sags_l2", "1-0:52.32.0", "","",'i'},
  {"num_volt_sags_l3", "1-0:72.32.0", "","",'i'},
  {"num_volt_swells_l1", "1-0:32.36.0", "","",'i'},
  {"num_volt_swells_l2", "1-0:52.36.0", "","",'i'},
  {"num_volt_swells_l3", "1-0:72.36.0", "","",'i'},
  {"breaker_ctrl_state_0", "0-0:96.3.10", "","",'i'},
  {"msg_long_e_meter", "0-0:96.13.0", "","",'s'},
  {"msg_long_ch2", "0-0:96.13.2", "","",'s'},
  {"msg_long_ch3", "0-0:96.13.3", "","",'s'},
  {"msg_long_ch4", "0-0:96.13.4", "","",'s'},
  {"msg_long_ch5", "0-0:96.13.5", "","",'s'},
  {"curr_l1", "1-0:31.7.0", "A","",'f'},
  {"curr_l2", "1-0:51.7.0", "A","",'f'},
  {"curr_l3", "1-0:71.7.0", "A","",'f'},
  {"act_pwr_imp_p_plus_l1", "1-0:21.7.0", "kW","",'f'},
  {"act_pwr_imp_p_plus_l2", "1-0:41.7.0", "kW","",'f'},
  {"act_pwr_imp_p_plus_l3", "1-0:61.7.0", "kW","",'f'},
  {"act_pwr_exp_p_minus_l1", "1-0:22.7.0", "kW","",'f'},
  {"act_pwr_exp_p_minus_l2", "1-0:42.7.0", "kW","",'f'},
  {"act_pwr_exp_p_minus_l3", "1-0:62.7.0", "kW","",'f'},
  {"react_pwr_imp_q_plus_l1", "1-0:23.7.0", "kvar","",'f'},
  {"react_pwr_imp_q_plus_l2", "1-0:43.7.0", "kvar","",'f'},
  {"react_pwr_imp_q_plus_l3", "1-0:63.7.0", "kvar","",'f'},
  {"react_pwr_exp_q_minus_l1", "1-0:24.7.0", "kvar","",'f'},
  {"react_pwr_exp_q_minus_l2", "1-0:44.7.0", "kvar","",'f'},
  {"react_pwr_exp_q_minus_l3", "1-0:64.7.0", "kvar","",'f'},  
  {"apparent_export_pwr", "1-0:10.7.0", "kVA","",'f'},
  {"apparent_import_pwr", "1-0:9.7.0", "kVA","",'f'},  
  {"breaker_ctrl_state_1", "0-1:96.3.10", "","",'i'},
  {"breaker_ctrl_state_2", "0-2:96.3.10", "","",'i'},
  {"gas_index", "0-1:24.2.1", "m3","",'f'},
  {"limiter_curr_monitor", "1-1:31.4.0", "A","",'f'},  
  {"volt_l1", "1-0:32.7.0", "V","",'f'},
  {"volt_l2", "1-0:52.7.0", "V","",'f'},
  {"volt_l3", "1-0:72.7.0", "V","",'f'},
};

Vector_GCM Vector_SM;
GCM<AES128> *gcmaes128 = 0;

/********** SETUP *************************************************************/

void setup() {
  B.set_led_log(true);                 // use builtin LED for debugging
  B.set_serial_log(true,1);           // 2 parameter = interface (1 = Serial1)
  B.set_udp_log(true, UDP_LOG_PC_IP, UDP_LOG_PORT); // use "nc -kulw 0 6666"
  init_wifi();  
  init_serial4smarty();
  mqttClient.setServer(MQTT_SERVER,MQTT_PORT); //open connection to MQTT server
  mqtt_connect();
  #ifdef OTA
    B.init_ota(OTA_NAME, OTA_PASS_HASH);
  #endif // ifdef OTA
  delay(2000); // give it some time 
  B.blink_led_x_times(3);  
}

/********** LOOP  **************************************************************/

void loop() {
  #ifdef OTA
    ArduinoOTA.handle();
  #endif // ifdef OTA
  // Publish every PUBLISH_TIME
  if (B.non_blocking_delay(PUBLISH_TIME)) {
    get_smarty_and_publish();  
  }    
  if (WiFi.status() != WL_CONNECTED) {  // if WiFi disconnected, reconnect
    init_wifi();
  }    
  if (!mqttClient.connected()) { // reconnect mqtt client, if needed  
    mqtt_connect();
  }
  mqttClient.loop();                    // make the MQTT live
  yield();  
}

/********** INIT functions ****************************************************/

void init_wifi() {
  #ifdef STATIC
    B.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_HOSTNAME, NET_LOCAL_IP,
                  NET_GATEWAY, NET_MASK);
  #else                  
    B.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_MDNSNAME, NET_HOSTNAME);                       
  #endif // ifdef STATIC
}

// connect to MQTT server
void mqtt_connect() {
  while (!mqttClient.connected()) { // Loop until we're reconnected
    B.log("Attempting MQTT connection...");    
    #ifdef MQTTSECURE  
      if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    #else
      if (mqttClient.connect(MQTT_CLIENT_ID)) { // Attempt to connect
    #endif // ifdef UNMQTTSECURE
      B.log_ln("MQTT connected");      
      // Once connected, publish an announcement...    
      //mqttClient.publish(MQTT_TOPIC, "{\"dt\":\"connected\"}");
      // don't because OpenHAB does not like this ...      
    }
    else {
      B.log("MQTT connection failed, rc=");
      B.log(String(mqttClient.state()));
      B.log_ln(" try again in 5 seconds");
      delay(5000);  // Wait 5 seconds before retrying
    }
  }
}

// open serial connection to smartmeter on Serial0
void init_serial4smarty() {  
  pinMode(DATA_REQUEST_SM, OUTPUT);
  delay(100);
  #ifdef ESP8266
    Serial.begin(115200); // Hardware serial connected to smarty    
  #else
    Serial.begin(115200,SERIAL_8N1, 1, 3); // change reversed pins of ESP32
  #endif //ESP32MK  
  Serial.setRxBufferSize(MAX_SERIAL_STREAM_LENGTH);
}  
/********** SMARTY functions **************************************************/

void get_smarty_and_publish() {
  B.log_ln("-----------------------------------");
  B.log("Get data from Smarty and publish");  
  serial_data_length = readTelegram();  
  //print_raw_data(serial_data_length);  // for thorough debugging
  init_vector(&Vector_SM,"Vector_SM",KEY_SMARTY);
  //print_vector(&Vector_SM);            // for thorough debugging
  decrypt_text(&Vector_SM);
  parse_dsmr_string(buffer);
  mqtt_publish_all(1); // 0 for simple string, 1 for json cooked  
  B.blink_led_x_times(4);      
}

int readTelegram() {
  unsigned short serial_cnt = 0;
  memset(telegram, 0, MAX_SERIAL_STREAM_LENGTH);  
  digitalWrite(DATA_REQUEST_SM,LOW);   // set request line to 5V    
  delay(500);                          // wait for the data (generated after ~100ms)
  digitalWrite(DATA_REQUEST_SM,HIGH);  // x*86,8µs (1024 byte need 88ms)
  while ((Serial.available()) && (serial_cnt < MAX_SERIAL_STREAM_LENGTH)) {  
    telegram[serial_cnt] = Serial.read();
    if (telegram[0] != 0xDB) {
      while (Serial.available() > 0) {Serial.read();} //clear the buffer!    
      break;
    }
    serial_cnt++;    
  }    
  return (serial_cnt);
}

void init_vector(Vector_GCM *vect, const char *Vect_name, byte *key_SM) {
  vect->name = Vect_name;  // vector name
  for (int i = 0; i < 16; i++) {
    vect->key[i] = key_SM[i];
  }  
  uint16_t Data_Length = uint16_t(telegram[11])*256 + uint16_t(telegram[12])-17; // get length of data
  if (Data_Length>MAX_SERIAL_STREAM_LENGTH) {
    Data_Length = MAX_SERIAL_STREAM_LENGTH-30;
  }
  for (int i = 0; i < Data_Length; i++) {
    vect->ciphertext[i] = telegram[i+18];
  }   
  byte AuthData[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
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

void decrypt_text(Vector_GCM *vect) {
  gcmaes128 = new GCM<AES128>();
  gcmaes128->setKey(vect->key, gcmaes128->keySize());
  gcmaes128->setIV(vect->iv, vect->ivsize);
  gcmaes128->decrypt((byte*)buffer, vect->ciphertext, vect->datasize);   
  delete gcmaes128;
}

void parse_dsmr_string(String Dmsr) {
  int field_index, byte_cnt, i, j;
  String Tmp;
  B.log_ln("+++++++++++++++++++++++++++++++++++++++");
  B.log_ln("DMSR:");
  B.log_ln(Dmsr);  
  // get the identification
  byte_cnt = Dmsr.indexOf('\n');
  dsmr[0].value = Dmsr.substring(0,byte_cnt-1); 
  B.log_ln("+++++++++++++++++++++++++++++++++++++++");
  B.log_ln(dsmr[0].value);  
  // run through all the OBIS fields  
  for (i = 1; i < (sizeof dsmr / sizeof dsmr[0]); i++) { 
    field_index = Dmsr.indexOf(dsmr[i].id);  // get field_indexes of OBIS codes
    if (field_index != -1) { // -1 = not found   
      byte_cnt = field_index;
      while (Dmsr[byte_cnt] != '(') { // go to '('
        byte_cnt++;
      }
      if ((dsmr[i].id)=="0-1:24.2.1") { // Merci Serge
        byte_cnt++; // get past '('             
        while (Dmsr[byte_cnt] != '(') { // go to '('
          byte_cnt++;
        }
      }
      Tmp = "";
      byte_cnt++; // get past '('             
      while (Dmsr[byte_cnt] != ')') { // get the String between brackets         
        Tmp.concat(Dmsr[byte_cnt]);
        byte_cnt++;              
      }
      if ((dsmr[i].id)=="0-0:42.0.0") { // convert equipment id (coded in HEX)                
        for (j = 0; j < Tmp.length()/2; j++) {
          char digit = char(((int(Tmp[j*2]))-48)*16+(int(Tmp[j*2+1])-48));
          Tmp.setCharAt(j, digit);          
        }
        Tmp.remove(j);  
      }
      if ((dsmr[i].id)=="0-0:1.0.0") { // convert timestamp to ISO 8601      
        Tmp = "20" + Tmp.substring(0,2) + "-" + Tmp.substring(2,4) +
              "-" + Tmp.substring(4,6) + "T" + Tmp.substring(6,8) +
              ":" + Tmp.substring(8,10) + ":" + Tmp.substring(10,12);
      }              
      Tmp.remove(Tmp.indexOf('*')); // remove the units      
      dsmr[i].value = Tmp;          // and save it to the value field
      B.log_ln(dsmr[i].value);  
    }
  }
}    

void mqtt_publish_all(boolean json) {
  String Sub_topic, Mqtt_msg = "";
  byte i;
  for (i=1; i < (sizeof dsmr / sizeof dsmr[0]); i++) {
    if (dsmr[i].value != "") {
      if (json) {
        if (dsmr[i].type == 'f') {    
          Mqtt_msg = "{\"" + dsmr[i].name + "_value" + "\":" + dsmr[i].value.toFloat();
        }  
        else if (dsmr[i].type == 'i') {
          Mqtt_msg = "{\"" + dsmr[i].name + "_value" + "\":" + dsmr[i].value.toInt();
        }
        else { // string
          Mqtt_msg = "{\"" + dsmr[i].name + "_value" + "\":\"" + dsmr[i].value;
        }
        if (dsmr[i].unit != "") {  
          Mqtt_msg = Mqtt_msg + ",\"" + dsmr[i].name + "_unit" + "\":\"" + dsmr[i].unit + "\"}";      
        }  
        else if (dsmr[i].type == 's') {
          Mqtt_msg = Mqtt_msg + "\"}";
        }
        else {
          Mqtt_msg = Mqtt_msg + "}";
        }
      }
      else {
        Mqtt_msg = dsmr[i].name + " = " + dsmr[i].value + " " + dsmr[i].unit;        
      }    
      Sub_topic = MQTT_TOPIC + '/' + dsmr[i].name;
      return_value=mqttClient.publish(Sub_topic.c_str(), Mqtt_msg.c_str());      
      B.log_ln("------------------");      
      B.log("Published message: ");
      B.log_ln(Mqtt_msg);          
    }
  }
}

/********** DEBUG functions ***************************************************/

void print_raw_data(unsigned short serial_data_length) {
  const byte LINE_LENGTH_RAW_SERIAL = 30; 
  B.log("\n-----------------------------------\nRaw length in Byte: ");
  B.log_ln(String(serial_data_length));
  B.log_ln("Raw data: ");
  int mul = (serial_data_length/LINE_LENGTH_RAW_SERIAL);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<LINE_LENGTH_RAW_SERIAL;j++) {
      B.log("0x"+String(telegram[i*LINE_LENGTH_RAW_SERIAL+j],HEX)+','); 
      //B.log(telegram[i*LINE_LENGTH_RAW_SERIAL+j],HEX); 
    }
    B.log_ln();
  }
  for(int j=0; j<(serial_data_length%LINE_LENGTH_RAW_SERIAL);j++) {
    B.log("0x"+String(telegram[mul*LINE_LENGTH_RAW_SERIAL+j],HEX)+',');
    //B.log(telegram[mul*LINE_LENGTH_RAW_SERIAL+j],HEX);
  }
  B.log_ln();  
}

void print_vector(Vector_GCM *vect) {
  const byte LINE_LENGTH_RAW_SERIAL = 30;   
  B.log("\n-----------------------------------\nPrint Vector: ");
  B.log("\nVector_Name: ");
  B.log(vect->name);
  B.log("\nKey: ");
  for(int cnt=0; cnt<16;cnt++) {
    B.log(String(vect->key[cnt]));  
    //B.log(vect->key[cnt],HEX);
  }    
  B.log("\nData (Text): ");
  int mul = (vect->datasize/LINE_LENGTH_RAW_SERIAL);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<LINE_LENGTH_RAW_SERIAL;j++) {
      B.log(String(vect->ciphertext[i*LINE_LENGTH_RAW_SERIAL+j])); 
      //B.log(vect->ciphertext[i*LINE_LENGTH_RAW_SERIAL+j],HEX); 
    }
    B.log_ln();
  }
  for(int j=0; j<(serial_data_length%LINE_LENGTH_RAW_SERIAL);j++) {
    B.log(String(vect->ciphertext[mul*LINE_LENGTH_RAW_SERIAL+j]));
    //B.log(vect->ciphertext[mul*LINE_LENGTH_RAW_SERIAL+j],HEX);
  }  
  B.log("\n\nAuth_Data: ");
  for(int cnt=0; cnt<17;cnt++) {
    B.log(String(vect->authdata[cnt]));
    //B.log(vect->authdata[cnt],HEX);
  }    
  B.log("\nInit_Vect: ");
  for(int cnt=0; cnt<12;cnt++) {
    B.log(String(vect->iv[cnt]));
    //B.log(vect->iv[cnt],HEX);
  }
  B.log("\nAuth_Tag: ");
  for(int cnt=0; cnt<12;cnt++) {
    B.log(String(vect->tag[cnt]));
    //B.log(vect->tag[cnt],HEX);
  }    
  B.log("\nAuth_Data Size: ");
  B.log(String(vect->authsize));
  B.log("\nData Size: ");
  B.log(String(vect->datasize));
  B.log("\nAuth_Tag Size: ");
  B.log(String(vect->tagsize));
  B.log("\nInit_Vect Size: ");
  B.log(String(vect->ivsize));
  B.log_ln();
}
