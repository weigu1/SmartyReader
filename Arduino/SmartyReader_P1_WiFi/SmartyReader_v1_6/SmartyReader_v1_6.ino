/*
  SmartyReader_1_6.ino

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
  V1.6 2022-07-16 Using config or secrets file for password, 3 new gas fields,
                  BME280 suppport, changed ESPBacker to ESPToolbox,
                  added Ethernet, bugfixes
  ---------------------------------------------------------------------------
  Copyright (C) 2017 Guy WEILER www.weigu.lu

  With ideas from Sam Grimee, Bob Fisch (www.fisch.lu), and Serge Wagner
  Alternative Software: https://githuTb.com/sgrimee

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

/*!!!!!!!!!! make your changes in config.h (or secrets.h) !!!!!!!!!*/

/*!!!!!!!!!! to debug you can use the onboard LED, a second serial port on D4
             or best: UDP! Look in setup()                !!!!!!!!!*/
             
/*?????? Comment or uncomment the following lines suiting your needs ??????*/

/* The file "secrets.h" has to be placed in the sketchbook libraries folder
   in a folder named "Secrets" and must contain the same things than the file config.h*/
//#define USE_SECRETS
#define OTA               // if Over The Air update needed (security risk!)
//#define OLD_HARDWARE    // for the boards before V2.0
//#define MQTTPASSWORD    // if you want an MQTT connection with password (recommended!!)
//#define STATIC          // if static IP needed (no DHCP)
//#define ETHERNET        // if Ethernet with Funduino (W5100) instead of WiFi
//#define BME280_I2C      // if you want to add a temp sensor to I2C connector
#define GET_NTP_TIME    // if you need the real time
/* everything item (DSMR and calculated) is normally published under its own topic
 * in config.h (or secrets.h) you can decide with 'y/n' if you want to publish it
 * PUBLISH_COOKED is a JSON String with the calculated values (needed by me :))*/
//#define PUBLISH_COOKED

/****** Arduino libraries needed ******/
#ifdef USE_SECRETS
  #include <secrets.h>
#else  
  #include "config.h"      // most of the things you need to change are here
#endif // USE_SECRETS  
#include "ESPToolbox.h"  // ESP helper lib (more on weigu.lu)
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <CircularBuffer.h>
#include <Crypto.h>
#include <AES.h>
#include <GCM.h>
#include <math.h>
//#include <time.h>
#ifdef BME280_I2C
  #include <Wire.h>
  #include <BME280I2C.h>
#endif

/****** WiFi and network settings ******/
const char *WIFI_SSID = MY_WIFI_SSID;           // if no secrets file, use the config.h file
const char *WIFI_PASSWORD = MY_WIFI_PASSWORD;   // if no secrets file, use the config.h file
#if defined(STATIC) || defined(ETHERNET)
  IPAddress NET_LOCAL_IP (NET_LOCAL_IP_BYTES);  // 3x optional for static IP
  IPAddress NET_GATEWAY (NET_GATEWAY_BYTES);    // look in config.h
  IPAddress NET_MASK (NET_MASK_BYTES);
  IPAddress NET_DNS (NET_MASK_BYTES);
#endif // #if defined(STATIC) || defined(ETHERNET)
#ifdef OTA                                // Over The Air update settings
  const char *OTA_NAME = MY_OTA_NAME;
  const char *OTA_PASS_HASH = MY_OTA_PASS_HASH;  // use the config.h file
#endif // ifdef OTA

IPAddress UDP_LOG_PC_IP(UDP_LOG_PC_IP_BYTES);     // UDP logging if enabled in setup() (config.h)

/****** MQTT settings ******/
const short MQTT_PORT = MY_MQTT_PORT;
#ifdef ETHERNET
  EthernetClient espClient;
#else
  WiFiClient espClient;
#endif // ifdef ETHERNET
PubSubClient MQTT_Client(espClient);
#ifdef MQTTPASSWORD
  const char *MQTT_USER = MY_MQTT_USER;
  const char *MQTT_PASS = MY_MQTT_PASS;
#endif // MQTTPASSWORD
String mqtt_msg;

/******* BME280 ******/
float temp(NAN), hum(NAN), pres(NAN);
#ifdef BME280_I2C
  BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                    // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,
#endif

const int SAMPLES = SAMPLE_TIME_MIN*6;
CircularBuffer<double,SAMPLES> ring_buffer_pc; 
CircularBuffer<double,SAMPLES> ring_buffer_pc_l1;
CircularBuffer<double,SAMPLES> ring_buffer_pc_l2;
CircularBuffer<double,SAMPLES> ring_buffer_pc_l3;
CircularBuffer<double,SAMPLES> ring_buffer_pp;
CircularBuffer<double,SAMPLES> ring_buffer_pp_l1;
CircularBuffer<double,SAMPLES> ring_buffer_pp_l2;
CircularBuffer<double,SAMPLES> ring_buffer_pp_l3;
CircularBuffer<double,SAMPLES> ring_buffer_es;

ESPToolbox Tb;

// needed to adjust the buffers new data streams > 1024
const uint16_t MAX_SERIAL_STREAM_LENGTH = 1800;
uint8_t telegram[MAX_SERIAL_STREAM_LENGTH];
char buffer[MAX_SERIAL_STREAM_LENGTH-30];

// other variables
uint16_t serial_data_length = 0;           // Data bytes of the raw serial stream 
const uint8_t LINE_LENGTH_RAW_SERIAL = 25; // Bytes to print per line if we want to see raw serial

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

Vector_GCM Vector_SM;
GCM<AES128> *gcmaes128 = 0;

/********** SETUP *************************************************************/

void setup() {
  Tb.set_led_log(true);                 // use builtin LED for debugging
  Tb.set_serial_log(true,1);           // 2 parameter = interface (1 = Serial1)
  Tb.set_udp_log(true, UDP_LOG_PC_IP, UDP_LOG_PORT); // use "nc -kulw 0 6666"
  #ifdef STATIC
    Tb.set_static_ip(true,NET_LOCAL_IP, NET_GATEWAY, NET_MASK, NET_DNS);
  #endif // ifdef STATIC
  #ifdef STATIC
    Tb.set_static_ip(true,NET_LOCAL_IP, NET_GATEWAY, NET_MASK, NET_DNS);
  #endif // ifdef STATIC
  #ifdef ETHERNET
    Tb.set_ethernet(true);
    Tb.init_eth(NET_MAC);
  #else
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_MDNSNAME, NET_HOSTNAME);
  #endif // ifdef ETHERNET
  #ifdef GET_NTP_TIME
    Tb.init_ntp_time();
  #endif // ifdef GET_NTP_TIME
  init_serial4smarty();
  MQTT_Client.setBufferSize(MQTT_MAXIMUM_PACKET_SIZE);
  MQTT_Client.setServer(MQTT_SERVER,MQTT_PORT); //open connection to MQTT server
  mqtt_connect();
  #ifdef OTA
    Tb.init_ota(OTA_NAME, OTA_PASS_HASH);
  #endif // ifdef OTA
  memset(telegram, 0, MAX_SERIAL_STREAM_LENGTH);
  #ifdef BME280_I2C
    init_bme280();     
  #endif
  delay(2000); // give it some time
  Tb.blink_led_x_times(3);
}

/********** LOOP  **************************************************************/

void loop() {
  #ifdef OTA
    ArduinoOTA.handle();
  #endif // ifdef OTA
  read_telegram();  
  if ((telegram[0] == 0xdb) && (telegram[1] != 0) && (telegram[2] != 0)) {  //valid telegram
    decrypt_and_calculate(SAMPLES);    
    if (Tb.non_blocking_delay(PUBLISH_TIME)) { // Publish every PUBLISH_TIME
      Tb.log("HuHu");
      #ifdef PUBLISH_COOKED        
        mqtt_publish_cooked();
      #else
        mqtt_publish_all();
      #endif // PUBLISH_COOKED
      Tb.blink_led_x_times(4);
    }
  }  
  #ifdef ETHERNET
    if (!espClient.connected()) {
      Tb.init_eth(NET_MAC);
    }
  #else
    if (WiFi.status() != WL_CONNECTED) {   // if WiFi disconnected, reconnect
      Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_MDNSNAME, NET_HOSTNAME);
    }
  #endif // ifdef ETHERNET
  if (!MQTT_Client.connected()) { // reconnect mqtt client, if needed
    mqtt_connect();
  }
  MQTT_Client.loop();                    // make the MQTT live
  
  yield();
}

/********** INIT functions ****************************************************/

// connect to MQTT server
void mqtt_connect() {
  while (!MQTT_Client.connected()) { // Loop until we're reconnected
    Tb.log("Attempting MQTT connection...");
    #ifdef MQTTPASSWORD
      if (MQTT_Client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    #else  
      if (MQTT_Client.connect(MQTT_CLIENT_ID)) { // Attempt to connect
    #endif // ifdef MQTTPASSWORD
      Tb.log_ln("MQTT connected");
      // Once connected, publish an announcement...
      //MQTT_Client.publish(MQTT_TOPIC_OUT, "{\"dt\":\"connected\"}");
      // don't because OpenHAB does not like this ...
    }
    else {
      Tb.log("MQTT connection failed, rc=");
      Tb.log(String(MQTT_Client.state()));
      Tb.log_ln(" try again in 5 seconds");
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
    #else // ESP32
      Serial.begin(115200,SERIAL_8N1, 1, 3); // change reversed pins of ESP32
    #endif //ESP8266
  #else
    #ifdef ESP8266   // true inverts signal
      Serial.begin(115200, SERIAL_8N1, SERIAL_FULL, 1, true);
    #else // new hardware 
      Serial.begin(115200,SERIAL_8N1, 1, 3, true); // change reversed pins of ESP32
    #endif
  #endif // OLD_HARDWARE
  Serial.setRxBufferSize(MAX_SERIAL_STREAM_LENGTH);
}
/********** MQTT functions **************************************************/

void mqtt_publish_all() {
  String Sub_topic, Mqtt_msg = "";
  uint8_t i;
  // if we want NTP time
  #ifdef GET_NTP_TIME
    Tb.get_time();   
    Mqtt_msg = Tb.t.datetime;
    Sub_topic = MQTT_TOPIC_OUT + '/' + "ntp_datetime";
    MQTT_Client.publish(Sub_topic.c_str(), Mqtt_msg.c_str());
    Tb.log_ln("Published message (NTP): " + Sub_topic + "   " + Mqtt_msg);    
  #endif // GET_NTP_TIME  
  // if we want BME280
  #ifdef BME280_I2C
    get_data_bme280();
    Mqtt_msg = String((int)(temp*10.0 + 0.5)/10.0);
    Sub_topic = MQTT_TOPIC_OUT + '/' + "bme280_temperature_C";
    MQTT_Client.publish(Sub_topic.c_str(), Mqtt_msg.c_str());
    Tb.log_ln("Published message (BME280): " + Sub_topic + "   " + Mqtt_msg);
    Mqtt_msg = String((int)(hum*10.0 + 0.5)/10.0);
    Sub_topic = MQTT_TOPIC_OUT + '/' + "bme280_humidity_%";
    MQTT_Client.publish(Sub_topic.c_str(), Mqtt_msg.c_str());
    Tb.log_ln("Published message (BME280): " + Sub_topic + "   " + Mqtt_msg);
    Mqtt_msg = String((int)((pres + 5)/10)/10.0);
    Sub_topic = MQTT_TOPIC_OUT + '/' + "bme280_pressure_hPa";
    MQTT_Client.publish(Sub_topic.c_str(), Mqtt_msg.c_str());
    Tb.log_ln("Published message (BME280): " + Sub_topic + "   " + Mqtt_msg);    
  #endif // GET_NTP_TIME  
  // DSMR data:
  for (i=2; i < (sizeof dsmr / sizeof dsmr[0]); i++) {
    if ((dsmr[i].value != "NA") && (dsmr[i].publish == 'y')) {
      if (dsmr[i].type == 'f') {
        Mqtt_msg = String(dsmr[i].value.toDouble());
      }  
      else if (dsmr[i].type == 'i') {
        Mqtt_msg = String(dsmr[i].value.toInt());
      }  
      else {
        Mqtt_msg = String(dsmr[i].value);
      }  
      if (dsmr[i].unit != "") {
        Sub_topic = MQTT_TOPIC_OUT + '/' + dsmr[i].name + '_' + dsmr[i].unit;
      }
      else {
        Sub_topic = MQTT_TOPIC_OUT + '/' + dsmr[i].name;
      }        
      MQTT_Client.publish(Sub_topic.c_str(), Mqtt_msg.c_str());      
      Tb.log_ln("Published message (Smartmeter field nr " + String(i) + "): " + Sub_topic + "   " + Mqtt_msg);
    }
    else {
      Tb.log_ln("No value for DMSR field nr: " + String(i) + " or shall not be published (no ''y'' in config.h).");
    }    
  }
  // calculated data
  for (i=0; i < (sizeof calculated_parameter / sizeof calculated_parameter[0]); i++) {
    if (calculated_parameter[i].publish == 'y') {
      Mqtt_msg = String(calculated_parameter[i].value);
      Sub_topic = MQTT_TOPIC_OUT + '/' + calculated_parameter[i].name;
      MQTT_Client.publish(Sub_topic.c_str(), Mqtt_msg.c_str());
      Tb.log_ln("Published message (Calculated field nr " + String(i) + "): " + Sub_topic + "   " + Mqtt_msg);
    }
    else {
      Tb.log_ln("No value for Calculated field nr: " + String(i) + " or shall not be published (no ''y'' in config.h).");
    }          
  }  
}

/**/
void mqtt_publish_cooked() {  
  DynamicJsonDocument doc_out(MQTT_MAXIMUM_PACKET_SIZE);
  String Sub_topic, Mqtt_msg = "";  
  #ifdef GET_NTP_TIME
    Tb.get_time();    
    doc_out["ntp_datetime"] = Tb.t.datetime;    
  #endif // GET_NTP_TIME
  #ifdef BME280_I2C
    get_data_bme280();
    doc_out["bme280_temperature_C"] = (int)(temp*10.0 + 0.5)/10.0;
    doc_out["bme280_humidity_%"] = (int)(hum*10.0 + 0.5)/10.0;
    doc_out["bme280_pressure_hPa"] = (int)((pres + 5)/10)/10.0;
  #endif    
  doc_out["smarty_datetime"] = dsmr[2].value;
  doc_out["smarty_id"] = dsmr[3].value;
  for (byte i=0; i<(sizeof(calculated_parameter)/24); i++) {
    if (calculated_parameter[i].publish == 'y') {
      doc_out[calculated_parameter[i].name] = calculated_parameter[i].value;  
    }
  }    
  serializeJson(doc_out, Mqtt_msg);
  MQTT_Client.publish(MQTT_TOPIC_OUT.c_str(), Mqtt_msg.c_str());  
  Tb.log_ln("Published message (Smartmeter): " + Sub_topic + "   " + Mqtt_msg);  
}


/********** SMARTY functions **************************************************/

void read_telegram() {
  uint16_t serial_cnt = 0;  
  #ifdef OLD_HARDWARE
    digitalWrite(DATA_REQUEST_SM,LOW);   // set request line to 5V    
  #endif // OLD_HARDWARE
  if (Serial.available()) {   // wait for the whole stream
    delay(500);
    }  
  #ifdef OLD_HARDWARE
    digitalWrite(DATA_REQUEST_SM,HIGH);  // x*86,8µs (1024 uint8_t need 88ms)
  #endif // OLD_HARDWARE
  while ((Serial.available()) && (serial_cnt < MAX_SERIAL_STREAM_LENGTH)) {
    telegram[serial_cnt] = Serial.read();
    if (telegram[0] != 0xDB) {      
      while (Serial.available() > 0) {Serial.read();} //clear the buffer!    
      break;
    }
    serial_cnt++;
  }
  if (serial_cnt > 500) {
    serial_data_length = serial_cnt;
  }
}


/* Here we calculate power from energy, the energy/day and the excess solar power */
void calculate_energy_and_power(int samples) {  
  struct tm t;
  time_t t_of_day;
  unsigned long epoch = 0;  
  int delta_time = 0;
  double energy_consumption = 0.0, energy_production = 0.0;
  double power_consumption = 0.0, power_production = 0.0;
  double power_consumption_calc_from_energy = 0.0, power_production_calc_from_energy = 0.0;
  double power_consumption_l1 = 0.0, power_consumption_l2 = 0.0, power_consumption_l3 = 0.0;
  double power_production_l1 = 0.0, power_production_l2 = 0.0, power_production_l3 = 0.0;    
  double power_excess_solar = 0.0, power_l1_excess_solar = 0.0, power_l2_excess_solar = 0.0, power_l3_excess_solar = 0.0;  
  double power_excess_solar_mean = 0.0, power_excess_solar_max = 0.0, power_excess_solar_min = 50000.0;  
  double energy_consumption_cumul_day = 0.0, energy_production_cumul_day = 0.0;  
  double power_consumption_calc_mean = 0.0, power_consumption_calc_max = 0.0, power_consumption_calc_min = 50000.0;  
  double power_consumption_l1_calc_mean = 0.0, power_consumption_l1_calc_max = 0.0, power_consumption_l1_calc_min = 50000.0;  
  double power_consumption_l2_calc_mean = 0.0, power_consumption_l2_calc_max = 0.0, power_consumption_l2_calc_min = 50000.0;  
  double power_consumption_l3_calc_mean = 0.0, power_consumption_l3_calc_max = 0.0, power_consumption_l3_calc_min = 50000.0;    
  double power_production_calc_mean = 0.0, power_production_calc_max = 0.0, power_production_calc_min = 50000.0;  
  double power_production_l1_calc_mean = 0.0, power_production_l1_calc_max = 0.0, power_production_l1_calc_min = 50000.0;  
  double power_production_l2_calc_mean = 0.0, power_production_l2_calc_max = 0.0, power_production_l2_calc_min = 50000.0;  
  double power_production_l3_calc_mean = 0.0, power_production_l3_calc_max = 0.0, power_production_l3_calc_min = 50000.0;    
  static unsigned long epoch_previous = 0;
  static double power_consumption_sum = 0.0, power_consumption_l1_sum = 0.0, power_consumption_l2_sum = 0.0, power_consumption_l3_sum = 0.0;
  static double power_production_sum = 0.0, power_production_l1_sum = 0.0, power_production_l2_sum = 0.0, power_production_l3_sum = 0.0;
  static double power_excess_solar_sum = 0.0;
  static double energy_consumption_previous = dsmr[4].value.toDouble()*1000.0;
  static double energy_production_previous = dsmr[5].value.toDouble()*1000.0;
  static double energy_consumption_midnight = dsmr[4].value.toDouble()*1000.0;
  static double energy_production_midnight = dsmr[5].value.toDouble()*1000.0;  
  // get the time of the day
  t.tm_year = dsmr[2].value.substring(0,4).toInt()-1900;
  t.tm_mon = dsmr[2].value.substring(5,7).toInt();
  t.tm_mday = dsmr[2].value.substring(8,10).toInt();
  t.tm_hour = dsmr[2].value.substring(11,13).toInt();
  t.tm_min = dsmr[2].value.substring(14,16).toInt();
  t.tm_sec = dsmr[2].value.substring(17,19).toInt();
  t.tm_isdst = -1;        // Is DST on? 1 = yes, 0 = no, -1 = unknown
  t_of_day = mktime(&t);
  if (epoch_previous != 0) { //to skip the first call (static variables still 0)    
    epoch = t_of_day;
    delta_time = epoch-epoch_previous;
    // get data in Wh resp. W
    energy_consumption = dsmr[4].value.toDouble()*1000.0;  
    energy_production = dsmr[5].value.toDouble()*1000.0;
    power_consumption = dsmr[8].value.toDouble()*1000.0;
    power_production = dsmr[9].value.toDouble()*1000.0; 
    power_consumption_l1 = dsmr[30].value.toDouble()*1000.0;
    power_consumption_l2 = dsmr[31].value.toDouble()*1000.0;
    power_consumption_l3 = dsmr[32].value.toDouble()*1000.0;
    power_production_l1 = dsmr[33].value.toDouble()*1000.0; 
    power_production_l2 = dsmr[34].value.toDouble()*1000.0; 
    power_production_l3 = dsmr[35].value.toDouble()*1000.0;         
    // calculate power_from_energy, cumul day and excess power       
    power_consumption_calc_from_energy = (energy_consumption-energy_consumption_previous)*3600/delta_time;
    power_production_calc_from_energy = (energy_production-energy_production_previous)*3600/delta_time;
    energy_consumption_cumul_day = energy_consumption - energy_consumption_midnight;
    energy_production_cumul_day = energy_production - energy_production_midnight;
    energy_consumption_cumul_day = constrain(energy_consumption_cumul_day,0, 70000); 
    energy_production_cumul_day = constrain(energy_production_cumul_day,0, 70000); 
    power_excess_solar = power_production - power_consumption;        
    power_l1_excess_solar = power_production_l1 - power_consumption_l1;
    power_l2_excess_solar = power_production_l2 - power_consumption_l2;
    power_l3_excess_solar = power_production_l3 - power_consumption_l3;
    // write to ringbuffer
    ring_buffer_pc.push(power_consumption); 
    ring_buffer_pc_l1.push(power_consumption_l1); 
    ring_buffer_pc_l2.push(power_consumption_l2); 
    ring_buffer_pc_l3.push(power_consumption_l3);     
    ring_buffer_pp.push(power_production); 
    ring_buffer_pp_l1.push(power_production_l1); 
    ring_buffer_pp_l2.push(power_production_l2); 
    ring_buffer_pp_l3.push(power_production_l3); 
    ring_buffer_es.push(power_excess_solar); 
    // calculate mean, max and min for all powers
    power_consumption_sum = 0.0;
    power_consumption_l1_sum = 0.0;
    power_consumption_l2_sum = 0.0;
    power_consumption_l3_sum = 0.0;
    power_production_sum = 0.0;
    power_production_l1_sum = 0.0;
    power_production_l2_sum = 0.0;
    power_production_l3_sum = 0.0;
    power_excess_solar_sum = 0.0;
    for (byte i=0; i<samples; i++) {
      power_consumption_sum += ring_buffer_pc[i];
      power_consumption_calc_max = max(power_consumption_calc_max,ring_buffer_pc[i]);
      power_consumption_calc_min = min(power_consumption_calc_min,ring_buffer_pc[i]);
      power_consumption_l1_sum += ring_buffer_pc_l1[i];
      power_consumption_l1_calc_max = max(power_consumption_l1_calc_max,ring_buffer_pc_l1[i]);
      power_consumption_l1_calc_min = min(power_consumption_l1_calc_min,ring_buffer_pc_l1[i]);
      power_consumption_l2_sum += ring_buffer_pc_l2[i];
      power_consumption_l2_calc_max = max(power_consumption_l2_calc_max,ring_buffer_pc_l2[i]);
      power_consumption_l2_calc_min = min(power_consumption_l2_calc_min,ring_buffer_pc_l2[i]);
      power_consumption_l3_sum += ring_buffer_pc_l3[i];
      power_consumption_l3_calc_max = max(power_consumption_l3_calc_max,ring_buffer_pc_l3[i]);
      power_consumption_l3_calc_min = min(power_consumption_l3_calc_min,ring_buffer_pc_l3[i]);      
      power_production_sum += ring_buffer_pp[i];
      power_production_calc_max = max(power_production_calc_max,ring_buffer_pp[i]);
      power_production_calc_min = min(power_production_calc_min,ring_buffer_pp[i]);
      power_production_l1_sum += ring_buffer_pp_l1[i];
      power_production_l1_calc_max = max(power_production_l1_calc_max,ring_buffer_pp_l1[i]);
      power_production_l1_calc_min = min(power_production_l1_calc_min,ring_buffer_pp_l1[i]);
      power_production_l2_sum += ring_buffer_pp_l2[i];
      power_production_l2_calc_max = max(power_production_l2_calc_max,ring_buffer_pp_l2[i]);
      power_production_l2_calc_min = min(power_production_l2_calc_min,ring_buffer_pp_l2[i]);
      power_production_l3_sum += ring_buffer_pp_l3[i];
      power_production_l3_calc_max = max(power_production_l3_calc_max,ring_buffer_pp_l3[i]);
      power_production_l3_calc_min = min(power_production_l3_calc_min,ring_buffer_pp_l3[i]);            
      power_excess_solar_sum += ring_buffer_es[i];
      power_excess_solar_max = max(power_excess_solar_max,ring_buffer_es[i]);
      power_excess_solar_min = min(power_excess_solar_min,ring_buffer_es[i]);      
    }
    power_consumption_calc_mean = round(power_consumption_sum/samples);
    power_consumption_l1_calc_mean = round(power_consumption_l1_sum/samples);
    power_consumption_l2_calc_mean = round(power_consumption_l2_sum/samples);
    power_consumption_l3_calc_mean = round(power_consumption_l3_sum/samples);
    power_production_calc_mean = round(power_production_sum/samples);
    power_production_l1_calc_mean = round(power_production_l1_sum/samples);
    power_production_l2_calc_mean = round(power_production_l2_sum/samples);
    power_production_l3_calc_mean = round(power_production_l3_sum/samples);
    power_excess_solar_mean = round(power_excess_solar_sum/samples);
    epoch_previous = t_of_day;  
    if ((t.tm_hour == 23) && (t.tm_min >= 55)) {
      energy_consumption_midnight = energy_consumption;
      energy_production_midnight = energy_production;
    }
    energy_consumption_previous = energy_consumption;
    energy_production_previous = energy_production;      
    calculated_parameter[0].value = energy_consumption/1000.0; 
    calculated_parameter[1].value = energy_production/1000.0;    
    calculated_parameter[2].value = constrain(energy_consumption_cumul_day,0,100000);
    calculated_parameter[3].value = constrain(energy_production_cumul_day,0,100000);
    calculated_parameter[4].value = power_consumption_calc_from_energy;    
    calculated_parameter[5].value = power_production_calc_from_energy;
    calculated_parameter[6].value = power_consumption;    
    calculated_parameter[7].value = power_consumption_calc_mean;
    calculated_parameter[8].value = power_consumption_calc_max;
    calculated_parameter[9].value = power_consumption_calc_min;    
    calculated_parameter[10].value = power_consumption_l1;
    calculated_parameter[11].value = power_consumption_l1_calc_mean;
    calculated_parameter[12].value = power_consumption_l1_calc_max;
    calculated_parameter[13].value = power_consumption_l1_calc_min;
    calculated_parameter[14].value = power_consumption_l2;
    calculated_parameter[15].value = power_consumption_l2_calc_mean;
    calculated_parameter[16].value = power_consumption_l2_calc_max;
    calculated_parameter[17].value = power_consumption_l2_calc_min;
    calculated_parameter[18].value = power_consumption_l3;    
    calculated_parameter[19].value = power_consumption_l3_calc_mean;
    calculated_parameter[20].value = power_consumption_l3_calc_max;
    calculated_parameter[21].value = power_consumption_l3_calc_min;
    calculated_parameter[22].value = power_production;    
    calculated_parameter[23].value = power_production_calc_mean;
    calculated_parameter[24].value = power_production_calc_max;
    calculated_parameter[25].value = power_production_calc_min; 
    calculated_parameter[26].value = power_production_l1;    
    calculated_parameter[27].value = power_production_l1_calc_mean;
    calculated_parameter[28].value = power_production_l1_calc_max;
    calculated_parameter[29].value = power_production_l1_calc_min;     
    calculated_parameter[30].value = power_production_l2;    
    calculated_parameter[31].value = power_production_l2_calc_mean;
    calculated_parameter[32].value = power_production_l2_calc_max;
    calculated_parameter[33].value = power_production_l2_calc_min; 
    calculated_parameter[34].value = power_production_l3;    
    calculated_parameter[35].value = power_production_l3_calc_mean;
    calculated_parameter[36].value = power_production_l3_calc_max;
    calculated_parameter[37].value = power_production_l3_calc_min; 
    calculated_parameter[38].value = constrain(power_excess_solar,0,30000);
    calculated_parameter[39].value = constrain(power_excess_solar_mean,0,30000);
    calculated_parameter[40].value = constrain(power_excess_solar_max,0,30000);
    calculated_parameter[41].value = constrain(power_excess_solar_min,0,30000);
    calculated_parameter[42].value = constrain(power_l1_excess_solar,0,30000);
    calculated_parameter[43].value = constrain(power_l2_excess_solar,0,30000);
    calculated_parameter[44].value = constrain(power_l3_excess_solar,0,30000);
  }  
  else {    
    epoch_previous = t_of_day;
    Serial.println(epoch_previous);
  }  
}

void decrypt_and_calculate(int samples) {
  Tb.log_ln("-----------------------------------");
  Tb.log_ln("Decrypt and calculate");
  //print_raw_data(serial_data_length);  // for thorough debugging
  init_vector(&Vector_SM,"Vector_SM", KEY_SMARTY, AUTH_DATA);
  //print_vector(&Vector_SM);            // for thorough debugging    
  if (Vector_SM.datasize != MAX_SERIAL_STREAM_LENGTH) {
    decrypt_text(&Vector_SM);
    parse_dsmr_string(buffer);  
    calculate_energy_and_power(samples);  
  }
  else {    //max datalength reached error!
    Tb.log_ln("\nerror, serial stream too big");
    Tb.blink_led_x_times(10);
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
  Tb.log_ln("+++++++++++++++++++++++++++++++++++++++");
  Tb.log_ln("DMSR:");
  Tb.log_ln(Dmsr);
  // get the identification
  uint8_t_cnt = Dmsr.indexOf('\n');
  dsmr[0].value = Dmsr.substring(0,uint8_t_cnt-1);
  Tb.log_ln("+++++++++++++++++++++++++++++++++++++++");
  Tb.log_ln(dsmr[0].value);
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
      Tb.log_ln(dsmr[i].value);
    }
  }
}

/********** BME280 functions *************************************************/

// initialise the BME280 sensor
#ifdef BME280_I2C
  void init_bme280() {
    Wire.begin();
    while(!bme.begin()) {
      Tb.log_ln("Could not find BME280 sensor!");
      delay(1000);
    }
    switch(bme.chipModel())  {
       case BME280::ChipModel_BME280:
         Tb.log_ln("Found BME280 sensor! Success.");
         break;
       case BME280::ChipModel_BMP280:
         Tb.log_ln("Found BMP280 sensor! No Humidity available.");
         break;
       default:
         Tb.log_ln("Found UNKNOWN sensor! Error!");
    }
  }

// get BME280 data and log it
void get_data_bme280() {
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme.read(pres, temp, hum, tempUnit, presUnit);
  Tb.log_ln("BME 280: Temp = " + (String(temp)) + "C Hum = " + (String(hum)) + 
           "% Pres = " + (String(pres)) + "hPA");
}
#endif  // BME280_I2C


/********** DEBUG functions ***************************************************/

void print_raw_data(uint16_t serial_data_length) {
  Tb.log("\n-----------------------------------\nRaw length in byte: ");
  Tb.log_ln(String(serial_data_length));
  Tb.log_ln("Raw data: ");
  int mul = (serial_data_length/LINE_LENGTH_RAW_SERIAL);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<LINE_LENGTH_RAW_SERIAL;j++) {
      Tb.log("0x"+String(telegram[i*LINE_LENGTH_RAW_SERIAL+j],HEX)+',');      
    }
    Tb.log_ln();
  }
  for(int j=0; j<(serial_data_length%LINE_LENGTH_RAW_SERIAL);j++) {
    Tb.log("0x"+String(telegram[mul*LINE_LENGTH_RAW_SERIAL+j],HEX)+',');    
  }
  Tb.log_ln();
}

void print_vector(Vector_GCM *vect) {
  Tb.log("\n-----------------------------------\nPrint Vector: ");
  Tb.log("\nVector_Name: ");
  Tb.log(vect->name);
  Tb.log("\nKey: ");
  for(int cnt=0; cnt<16;cnt++) {
    Tb.log(String(vect->key[cnt],HEX) + ' ');
    
  }
  Tb.log("\nData (Text): ");
  int mul = (vect->datasize/LINE_LENGTH_RAW_SERIAL);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<LINE_LENGTH_RAW_SERIAL;j++) {
      Tb.log(String(vect->ciphertext[i*LINE_LENGTH_RAW_SERIAL+j],HEX) + ' ');      
    }
    Tb.log_ln();
  }
  for(int j=0; j<(serial_data_length%LINE_LENGTH_RAW_SERIAL);j++) {
    Tb.log(String(vect->ciphertext[mul*LINE_LENGTH_RAW_SERIAL+j],HEX) + ' ');    
  }
  Tb.log("\n\nAuth_Data: ");
  for(int cnt=0; cnt<17;cnt++) {
    Tb.log(String(vect->authdata[cnt],HEX) + ' ');    
  }
  Tb.log("\nInit_Vect: ");
  for(int cnt=0; cnt<12;cnt++) {
    Tb.log(String(vect->iv[cnt],HEX) + ' ');    
  }
  Tb.log("\nAuth_Tag: ");
  for(int cnt=0; cnt<12;cnt++) {
    Tb.log(String(vect->tag[cnt],HEX) + ' ');    
  }
  Tb.log("\nAuth_Data Size: ");
  Tb.log(String(vect->authsize));
  Tb.log("\nData Size: ");
  Tb.log(String(vect->datasize));
  Tb.log("\nAuth_Tag Size: ");
  Tb.log(String(vect->tagsize));
  Tb.log("\nInit_Vect Size: ");
  Tb.log(String(vect->ivsize));
  Tb.log_ln();
}
