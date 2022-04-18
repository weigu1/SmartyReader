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
  V1.6 2021-08-12 Using config or secrets file for password, 3 new gas fields,
                  BME280 suppport, changed ESPBacker to ESPToolbox, bugfixes
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

/*!!!!!!!!!! Comment or uncomment the following lines suiting your needs !!!!!!!!!*/
//#define OLD_HARDWARE    // for the boards before V2.0
//#define MQTTPASSWORD    // if you want an MQTT connection with password (recommended!!)
//#define STATIC          // if static IP needed (no DHCP)
//#define ETHERNET        // if Ethernet with Funduino (W5100) instead of WiFi
#define OTA             // if Over The Air update needed (security risk!)
/* The file "secrets.h" has to be placed in the sketchbook libraries folder
   in a folder named "Secrets" and must contain the same things than the file config.h*/
#define USE_SECRETS
//#define BME280_I2C     // if you wanSmartyReader_v1_6_alphat to add a temp sensor to I2C connector
/* power and energy and energy per day are published as JSON string,
   Subscribe to topic/# (e.g. lamsmarty/#). For more data uncomment the following
   line, then all the data is published (0 for normal string, 1 for json).*/
//#define PUBLISH_ALL_VALUES_ONLY
#define PUBLISH_COOKED

/****** Arduino libraries needed ******/
#ifdef USE_SECRETS
  #include <secrets.h>
#else  
  #include "config.h"      // most of the things you need to change are here
#endif // USE_SECRETS  
#include "ESPToolbox.h"  // ESP helper lib (more on weigu.lu)
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Crypto.h>
#include <AES.h>
#include <GCM.h>
//#include <time.h>
#ifdef BME280_I2C
  #include <Wire.h>
  #include <BME280I2C.h>
#endif

/****** WiFi and network settings ******/
const char *WIFI_SSID = MY_WIFI_SSID;           // if no secrets file, use the config.h file
const char *WIFI_PASSWORD = MY_WIFI_PASSWORD;   // if no secrets file, use the config.h file
#ifdef OTA                                // Over The Air update settings
  const char *OTA_NAME = MY_OTA_NAME;
  const char *OTA_PASS_HASH = MY_OTA_PASS_HASH;  // use the config.h file
#endif // ifdef OTA      
#ifdef STATIC
  IPAddress NET_LOCAL_IP (NET_LOCAL_IP_BYTES);    // 3x optional for static IP
  IPAddress NET_GATEWAY (NET_GATEWAY_BYTES);      // look in config.h
  IPAddress NET_MASK (NET_MASK_BYTES);  
#endif // ifdef STATIC
IPAddress UDP_LOG_PC_IP(UDP_LOG_PC_IP_BYTES);     // UDP logging if enabled in setup() (config.h)

/****** MQTT settings ******/
#define MQTT_MAX_PACKET_SIZE 512
const short MQTT_PORT = MY_MQTT_PORT;
WiFiClient espClient;
#ifdef MQTTPASSWORD
  const char *MQTT_USER = MY_MQTT_USER;
  const char *MQTT_PASS = MY_MQTT_PASS;
#endif // MQTTPASSWORD

PubSubClient MQTT_Client(espClient);
String mqtt_msg;
DynamicJsonDocument doc_out(1024);


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

// DMSR variables
struct Dsmr_field {
  const byte nr;
  const String name;
  const String id;
  const String unit;
  String value;
  const char type;  // string s, double f, integer i
};

// https://electris.lu/files/Dokumente_und_Formulare/Netz_Tech_Dokumente/SPEC_-_E-Meter_P1_specification_20210305.pdf

Dsmr_field dsmr[] = {
  {0,"identification", "", "","NA",'s'},
  {1,"p1_version", "1-3:0.2.8", "","NA",'s'},
  {2,"timestamp", "0-0:1.0.0", "","NA",'s'},
  {3,"equipment_id", "0-0:42.0.0", "","NA",'s'},
  {4,"act_energy_imported_p_plus", "1-0:1.8.0", "kWh","NA",'f'},
  {5,"act_energy_exported_p_minus", "1-0:2.8.0", "kWh","NA",'f'},
  {6,"react_energy_imported_q_plus", "1-0:3.8.0", "kvarh","NA",'f'},
  {7,"react_energy_exported_q_minus", "1-0:4.8.0", "kvarh","NA",'f'},
  {8,"act_pwr_imported_p_plus", "1-0:1.7.0", "kW","NA",'f'},
  {9,"act_pwr_exported_p_minus", "1-0:2.7.0", "kW","NA",'f'},
  {10,"react_pwr_imported_q_plus", "1-0:3.7.0", "kvar","NA",'f'},
  {11,"react_pwr_exported_q_minus", "1-0:4.7.0", "kvar","NA",'f'},
  {12,"active_threshold_smax", "0-0:17.0.0", "kVA","NA",'f'},
  {13,"breaker_ctrl_state_0", "0-0:96.3.10", "","NA",'i'},
  {14,"num_pwr_failures", "0-0:96.7.21", "","NA",'i'},
  {15,"num_volt_sags_l1", "1-0:32.32.0", "","NA",'i'},
  {16,"num_volt_sags_l2", "1-0:52.32.0", "","NA",'i'},
  {17,"num_volt_sags_l3", "1-0:72.32.0", "","NA",'i'},
  {18,"num_volt_swells_l1", "1-0:32.36.0", "","NA",'i'},
  {19,"num_volt_swells_l2", "1-0:52.36.0", "","NA",'i'},
  {20,"num_volt_swells_l3", "1-0:72.36.0", "","NA",'i'},
  {21,"breaker_ctrl_state_0", "0-0:96.3.10", "","NA",'i'},
  {22,"msg_long_e_meter", "0-0:96.13.0", "","NA",'s'},
  {23,"msg_long_ch2", "0-0:96.13.2", "","NA",'s'},
  {24,"msg_long_ch3", "0-0:96.13.3", "","NA",'s'},
  {25,"msg_long_ch4", "0-0:96.13.4", "","NA",'s'},
  {26,"msg_long_ch5", "0-0:96.13.5", "","NA",'s'},
  {27,"curr_l1", "1-0:31.7.0", "A","NA",'f'},
  {28,"curr_l2", "1-0:51.7.0", "A","NA",'f'},
  {29,"curr_l3", "1-0:71.7.0", "A","NA",'f'},
  {30,"act_pwr_imp_p_plus_l1", "1-0:21.7.0", "kW","NA",'f'},
  {31,"act_pwr_imp_p_plus_l2", "1-0:41.7.0", "kW","NA",'f'},
  {32,"act_pwr_imp_p_plus_l3", "1-0:61.7.0", "kW","NA",'f'},
  {33,"act_pwr_exp_p_minus_l1", "1-0:22.7.0", "kW","NA",'f'},
  {34,"act_pwr_exp_p_minus_l2", "1-0:42.7.0", "kW","NA",'f'},
  {35,"act_pwr_exp_p_minus_l3", "1-0:62.7.0", "kW","NA",'f'},
  {36,"react_pwr_imp_q_plus_l1", "1-0:23.7.0", "kvar","NA",'f'},
  {37,"react_pwr_imp_q_plus_l2", "1-0:43.7.0", "kvar","NA",'f'},
  {38,"react_pwr_imp_q_plus_l3", "1-0:63.7.0", "kvar","NA",'f'},
  {39,"react_pwr_exp_q_minus_l1", "1-0:24.7.0", "kvar","NA",'f'},
  {40,"react_pwr_exp_q_minus_l2", "1-0:44.7.0", "kvar","NA",'f'},
  {41,"react_pwr_exp_q_minus_l3", "1-0:64.7.0", "kvar","NA",'f'},
  {42,"apparent_export_pwr", "1-0:10.7.0", "kVA","NA",'f'},
  {43,"apparent_import_pwr", "1-0:9.7.0", "kVA","NA",'f'},
  {44,"breaker_ctrl_state_1", "0-1:96.3.10", "","NA",'i'},
  {45,"breaker_ctrl_state_2", "0-2:96.3.10", "","NA",'i'},
  {46,"limiter_curr_monitor", "1-1:31.4.0", "A","NA",'f'},
  {47,"volt_l1", "1-0:32.7.0", "V","NA",'f'},
  {48,"volt_l2", "1-0:52.7.0", "V","NA",'f'},
  {49,"volt_l3", "1-0:72.7.0", "V","NA",'f'},
  {50,"gas_index", "0-1:24.2.1", "m3","NA",'f'},
  {51,"device_Type", "0-1:24.4.0", "", "NA",' '},
  {52,"mbus_ch_sw_pos", "0-1:24.1.0", "", "NA",' '},
  {53,"gas_meter_id_hex", "0-1:96.1.0", "", "NA",' '}, 
};

Vector_GCM Vector_SM;
GCM<AES128> *gcmaes128 = 0;

/********** SETUP *************************************************************/

void setup() {
  Tb.set_led_log(true);                 // use builtin LED for debugging
  Tb.set_serial_log(true,1);           // 2 parameter = interface (1 = Serial1)
  Tb.set_udp_log(true, UDP_LOG_PC_IP, UDP_LOG_PORT); // use "nc -kulw 0 6666"
  init_wifi();
  init_serial4smarty();
  MQTT_Client.setBufferSize(512);
  MQTT_Client.setServer(MQTT_SERVER,MQTT_PORT); //open connection to MQTT server
  mqtt_connect();
  #ifdef OTA
    Tb.init_ota(OTA_NAME, OTA_PASS_HASH);
  #endif // ifdef OTA
  memset(telegram, 0, MAX_SERIAL_STREAM_LENGTH);
  delay(2000); // give it some time
  Tb.blink_led_x_times(3);
}

/********** LOOP  **************************************************************/

void loop() {
  #ifdef OTA
    ArduinoOTA.handle();
  #endif // ifdef OTA
  read_telegram();
  if (Tb.non_blocking_delay(PUBLISH_TIME)) { // Publish every PUBLISH_TIME
    decrypt_and_publish(SAMPLES);
  }
  if (WiFi.status() != WL_CONNECTED) {  // if WiFi disconnected, reconnect
    init_wifi();
  }
  if (!MQTT_Client.connected()) { // reconnect mqtt client, if needed
    mqtt_connect();
  }
  MQTT_Client.loop();                    // make the MQTT live
  yield();
}

/********** INIT functions ****************************************************/

void init_wifi() {
  #ifdef STATIC
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_HOSTNAME, NET_LOCAL_IP,
                  NET_GATEWAY, NET_MASK);
  #else
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_MDNSNAME, NET_HOSTNAME);
  #endif // ifdef STATIC
}

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

void mqtt_publish_all(boolean json) {
  String Sub_topic, Mqtt_msg = "";
  uint8_t i;
  for (i=1; i < (sizeof dsmr / sizeof dsmr[0]); i++) {
    if (dsmr[i].value != "NA") {
      if (json) {
        if (dsmr[i].type == 'f') {
          Mqtt_msg = "{\"" + dsmr[i].name + "_value" + "\":" + dsmr[i].value.toDouble();          
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
      Sub_topic = MQTT_TOPIC_OUT + '/' + dsmr[i].name;
      MQTT_Client.publish(Sub_topic.c_str(), Mqtt_msg.c_str());
      Tb.log_ln("------------------");
      Tb.log("Published message: ");
      Tb.log_ln(Mqtt_msg);
    }
    else {
      Tb.log_ln("error when publishing message (buffer big enough?)");
    }
  }
}

/**/
void mqtt_publish_cooked(int samples) {
  double calculated_values[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  calculate_energy_and_power(calculated_values,samples);
  String Sub_topic, Mqtt_msg = "";
  doc_out[dsmr[2].name] = dsmr[2].value;
  doc_out[dsmr[3].name] = dsmr[3].value;
  doc_out["energy_consumption_kWh"] = calculated_values[0];  
  doc_out["energy_consumption_cumul_day_kWh"] = calculated_values[6];
  doc_out["energy_production_kWh"] = calculated_values[1];  
  doc_out["energy_production_cumul_day_kWh"] = calculated_values[7];
  doc_out["power_consumption_kW"] = calculated_values[2];  
  doc_out["power_production_kW"] = calculated_values[3];
  doc_out["power_consumption_calculated_kW"] = calculated_values[4];  
  doc_out["power_production_calculated_kW"] = calculated_values[5];
  doc_out["power_excess_solar_kW"] = calculated_values[8];
  doc_out["power_excess_solar_calculated_kW"] = calculated_values[9];     
  doc_out["power_consumption_calc_average_kW"] = calculated_values[10];
  doc_out["power_production_calc_average_kW"] = calculated_values[11];
  serializeJson(doc_out, Mqtt_msg);
  MQTT_Client.publish(MQTT_TOPIC_OUT.c_str(), Mqtt_msg.c_str());  
  Tb.log_ln("------------------");
  Tb.log("Published message: ");
  Tb.log_ln(Mqtt_msg);
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
/*void calculate_energy_and_power(double calculated_values[20], int samples) {  
  struct tm t;
  time_t t_of_day;
  static double energy_consumption_previous = dsmr[4].value.toDouble();
  static double energy_production_previous = dsmr[5].value.toDouble();
  static double energy_consumption_midnight = dsmr[4].value.toDouble();
  static double energy_production_midnight = dsmr[5].value.toDouble();  
  static double energy_consumption_sum = 0;
  static double energy_production_sum = 0;  
  static int counter = 0; // needed to calculate average over samples
  counter = counter + 1;
  double power_consumption_calculated = 0;
  double power_production_calculated = 0;
  double power_excess_solar = 0;  
  double power_excess_solar_calc = 0;
  double energy_consumption_cumul_day = 0;
  double energy_production_cumul_day = 0;
  double power_consumption_calc_average = 0;
  double power_production_calc_average = 0;  
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
  double energy_consumption = dsmr[4].value.toDouble();
  double energy_production = dsmr[5].value.toDouble();
  double power_consumption = dsmr[8].value.toDouble();
  double power_production = dsmr[9].value.toDouble();
  energy_consumption_sum += dsmr[4].value.toDouble();
  energy_production_sum += dsmr[5].value.toDouble();
  if (counter == samples) {
    power_consumption_calc_average = energy_consumption_sum/counter;
    power_production_calc_average = energy_consumption_sum/counter;
    counter = 0;
    energy_consumption_sum = 0;
    energy_consumption_sum = 0;    
  }  
  int delta_time = epoch_previous-epoch;
  power_consumption_calculated = (energy_consumption_previous-energy_consumption)*3600/delta_time;
  power_production_calculated = (energy_production_previous-energy_production)*3600/delta_time;
  energy_consumption_cumul_day = energy_consumption - energy_consumption_midnight;
  energy_production_cumul_day = energy_production - energy_production_midnight;
  power_excess_solar = power_production - power_consumption;
  power_excess_solar_calc = power_production_calculated - power_consumption_calculated;  
  Tb.log_ln("*****************");
  Tb.log_ln(String(power_consumption_calculated));
  Tb.log_ln(String(power_production_calculated));
  Tb.log_ln(String(delta_time));
  Tb.log_ln("*****************");
  energy_consumption_previous = energy_consumption;
  energy_production_previous = energy_production;
  epoch_previous = t_of_day;
  if ((t.tm_hour == 23) && (t.tm_min >= 55)) {
    energy_consumption_midnight = energy_consumption;
  }
  if ((t.tm_hour == 23) && (t.tm_min >= 55)) {
    energy_production_midnight = energy_production;
  }
  calculated_values[0] = energy_consumption;
  calculated_values[1] = energy_production;
  calculated_values[2] = power_consumption;
  calculated_values[3] = power_production;  
  calculated_values[4] = power_consumption_calculated;
  calculated_values[5] = power_production_calculated;
  calculated_values[6] = energy_consumption_cumul_day;
  calculated_values[7] = energy_production_cumul_day;
  calculated_values[8] = power_excess_solar;
  calculated_values[9] = power_excess_solar_calc;
  calculated_values[10] = power_consumption_calc_average;
  calculated_values[11] = power_production_calc_average;  
}*/

/* Here we calculate power from energy, the energy/day and the excess solar power */
void calculate_energy_and_power(double calculated_values[20], int samples) {  
  struct tm t;
  time_t t_of_day;
  unsigned long epoch = 0;  
  int delta_time = 0;
  double energy_consumption = 0.0;
  double energy_production = 0.0;
  double power_consumption = 0.0;
  double power_production = 0.0;
  double power_c_L1 = 0.0;
  double power_c_L2 = 0.0;
  double power_c_L3 = 0.0;
  double power_p_L1 = 0.0;
  double power_p_L2 = 0.0;
  double power_p_L3 = 0.0;
  double power_consumption_calculated = 0.0;
  double power_production_calculated = 0.0;
  double power_excess_solar = 0.0;
  double power_excess_solar_calc = 0.0;
  double energy_consumption_cumul_day = 0.0;
  double energy_production_cumul_day = 0.0;
  double power_consumption_calc_average = 0.0;
  double power_production_calc_average = 0.0;
  static unsigned long epoch_previous = 0;
  static int counter = 0; // needed to calculate average over samples
  static double energy_consumption_sum = 0.0;
  static double energy_production_sum = 0.0;  
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
    
    counter = counter + 1;    
    Serial.print(String(counter) + "\t");
    epoch = t_of_day;
    delta_time = epoch_previous-epoch;
    // get data in Wh resp. W
    energy_consumption = dsmr[4].value.toDouble()*1000.0;  
    energy_production = dsmr[5].value.toDouble()*1000.0;
    power_consumption = dsmr[8].value.toDouble()*1000.0;
    power_production = dsmr[9].value.toDouble()*1000.0;
    power_consumption_calculated = (energy_consumption_previous-energy_consumption)*3600/delta_time;
    power_production_calculated = (energy_production_previous-energy_production)*3600/delta_time;
    energy_consumption_cumul_day = energy_consumption - energy_consumption_midnight;
    energy_production_cumul_day = energy_production - energy_production_midnight;
    power_excess_solar = power_production - power_consumption;
    power_excess_solar_calc = power_production_calculated - power_consumption_calculated;  
    energy_consumption_previous = energy_consumption;
    energy_production_previous = energy_production;
    energy_consumption_sum += energy_consumption_previous-energy_consumption;
    energy_production_sum += energy_production_previous-energy_production;
    
    
    if (counter == samples) {
      power_consumption_calc_average = (energy_consumption_sum*3600)/counter/delta_time;
      power_production_calc_average = (energy_production_sum*3600)/counter/delta_time;
      counter = 0;
      energy_consumption_sum = 0;
      energy_consumption_sum = 0;    
    }  
    
    
    
      epoch_previous = t_of_day;  
      if ((t.tm_hour == 23) && (t.tm_min >= 55)) {
        energy_consumption_midnight = energy_consumption;
      }
      if ((t.tm_hour == 23) && (t.tm_min >= 55)) {
        energy_production_midnight = energy_production;
      }
    calculated_values[0] = energy_consumption/1000.0; 
    calculated_values[1] = energy_production/1000.0;
    calculated_values[2] = power_consumption;
    calculated_values[3] = power_production;  
    calculated_values[4] = power_consumption_calculated;
    calculated_values[5] = power_production_calculated;
    calculated_values[6] = energy_consumption_cumul_day;
    calculated_values[7] = energy_production_cumul_day;
    calculated_values[8] = power_excess_solar;
    calculated_values[9] = power_excess_solar_calc;
    calculated_values[10] = power_consumption_calc_average;
    calculated_values[11] = power_production_calc_average; 
    Serial.print(String(calculated_values[0]) + "\t");
    Serial.print(String(calculated_values[1]) + "\t");
    Serial.print(String(calculated_values[2]) + "\t");
    Serial.print(String(calculated_values[3]) + "\t");
    Serial.print("PC cal " + String(calculated_values[4]) + "\t");
    Serial.print("PP cal " + String(calculated_values[5]) + "\t"); 
    Serial.print("EC_cum " + String(calculated_values[6]) + "\t");
    Serial.print("EP cum " + String(calculated_values[7]) + "\t");
    Serial.print("Exc. " + String(calculated_values[8]) + "\t");
    Serial.print("Excess cal " + String(calculated_values[9]) + "\t");
    Serial.print("PC av " + String(calculated_values[10]) + "\t");
    Serial.print("PP av " + String(calculated_values[11]) + "\t");
  
    Serial.println();
    
  }  
  else {    
    epoch_previous = t_of_day;
    Serial.println(epoch_previous);
  }  
}


void decrypt_and_publish(int samples) {
  Tb.log_ln("-----------------------------------");
  Tb.log("Get data from Smarty and publish");
  print_raw_data(serial_data_length);  // for thorough debugging
  init_vector(&Vector_SM,"Vector_SM", KEY_SMARTY, AUTH_DATA);
  print_vector(&Vector_SM);            // for thorough debugging
  if (Vector_SM.datasize != MAX_SERIAL_STREAM_LENGTH) {
    decrypt_text(&Vector_SM);
    parse_dsmr_string(buffer);    
    #ifdef PUBLISH_ALL_VALUES_ONLY
      mqtt_publish_all(0); // 0 for values only
    #endif // PUBLISH_ALL_VALUES_ONLY       
    #ifdef PUBLISH_COOKED
      mqtt_publish_cooked(samples);
    #endif // PUBLISH_COOKED
    #if not defined(PUBLISH_ALL_VALUES_ONLY) and not defined(PUBLISH_COOKED)
      mqtt_publish_all(1); // 1 for json cooked
    #endif  // use json    
    Tb.blink_led_x_times(4);
  }
  else {    //max datalength reached error!
    Tb.log_ln("error, serial stream too big");
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
