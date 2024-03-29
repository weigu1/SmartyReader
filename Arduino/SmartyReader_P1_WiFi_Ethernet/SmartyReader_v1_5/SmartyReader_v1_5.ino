/*
  SmartyReader_1_5.ino

  Read P1 port from Luxemburgian smartmeter.
  Decode and publish with MQTT over Wifi or Ethernet
  using ESP8266 (LOLIN/WEMOS D1 mini pro) or ESP32 (MH ET LIVE ESP32MiniKit)
  ESP8266 is recommended (more stable)

  V0.1 2017-08-16
  V1.0 2019-05-04
  V1.1 2019-05-05 OTA added
  V1.2 2019-12-09 now using my new helper lib: ESPBacker
  V1.3 2020-06-23 major changes in code, working with with Strings, publish
                  all data (json or string)
  V1.4 2020-06-24 enable line no more switched (10s data)
  V1.5 2021-04-26 for new boards V2.0 (PCB) without transistor
                  old hardware supported
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

  !! You have to remove the LOLIN/WEMOS from its socket to program it! because the
  hardware serial port is also used during programming !!

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
const long PUBLISH_TIME = 20000;

// Comment or uncomment the following lines suiting your needs
//#define OLD_HARDWARE    // for the boards before V2.0
//#define MQTTPASSWORD     // if you want an MQTT connection with password (recommended!!)
//#define STATIC        // if static IP needed (no DHCP)
//#define ETHERNET      // if Ethernet with Funduino (W5100) instead of WiFi
#define OTA             // if Over The Air update needed (security risk!)
// power and energy are published as JSON string, for more data uncomment the following line
// subscribe to topic/# (e.g. lamsmarty/#)
//#define PUBLISH_ALL 0   // all the data is published 0 for normal string, 1 for json

#include "ESPBacker.h"  // ESP helper lib (more on weigu.lu)
#include <PubSubClient.h>
#include <Crypto.h>
#include <AES.h>
#include <GCM.h>
#include <time.h>

// WiFi and network settings
const char *WIFI_SSID = "";       // SSID
const char *WIFI_PASSWORD = "";   // password
const char *NET_MDNSNAME = "SmartyReader";      // optional (access with mdnsname.local)
const char *NET_HOSTNAME = "SmartyReader";      // optional

// Key for SAG1030700089067 (16 byte!)
uint8_t KEY_SMARTY[] = {0xAE, 0xBD, 0x21, 0xB7, 0x69, 0xA6, 0xD1, 0x3C,
                        0x0D, 0xF0, 0x64, 0xE3, 0x83, 0x68, 0x2E, 0xFF};
// Auth data: hard coded in Lux but not in Austria :)  (17 byte!)
uint8_t AUTH_DATA[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                       0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
                       0xFF};
  
#ifdef STATIC
  IPAddress NET_LOCAL_IP (192,168,1,181);    // 3x optional for static IP
  IPAddress NET_GATEWAY (192,168,1,20);
  IPAddress NET_MASK (255,255,255,0);
#endif // ifdef STATIC*/
#ifdef ETHERNET
  uint8_t NET_MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //only for ethernet
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
const char *MQTT_SERVER = "192.168.1.222";
const char *MQTT_CLIENT_ID = "smarty_lam1_p13";
String MQTT_TOPIC = "lamsmarty";
const short MQTT_PORT = 1883; // or 8883
WiFiClient espClient;
#ifdef MQTTPASSWORD
  const char *MQTT_USER = "me";
  const char *MQTT_PASS = "meagain";
#endif // MQTTPASSWORD

ESPBacker B;
PubSubClient mqttClient(espClient);

// needed to adjust the buffers new data streams > 1024
const uint16_t MAX_SERIAL_STREAM_LENGTH = 1500;
uint8_t telegram[MAX_SERIAL_STREAM_LENGTH];
char buffer[MAX_SERIAL_STREAM_LENGTH-30];

// other variables
char msg[128], filename[13];
uint8_t return_value;
uint16_t serial_data_length = 0;
const uint8_t LINE_LENGTH_RAW_SERIAL = 30;

#ifdef OLD_HARDWARE
  #ifdef ESP8266
    const uint8_t DATA_REQUEST_SM = D3; //active Low! D3 for LOLIN/WEMOS D1 mini pro
  #else
    const uint8_t DATA_REQUEST_SM = 17; //active Low! 17  for MH ET LIVE ESP32MiniKit
  #endif // #ifdef ESP8266
#endif

// Crypto and Smarty variables
struct Vector_GCM {
  const char *name;
  uint8_t key[16];
  uint8_t ciphertext[MAX_SERIAL_STREAM_LENGTH];
  uint8_t authdata[17];
  uint8_t iv[12];
  uint8_t tag[16];
  size_t authsize;
  size_t datasize;
  size_t tagsize;
  size_t ivsize;
};

// DMSR variables
struct Dsmr_field {
  const String name;
  const String id;
  const String unit;
  String value;
  char type;  // string s, float f, integer i
};

// https://electris.lu/files/Dokumente_und_Formulare/P1PortSpecification.pdf
struct Dsmr_field dsmr[] = {
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
  mqttClient.setBufferSize(512);
  mqttClient.setServer(MQTT_SERVER,MQTT_PORT); //open connection to MQTT server
  mqtt_connect();
  #ifdef OTA
    B.init_ota(OTA_NAME, OTA_PASS_HASH);
  #endif // ifdef OTA
  memset(telegram, 0, MAX_SERIAL_STREAM_LENGTH);
  delay(2000); // give it some time
  B.blink_led_x_times(3);
}

/********** LOOP  **************************************************************/

void loop() {
  #ifdef OTA
    ArduinoOTA.handle();
  #endif // ifdef OTA
  serial_data_length = read_telegram();
  if (B.non_blocking_delay(PUBLISH_TIME)) { // Publish every PUBLISH_TIME
    decrypt_and_publish();
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
    #ifdef MQTTPASSWORD
      if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    #else  
      if (mqttClient.connect(MQTT_CLIENT_ID)) { // Attempt to connect
    #endif // ifdef MQTTPASSWORD
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
  #ifdef OLD_HARDWARE
    pinMode(DATA_REQUEST_SM, OUTPUT);
    delay(100);
    #ifdef ESP8266   // true inverts signal
      Serial.begin(115200);
    #else
      Serial.begin(115200,SERIAL_8N1, 1, 3); // change reversed pins of ESP32
    #endif
  #else
    #ifdef ESP8266   // true inverts signal
      Serial.begin(115200, SERIAL_8N1, SERIAL_FULL, 1, true);
    #else
      Serial.begin(115200,SERIAL_8N1, 1, 3, true); // change reversed pins of ESP32
    #endif
  #endif    
  Serial.setRxBufferSize(MAX_SERIAL_STREAM_LENGTH);
}
/********** MQTT functions **************************************************/

void mqtt_publish_all(boolean json) {
  String Sub_topic, Mqtt_msg = "";
  uint8_t i;
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
        Mqtt_msg = dsmr[i].value;
      }
      Sub_topic = MQTT_TOPIC + '/' + dsmr[i].name;
      return_value=mqttClient.publish(Sub_topic.c_str(), Mqtt_msg.c_str());
      B.log_ln("------------------");
      B.log("Published message: ");
      B.log_ln(Mqtt_msg);
    }
  }
}

void mqtt_publish_energy_and_power() {
  struct tm t;
  time_t t_of_day;
  static float energy_consumption_previous = dsmr[4].value.toFloat();
  static float energy_production_previous = dsmr[5].value.toFloat();
  static float energy_consumption_midnight = dsmr[4].value.toFloat();
  static float energy_production_midnight = dsmr[5].value.toFloat();
  float power_consumption = 0;
  float power_production = 0;
  float energy_consumption_cumul_day = 0;
  float energy_production_cumul_day = 0;
  t.tm_year = dsmr[2].value.substring(0,4).toInt()-1900;
  t.tm_mon = dsmr[2].value.substring(5,7).toInt();
  t.tm_mday = dsmr[2].value.substring(8,10).toInt();
  t.tm_hour = dsmr[2].value.substring(11,13).toInt();
  t.tm_min = dsmr[2].value.substring(14,16).toInt();
  t.tm_sec = dsmr[2].value.substring(17,19).toInt();
  t.tm_isdst = -1;        // Is DST on? 1 = yes, 0 = no, -1 = unknown
  t_of_day = mktime(&t);
  static unsigned long epoch_previous = t_of_day;
  unsigned long epoch = t_of_day;
  float energy_consumption = dsmr[4].value.toFloat();
  float energy_production = dsmr[5].value.toFloat();
  int delta_time = epoch_previous-epoch;
  power_consumption = (energy_consumption_previous-energy_consumption)*3600/delta_time;
  power_production = (energy_production_previous-energy_production)*3600/delta_time;
  energy_consumption_cumul_day = energy_consumption - energy_consumption_midnight;
  energy_production_cumul_day = energy_production - energy_production_midnight;
  B.log_ln("*****************");
  B.log_ln(String(power_consumption));
  B.log_ln(String(power_production));
  B.log_ln(String(delta_time));
  B.log_ln("*****************");
  String Sub_topic, Mqtt_msg = "";
  Mqtt_msg = "{\"" + dsmr[2].name + "\":\"" + dsmr[2].value + "\",\"" + dsmr[3].name + "\":\"" +
             dsmr[3].value +  "\",\"energy_consumption [" + dsmr[4].unit + "]\":" +
             energy_consumption + ",\"energy_consumption_cumul_day [kWh]\":" +
             energy_consumption_cumul_day + ",\"energy_production [" + dsmr[5].unit + "]\":" +
             energy_production + ",\"energy_production_cumul_day [kWh]\":" +
             energy_production_cumul_day + ",\"power_consumption [kW]\":" +
             power_consumption + ",\"power_production [kW]\":" + power_production + "}";
  return_value=mqttClient.publish(MQTT_TOPIC.c_str(), Mqtt_msg.c_str());
  B.log_ln(String(return_value));
  B.log_ln("------------------");
  B.log("Published message: ");
  B.log_ln(Mqtt_msg);
  energy_consumption_previous = dsmr[4].value.toFloat();
  energy_production_previous = dsmr[5].value.toFloat();
  epoch_previous = t_of_day;
  if ((t.tm_hour == 23) && (t.tm_min >= 55)) {
    energy_consumption_midnight = dsmr[4].value.toFloat();
  }
  if ((t.tm_hour == 23) && (t.tm_min >= 55)) {
    energy_production_midnight = dsmr[5].value.toFloat();
  }
}


/********** SMARTY functions **************************************************/

uint16_t read_telegram() {
  uint16_t serial_cnt = 0;
  #ifdef OLD_HARDWARE
    digitalWrite(DATA_REQUEST_SM,LOW);   // set request line to 5V    
  #endif
  if (Serial.available()) {delay(500);}  // wait for the whole stream
  #ifdef OLD_HARDWARE
    digitalWrite(DATA_REQUEST_SM,HIGH);  // x*86,8µs (1024 uint8_t need 88ms)
  #endif  
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

void decrypt_and_publish() {
  B.log_ln("-----------------------------------");
  B.log("Get data from Smarty and publish");
  //print_raw_data(serial_data_length);  // for thorough debugging
  init_vector(&Vector_SM,"Vector_SM", KEY_SMARTY, AUTH_DATA);
  //print_vector(&Vector_SM);            // for thorough debugging
  if (Vector_SM.datasize != MAX_SERIAL_STREAM_LENGTH) {
    decrypt_text(&Vector_SM);
    parse_dsmr_string(buffer);
    #ifdef PUBLISH_ALL
      mqtt_publish_all(PUBLISH_ALL); // 0 for simple string, 1 for json cooked
    #else
      mqtt_publish_energy_and_power();
    #endif  
    B.blink_led_x_times(4);
  }
  else {    //max datalength reached error!
    B.log_ln("error, serial stream too big");
    B.blink_led_x_times(10);
  }
}

void init_vector(Vector_GCM *vect, const char *Vect_name, uint8_t *key_SM, uint8_t *auth_data) {
  vect->name = Vect_name;  // vector name    
  for (int i = 0; i < 16; i++) {
    vect->key[i] = key_SM[i];
  }
  uint16_t data_length = uint16_t(telegram[11])*256 + uint16_t(telegram[12])-17; // get length of data
  if (data_length>MAX_SERIAL_STREAM_LENGTH) {
    data_length = MAX_SERIAL_STREAM_LENGTH; // mark error
  }
  for (int i = 0; i < data_length; i++) {
    vect->ciphertext[i] = telegram[i+18];
  }  
  for (int i = 0; i < 17; i++) {
     vect->authdata[i] = auth_data[i];
  }
  for (int i = 0; i < 8; i++) {
     vect->iv[i] = telegram[2+i];
  }
  for (int i = 8; i < 12; i++) {
    vect->iv[i] = telegram[6+i];
  }
  for (int i = 0; i < 12; i++) {
    vect->tag[i] = telegram[data_length+18+i];
  }
  vect->authsize = 17;
  vect->datasize = data_length;
  vect->tagsize = 12;
  vect->ivsize  = 12;
  memset(telegram, 0, MAX_SERIAL_STREAM_LENGTH);
}

void decrypt_text(Vector_GCM *vect) {
  memset(buffer, 0, MAX_SERIAL_STREAM_LENGTH-30);
  gcmaes128 = new GCM<AES128>();
  gcmaes128->setKey(vect->key, gcmaes128->keySize());
  gcmaes128->setIV(vect->iv, vect->ivsize);
  gcmaes128->decrypt((uint8_t*)buffer, vect->ciphertext, vect->datasize);
  delete gcmaes128;
}

void parse_dsmr_string(String Dmsr) {
  int field_index, uint8_t_cnt, i, j;
  String Tmp;
  B.log_ln("+++++++++++++++++++++++++++++++++++++++");
  B.log_ln("DMSR:");
  B.log_ln(Dmsr);
  // get the identification
  uint8_t_cnt = Dmsr.indexOf('\n');
  dsmr[0].value = Dmsr.substring(0,uint8_t_cnt-1);
  B.log_ln("+++++++++++++++++++++++++++++++++++++++");
  B.log_ln(dsmr[0].value);
  // run through all the OBIS fields
  for (i = 1; i < (sizeof dsmr / sizeof dsmr[0]); i++) {
    field_index = Dmsr.indexOf(dsmr[i].id);  // get field_indexes of OBIS codes
    if (field_index != -1) { // -1 = not found
      uint8_t_cnt = field_index;
      while (Dmsr[uint8_t_cnt] != '(') { // go to '('
        uint8_t_cnt++;
      }
      if ((dsmr[i].id)=="0-1:24.2.1") { // Merci Serge
        uint8_t_cnt++; // get past '('
        while (Dmsr[uint8_t_cnt] != '(') { // go to '('
          uint8_t_cnt++;
        }
      }
      Tmp = "";
      uint8_t_cnt++; // get past '('
      while (Dmsr[uint8_t_cnt] != ')') { // get the String between brackets
        Tmp.concat(Dmsr[uint8_t_cnt]);
        uint8_t_cnt++;
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
/********** DEBUG functions ***************************************************/

void print_raw_data(uint16_t serial_data_length) {
  B.log("\n-----------------------------------\nRaw length in uint8_t: ");
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
