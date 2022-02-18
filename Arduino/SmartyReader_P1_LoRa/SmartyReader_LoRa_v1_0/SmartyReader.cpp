#include "Arduino.h"
#include "SmartyReader.h"


 
 /********** INIT functions ****************************************************/

// initialise the build in LED and switch it on
void SmartyReader::init_led() {
  pinMode(SmartyReader::led_log_pin,OUTPUT);
  led_on();
}

// open serial connection to smartmeter on Serial0
void SmartyReader::init_serial4smarty() {
  #ifdef ESP8266   // true inverts signal
    Serial.begin(115200, SERIAL_8N1, SERIAL_FULL, 1, true);
  #else // new hardware 
    Serial.begin(115200,SERIAL_8N1, 1, 3, true); // change reversed pins of ESP32
  #endif
  Serial.setRxBufferSize(MAX_SERIAL_STREAM_LENGTH);
}

/****** GETTER functions ******************************************************/

// get logger flag for LED
bool SmartyReader::get_led_log() { return SmartyReader::enable_led_log; }

// get logger flag for Serial
bool SmartyReader::get_serial_log() { return SmartyReader::enable_serial_log; }

// LED uses negative logic if true
bool SmartyReader::get_led_pos_logic() { return SmartyReader::led_pos_logic; }

/****** SETTER functions ******************************************************/

// set logger flag for LED
void SmartyReader::set_led_log(bool flag) {
  SmartyReader::enable_led_log = flag;
  if (flag) { SmartyReader::init_led(); }
}

// set logger flag for LED overloaded to add logic
void SmartyReader::set_led_log(bool flag, bool pos_logic) {
  SmartyReader::enable_led_log = flag;
  SmartyReader::led_pos_logic = pos_logic;
  if (flag) { SmartyReader::init_led(); }
}

// set logger flag for LED and change LED pin (overloaded)
void SmartyReader::set_led_log(bool flag, byte led_pin) {
  SmartyReader::enable_led_log = flag;
  SmartyReader::led_log_pin = led_pin;
  if (flag) { SmartyReader::init_led(); }
}

// set logger flag for LED and change LED pin + change logic (overloaded)
void SmartyReader::set_led_log(bool flag, byte led_pin, bool pos_logic) {
  SmartyReader::enable_led_log = flag;
  SmartyReader::led_log_pin = led_pin;
  SmartyReader::led_pos_logic = pos_logic;
  if (flag) { SmartyReader::init_led(); }
}

// set logger flag for Serial
void SmartyReader::set_serial_log(bool flag) {
  SmartyReader::enable_serial_log = flag;
  if (flag) {
    if (SmartyReader::serial_interface_number == 1) { Serial1.begin(115200); }
    #ifndef ESP8266  //Serial2 only available on ESP32!
      if (SmartyReader::serial_interface_number == 2) { Serial2.begin(115200); }
    #endif // ifndef ESP8266
    delay(500);
    if (SmartyReader::serial_interface_number == 1) { Serial1.println("\n\rLogging initialised!"); }
    #ifndef ESP8266  //Serial2 only available on ESP32!
      if (SmartyReader::serial_interface_number == 2) { Serial2.println("\n\rLogging initialised!");}
    #endif // ifndef ESP8266
  }
}

// set serial logger flag and overload to change the serial interface number
void SmartyReader::set_serial_log(bool flag, byte interface_number) {
  SmartyReader::enable_serial_log = flag;
  SmartyReader::serial_interface_number = interface_number;
  if (flag) {    
    if (interface_number == 1) { Serial1.begin(115200); }
    #ifndef ESP8266  //Serial2 only available on ESP32!
      if (interface_number == 2) { Serial2.begin(115200); }
    #endif // ifndef ESP8266
    delay(500);    
    if (interface_number == 1) { Serial1.println("\n\rLogging initialised!"); }
    #ifndef ESP8266  //Serial2 only available on ESP32!
      if (interface_number == 2) { Serial2.println("\n\rLogging initialised!");}
    #endif // ifndef ESP8266
  }
}


/********** SMARTY functions **************************************************/

void SmartyReader::read_telegram() {
  uint16_t serial_cnt = 0;
  if (Serial.available()) {
    delay(500);  // wait for the whole stream
  }  
  while ((Serial.available()) && (serial_cnt < MAX_SERIAL_STREAM_LENGTH)) {
    SmartyReader::telegram[serial_cnt] = Serial.read();
    if (SmartyReader::telegram[0] != 0xDB) {
      while (Serial.available() > 0) {Serial.read();} //clear the buffer!    
      break;
    }
    serial_cnt++;
  }
  if (serial_cnt > 500) {
    SmartyReader::serial_data_length = serial_cnt;
  }
}

void SmartyReader::decrypt() {
  log("-----------------------------------");
  log("Get data from Smarty and publish\n");
  print_raw_data(SmartyReader::serial_data_length);  // for thorough debugging
  init_vector(&Vector_SM,"Vector_SM", KEY_SMARTY, AUTH_DATA);
  print_vector(&Vector_SM);            // for thorough debugging
  if (Vector_SM.datasize != MAX_SERIAL_STREAM_LENGTH) {
    decrypt_text(&Vector_SM);
    parse_dsmr_string(buffer);
  }
  else {    //max datalength reached error!
    log("Error!, serial stream too big");    
  }
}

String SmartyReader::lora_cook_message() {
  log_ln("----\nCooking LoRa message:");
  log_ln(dsmr[4].value + ' ' + dsmr[5].value);
  float energy_consumption = dsmr[4].value.toFloat();
  float energy_production = dsmr[5].value.toFloat();
  String lora_message = String(energy_consumption) + ';' + String(energy_production);
  log_ln("Lora message: " + lora_message);
  return lora_message;
}

/*void SmartyReader::mqtt_publish_message(boolean json, boolean all, boolean minimum) {
  String Sub_topic, msg = "";
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
        Mqtt_msg = dsmr[i].name + " = " + dsmr[i].value + " " + dsmr[i].unit;
      }
      Sub_topic = MQTT_TOPIC_OUT + '/' + dsmr[i].name;
      
      Tb.log_ln("------------------");
      Tb.log("Published message: ");
      Tb.log_ln(Mqtt_msg);
    }
  }
}*/
    
void SmartyReader::init_vector(Vector_GCM *vect, const char *Vect_name,
                               const uint8_t *key_SM, const uint8_t *auth_data) {
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

void SmartyReader::decrypt_text(Vector_GCM *vect) {
  memset(buffer, 0, MAX_SERIAL_STREAM_LENGTH-30);
  gcmaes128 = new GCM<AES128>();
  gcmaes128->setKey(vect->key, gcmaes128->keySize());
  gcmaes128->setIV(vect->iv, vect->ivsize);
  gcmaes128->decrypt((uint8_t*)buffer, vect->ciphertext, vect->datasize);
  delete gcmaes128;
}

void SmartyReader::parse_dsmr_string(String Dmsr) {
  int field_index, uint8_t_cnt, i, j;
  String Tmp;
  log_ln("+++++++++++++++++++++++++++++++++++++++");
  log_ln("DMSR:");
  log_ln(Dmsr);
  // get the identification
  uint8_t_cnt = Dmsr.indexOf('\n');
  dsmr[0].value = Dmsr.substring(0,uint8_t_cnt-1);
  log_ln("+++++++++++++++++++++++++++++++++++++++");
  log_ln(dsmr[0].value);
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
      log_ln(dsmr[i].value);
    }
  }
}

/********** DEBUG functions ***************************************************/

void SmartyReader::print_raw_data(uint16_t serial_data_length) {
  log_ln("\n------------------------------------------------------------------");  
  log_ln("Raw length in byte: " + String(serial_data_length));
  log_ln("Raw data: ");
  int mul = (serial_data_length/LINE_LENGTH_RAW_SERIAL);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<LINE_LENGTH_RAW_SERIAL;j++) {
      if (telegram[i*LINE_LENGTH_RAW_SERIAL+j] > 15) {
        log("0x"+String(telegram[i*LINE_LENGTH_RAW_SERIAL+j],HEX)+',');        
      }
      else {
        log("0x0"+String(telegram[i*LINE_LENGTH_RAW_SERIAL+j],HEX)+',');        
      }    
    }  
    log_ln();
  }
  for(int j=0; j<(serial_data_length%LINE_LENGTH_RAW_SERIAL);j++) {
    if (telegram[mul*LINE_LENGTH_RAW_SERIAL+j] > 15) {
      log("0x"+String(telegram[mul*LINE_LENGTH_RAW_SERIAL+j],HEX)+',');
    }
      else {
      log("0x0"+String(telegram[mul*LINE_LENGTH_RAW_SERIAL+j],HEX)+',');    
    }
  }
  log_ln();
}

void SmartyReader::print_vector(Vector_GCM *vect) {
  log("\n-----------------------------------\nPrint Vector: ");
  log("\nVector_Name: ");
  log(vect->name);
  log("\nKey: ");
  for(int cnt=0; cnt<16;cnt++) {
    log(String(vect->key[cnt],HEX) + ' ');    
  }
  log("\nData (Text): ");
  int mul = (vect->datasize/LINE_LENGTH_RAW_SERIAL);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<LINE_LENGTH_RAW_SERIAL;j++) {
      log(String(vect->ciphertext[i*LINE_LENGTH_RAW_SERIAL+j],HEX) + ' ');      
    }
    log_ln();
  }  
  for(int j=0; j<(serial_data_length%LINE_LENGTH_RAW_SERIAL);j++) {
    log(String(vect->ciphertext[mul*LINE_LENGTH_RAW_SERIAL+j],HEX) + ' ');    
  }
  log("\n\nAuth_Data: ");
  for(int cnt=0; cnt<17;cnt++) {
    log(String(vect->authdata[cnt],HEX) + ' ');    
  }
  log("\nInit_Vect: ");
  for(int cnt=0; cnt<12;cnt++) {
    log(String(vect->iv[cnt],HEX) + ' ');    
  }
  log("\nAuth_Tag: ");
  for(int cnt=0; cnt<12;cnt++) {
    log(String(vect->tag[cnt],HEX) + ' ');    
  }
  log("\nAuth_Data Size: ");
  log(String(vect->authsize));
  log("\nData Size: ");
  log(String(vect->datasize));
  log("\nAuth_Tag Size: ");
  log(String(vect->tagsize));
  log("\nInit_Vect Size: ");
  log(String(vect->ivsize));
  log_ln();
}


/****** LOGGING functions *****************************************************/

// print log line to Serial
void SmartyReader::log(String message) {
  if (SmartyReader::enable_serial_log) {
    if (SmartyReader::enable_serial_log) {
      /* for line feed add '\n' to your message */
      if (SmartyReader::serial_interface_number == 1) { Serial1.print(message); }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (SmartyReader::serial_interface_number == 2) { Serial2.print(message); }
      #endif // ifndef ESP8266
    }
  }
}

// print linefeed to Serial
void SmartyReader::log_ln() {
  if (SmartyReader::enable_serial_log) {
    if (SmartyReader::enable_serial_log) {
      /* for line feed add '\n' to your message */      
      if (SmartyReader::serial_interface_number == 1) { Serial1.println(); }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (SmartyReader::serial_interface_number == 2) { Serial2.println(); }
      #endif // ifndef ESP8266
    }
  }  
}

// print log line with linefeed to Serial
void SmartyReader::log_ln(String message) {
  if (SmartyReader::enable_serial_log) {
    if (SmartyReader::enable_serial_log) {
      /* for line feed add '\n' to your message */      
      if (SmartyReader::serial_interface_number == 1) { Serial1.println(message); }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (SmartyReader::serial_interface_number == 2) {Serial2.println(message);}
      #endif // ifndef ESP8266
    }
  }
}

/****** HELPER functions ******************************************************/

// build in LED on
void SmartyReader::led_on() {
  if (led_pos_logic) {
    digitalWrite(SmartyReader::led_log_pin,HIGH); // LED on (positive logic)
  }
  else {
    digitalWrite(SmartyReader::led_log_pin,LOW); // LED on (negative logic)
  }
}

// build in LED off
void SmartyReader::led_off() {
  if (led_pos_logic) {
    digitalWrite(SmartyReader::led_log_pin,LOW); // LED off (positive logic)
  }
  else {
    digitalWrite(SmartyReader::led_log_pin,HIGH); // LED off (negative logic)
  }
}

// blink LED_BUILTIN x times (LED was on)
void SmartyReader::blink_led_x_times(byte x) {
  SmartyReader::blink_led_x_times(x, SmartyReader::default_delay_time_ms);
}

// blink LED_BUILTIN x times (LED was on) with delay_time_ms
void SmartyReader::blink_led_x_times(byte x, word delay_time_ms) {
  for(byte i = 0; i < x; i++) { // Blink x times
    SmartyReader::led_off();
    delay(delay_time_ms);
    SmartyReader::led_on();
    delay(delay_time_ms);
  }
}

// non blocking delay using millis(), returns true if time is up
bool SmartyReader::non_blocking_delay(unsigned long milliseconds) {
  static unsigned long nb_delay_prev_time = 0;
  if(millis() >= nb_delay_prev_time + milliseconds) {
    nb_delay_prev_time += milliseconds;
    return true;
  }
  return false;
}
      
// non blocking delay using millis(), returns 1, 2 or 3 if time is up    
byte SmartyReader::non_blocking_delay_x3(unsigned long ms_1, unsigned long ms_2, unsigned long ms_3) {
  static unsigned long nb_delay_prev_time_1 = 0;
  static unsigned long nb_delay_prev_time_2 = 0;
  static unsigned long nb_delay_prev_time_3 = 0;
  unsigned long millis_now = millis();
  if(millis_now >= nb_delay_prev_time_1 + ms_1) {
    nb_delay_prev_time_1 += ms_1;
    return 1;
  }
  if(millis_now >= nb_delay_prev_time_2 + ms_2) {
    nb_delay_prev_time_2 += ms_2;
    return 2;
  }
  if(millis_now >= nb_delay_prev_time_3 + ms_3) {
    nb_delay_prev_time_3 += ms_3;
    return 3;
  }
  return 0;
}
