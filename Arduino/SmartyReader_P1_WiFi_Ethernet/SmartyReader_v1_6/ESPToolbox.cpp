#include "Arduino.h"
#include "ESPToolbox.h"

/****** INIT functions ********************************************************/

// initialise the build in LED and switch it on
void ESPToolbox::init_led() {
  pinMode(led_log_pin,OUTPUT);
  led_on();
}

// initialise the build in LED and switch it on, set logic
void ESPToolbox::init_led(bool pos_logic) {
  led_pos_logic = pos_logic;
  pinMode(led_log_pin,OUTPUT);
  led_on();
}

// initialise any LED and switch it on
void ESPToolbox::init_led(byte led_pin) {
  pinMode(led_pin,OUTPUT);
  led_on(led_pin);
}

// initialise any LED and switch it on, set logic
void ESPToolbox::init_led(byte led_pin, bool pos_logic) {
  pinMode(led_pin,OUTPUT);
  led_on(led_pin,pos_logic);
}

void ESPToolbox::init_ntp_time() {
  #ifdef ESP8266
    configTime(TZ_INFO, NTP_SERVER);
  #else
    configTime(0, 0, NTP_SERVER); // 0, 0 because we will use TZ in the next line
    setenv("TZ", TZ_INFO, 1);     // set environment variable with your time zone
    tzset();
  #endif
}

// initialise Wlan for station
void ESPToolbox::init_wifi_sta(const char *WIFI_SSID,
                               const char *WIFI_PASSWORD) {
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
  if (enable_static_ip) {
    // DNS1 = Gateway, DNS2 = net_dns
    WiFi.config(net_local_ip, net_gateway, net_mask, net_gateway, net_dns);
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    log("WiFi connection failed! Rebooting...\n");
    delay(5000);
    ESP.restart();
  }
  log("Connected to SSID " + WiFi.SSID() + " with IP " + \
      WiFi.localIP().toString() + "\nSignal strength is " \
      + WiFi.RSSI() + " dBm\n");
  blink_led_x_times(3);              // blink 3x to show WiFi is initialised
}

// initialise Wlan for station, overloaded method to use mDNS and hostname
void ESPToolbox::init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                              const char *NET_MDNSNAME,
                              const char *NET_HOSTNAME) {
  ESPToolbox::mdns_name = NET_MDNSNAME;
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
  #ifdef ESP8266
    WiFi.hostname(NET_HOSTNAME);
  #else
    WiFi.setHostname(NET_HOSTNAME);
  #endif // ifdef ESP8266
  if (enable_static_ip) {
    // DNS1 = Gateway, DNS2 = net_dns
    WiFi.config(net_local_ip, net_gateway, net_mask, net_gateway, net_dns);
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    log("WiFi connection failed! Rebooting...\n");
    delay(5000);
    ESP.restart();
  }
  log("Connected to SSID " + WiFi.SSID() + " with IP " + \
      WiFi.localIP().toString() + "\nSignal strength is " \
      + WiFi.RSSI() + " dBm\n");
  if (!MDNS.begin(mdns_name)) {      // start the mDNS responder for name.local
    log("Error setting up MDNS responder!\n");
  }
  else {
    log("mDNS responder started: " + String(mdns_name) + ".local\n");
  }
  blink_led_x_times(3);              // blink 3x to show WiFi is initialised
}

// initialise Wlan for access point, overloaded method to use mDNS and hostname
/*void ESPToolbox::init_wifi_ap(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                             IPAddress IP_AP, IPAddress MASK_AP) {
  WiFi.mode(WIFI_AP_STA);
  delay(200);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);       // start the access point
  delay(100); // needed if Address is not 192.168.4.1! (ESP32)
  WiFi.softAPConfig(IP_AP, IP_AP, MASK_AP); // second is gateway same as ip
  IPAddress myIP = WiFi.softAPIP();
  log("Access Point " + String(WIFI_SSID) + "\nAP IP address: " + String(myIP));
  blink_led_x_times(3);              // blink 3x to show WiFi is initialised
}
*/

// initialise ethernet
void ESPToolbox::init_eth(byte *NET_MAC) {
  Ethernet.init(pin_cs);
  if (enable_static_ip) {
    // DNS1 = Gateway, DNS2 = net_dns
    Ethernet.begin(NET_MAC, net_local_ip, net_dns, net_gateway, net_mask);
  }
  else {
    Ethernet.begin(NET_MAC);
  }
  Eth_Udp.begin(ESPToolbox::udp_log_port);
  delay(200);
  WiFi.mode(WIFI_OFF);
  log_ln("Ethernet connected with IP: " + Ethernet.localIP().toString());
  randomSeed(micros());
}

/*

#ifdef WEBSERVER
  // init a http server and handle http requests: http://192.168.168.168/
  void ESPToolbox::init_http_server() {
    http_server.onNotFound(handle_not_found);
    http_server.on("/", handle_root);  // (don't forget the "/")
    http_server.begin();               // start the HTTP server
    // needed to avoid net:err
    http_server.setContentLength(ESPToolbox::my_homepage.length());
  }
#endif //ifdef WEBSERVER
*/

/*// init websocket_server server
void ESPToolbox::init_ws_server() {
  ws_server.begin();                 // start the ws server
  ws_server.onEvent(ws_server_event);// if ws message, websocket_server_event()
}            */



// initialise Over The Air update service
void ESPToolbox::init_ota(const char *OTA_NAME, const char *OTA_PASS_HASH) {
  /* Port defaults to 8266,  Hostname defaults to esp3232-[MAC],
   * No authentication by default, Password can be set with it's md5 value
   * on Ubuntu md5sum, enter, type pw, 2x Ctrl D
   * other possible commands:
   * ArduinoOTA.setPort(8266); ArduinoOTA.setPassword("admin");  */
  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.setPasswordHash(OTA_PASS_HASH);
  ArduinoOTA.onStart([&]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount
    // SPIFFS using SPIFFS.end()
    log("Start updating " + type);
  });
  ArduinoOTA.onEnd([&]() {
    log("\nEnd");
  });
  ArduinoOTA.onProgress([&](unsigned int progress, unsigned int total) {
    log("Progress");
    //Debug_Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([&](ota_error_t error) {
    log_ln("Error!!");
    //Debug_Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) log("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) log("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) log("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) log("Receive Failed");
    else if (error == OTA_END_ERROR) log("End Failed");
  });
  ArduinoOTA.begin();
}



/****** GETTER functions ******************************************************/

// get logger flag for LED
bool ESPToolbox::get_led_log() {
  return ESPToolbox::enable_led_log;
}

// get logger flag for Serial
bool ESPToolbox::get_serial_log() {
  return ESPToolbox::enable_serial_log;
}

// get interface number for Serial
byte ESPToolbox::get_serial_interface_number() {
  return ESPToolbox::serial_interface_number;
}

// get logger flag for UDP
bool ESPToolbox::get_udp_log() {
  return ESPToolbox::enable_udp_log;
}

// LED uses negative logic if true
bool ESPToolbox::get_led_pos_logic() {
  return ESPToolbox::led_pos_logic;
}

void ESPToolbox::get_time() {
      time(&now);                     // this function calls the NTP server only every hour
      localtime_r(&now, &timeinfo);   // converts epoch time to tm structure
      t.second  = timeinfo.tm_sec;
      t.minute  = timeinfo.tm_min;
      t.hour  = timeinfo.tm_hour;
      t.day  = timeinfo.tm_mday;
      t.month  = timeinfo.tm_mon + 1;    // beer (Andreas video)
      t.year  = timeinfo.tm_year + 1900; // beer
      t.weekday = timeinfo.tm_wday;
      if (t.weekday == 0) {              // beer
        t.weekday = 7;
      }
      t.yearday = timeinfo.tm_yday + 1;  // beer
      t.daylight_saving_flag  = timeinfo.tm_isdst;
      char buffer[25];
      strftime(buffer, 25, "%A", localtime(&now));
      t.name_of_day = String(buffer);
      strftime(buffer, 25, "%B", localtime(&now));
      t.name_of_month = String(buffer);
      strftime(buffer, 25, "20%y-%m-%d", localtime(&now));
      t.date = String(buffer);
      strftime(buffer, 25, "%H:%M:%S", localtime(&now));
      t.time = String(buffer);
      strftime(buffer, 25, "20%y-%m-%dT%H:%M:%S", localtime(&now));
      t.datetime = String(buffer);
    }

bool ESPToolbox::get_static_ip() {
  return ESPToolbox::enable_static_ip;
}

bool ESPToolbox::get_ethernet() {
  return ESPToolbox::enable_ethernet;
}


/****** SETTER functions ******************************************************/

// set logger flag for LED
void ESPToolbox::set_led_log(bool flag) {
  ESPToolbox::enable_led_log = flag;
  if (flag) {
    ESPToolbox::init_led();
  }
}

// set logger flag for LED overloaded to add logic
void ESPToolbox::set_led_log(bool flag, bool pos_logic) {
  ESPToolbox::enable_led_log = flag;
  ESPToolbox::led_pos_logic = pos_logic;
  if (flag) {
    ESPToolbox::init_led();
  }
}

// set logger flag for LED and change LED pin (overloaded)
void ESPToolbox::set_led_log(bool flag, byte led_pin) {
  ESPToolbox::enable_led_log = flag;
  ESPToolbox::led_log_pin = led_pin;
  if (flag) {
    ESPToolbox::init_led();
  }
}

// set logger flag for LED and change LED pin + change logic (overloaded)
void ESPToolbox::set_led_log(bool flag, byte led_pin, bool pos_logic) {
  ESPToolbox::enable_led_log = flag;
  ESPToolbox::led_log_pin = led_pin;
  ESPToolbox::led_pos_logic = pos_logic;
  if (flag) {
    ESPToolbox::init_led();
  }
}

// set logger flag for Serial
void ESPToolbox::set_serial_log(bool flag) {
  enable_serial_log = flag;
  if (flag) {
    if (serial_interface_number == 0) {
      Serial.begin(115200);
      delay(500);
      Serial.println("\n\rLogging initialised!");
    }
    if (serial_interface_number == 1) {
      Serial1.begin(115200);
      delay(500);
      Serial1.println("\n\rLogging initialised!");
    }
    #ifndef ESP8266  // Serial2 only available on ESP32!
      if (serial_interface_number == 2) {
        Serial2.begin(115200);
        delay(500);
        Serial2.println("\n\rLogging initialised!");
      }
    #endif // ifndef ESP8266
  }
}

// set serial logger flag and overload to change the serial interface number
void ESPToolbox::set_serial_log(bool flag, byte interface_number) {
  enable_serial_log = flag;
  serial_interface_number = interface_number;
  if (flag) {
    if (serial_interface_number == 0) {
      Serial.begin(115200);
      delay(500);
      Serial.println("\n\rLogging initialised!");
    }
    if (serial_interface_number == 1) {
      Serial1.begin(115200);
      delay(500);
      Serial1.println("\n\rLogging initialised!");
    }

    #ifndef ESP8266  // Serial2 only available on ESP32!
      if (serial_interface_number == 2) {
        Serial2.begin(115200);
        delay(500);
        Serial2.println("\n\rLogging initialised!");
      }
    #endif // ifndef ESP8266
  }
}

// set logger flag for UDP
void ESPToolbox::set_udp_log(bool flag, IPAddress UDP_LOG_PC_IP,
                            const word UDP_LOG_PORT) {
  ESPToolbox::enable_udp_log = flag;
  ESPToolbox::udp_log_pc_ip = UDP_LOG_PC_IP;
  ESPToolbox::udp_log_port = UDP_LOG_PORT;
}

// set flag for static IP
void ESPToolbox::set_static_ip(bool flag, IPAddress NET_LOCAL_IP,
                               IPAddress NET_GATEWAY, IPAddress NET_MASK,
                               IPAddress NET_DNS) {
  ESPToolbox::enable_static_ip = flag;
  ESPToolbox::net_local_ip = NET_LOCAL_IP;
  ESPToolbox::net_gateway = NET_GATEWAY;
  ESPToolbox::net_mask = NET_MASK;
  ESPToolbox::net_dns = NET_DNS;
}

// set flag and cs pin for Ethernet
void ESPToolbox::set_ethernet(bool flag, byte pin_cs) {
  ESPToolbox::enable_ethernet = flag;
  ESPToolbox::pin_cs = pin_cs;
}


/****** LOGGING functions *****************************************************/

// print log line to Serial and/or remote UDP port
void ESPToolbox::log(String message) {
  if (ESPToolbox::enable_serial_log) {
      if (serial_interface_number == 0) {
        Serial.print(message);
      }
      if (serial_interface_number == 1) {
        Serial1.print(message);
      }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (serial_interface_number == 2) {
          Serial2.print(message);
        }
      #endif // ifndef ESP8266
  }
  if (ESPToolbox::enable_udp_log) {
    if (enable_ethernet) {
      ESPToolbox::Eth_Udp.beginPacket(ESPToolbox::udp_log_pc_ip,
                                ESPToolbox::udp_log_port);
      ESPToolbox::Eth_Udp.write(message.c_str());
      ESPToolbox::Eth_Udp.endPacket();
      delay(2);                        // prevent output buffer overflow
    }
    else {
      ESPToolbox::Udp.beginPacket(ESPToolbox::udp_log_pc_ip,
                                ESPToolbox::udp_log_port);
      ESPToolbox::Udp.print(message);
      ESPToolbox::Udp.endPacket();
      delay(2);                        // prevent output buffer overflow
    }
  }
}

void ESPToolbox::log_ln() {
  ESPToolbox::log("\n");
}

void ESPToolbox::log_ln(String message) {
  ESPToolbox::log(message + "\n");
}


/*// print linefeed to Serial and/or remote UDP port
void ESPToolbox::log_ln() {
  if (ESPToolbox::enable_serial_log) {
      if (ESPToolbox::serial_interface_number == 0) { Serial.println(); }
      if (ESPToolbox::serial_interface_number == 1) { Serial1.println(); }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (ESPToolbox::serial_interface_number == 2) { Serial2.println(); }
      #endif // ifndef ESP8266
    //}
  }
  if (ESPToolbox::enable_udp_log) {
    ESPToolbox::Udp.beginPacket(ESPToolbox::udp_log_pc_ip,
                               ESPToolbox::udp_log_port);
    ESPToolbox::Udp.println();
    ESPToolbox::Udp.endPacket();
    delay(2);                        // prevent output buffer overflow
  }
}

// print log line with linefeed to Serial and/or remote UDP port
void ESPToolbox::log_ln(String message) {
  if (ESPToolbox::enable_serial_log) {
      if (ESPToolbox::serial_interface_number == 0) { Serial.println(message); }
      if (ESPToolbox::serial_interface_number == 1) { Serial1.println(message); }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (ESPToolbox::serial_interface_number == 2) {Serial2.println(message);}
      #endif // ifndef ESP8266
    //}
  }
  if (ESPToolbox::enable_udp_log) {
    ESPToolbox::Udp.beginPacket(ESPToolbox::udp_log_pc_ip,
                               ESPToolbox::udp_log_port);
    ESPToolbox::Udp.println(message);
    ESPToolbox::Udp.endPacket();
    delay(2);                        // prevent output buffer overflow
  }
}*/


/****** SERVER functions ******************************************************/
#ifdef WEBSERVER
void ESPToolbox::pass_homepage(String homepage) {
  ESPToolbox::my_homepage = homepage;
  log(homepage);
}

void http_server_handle_client() {
  http_server.handleClient();
}

// if the requested page doesn't exist, return a 404 not found error
void ESPToolbox::handle_not_found() {
  http_server.send(404, "text/plain", "404: Not Found");
}

// handle root (deliver webpage)
void ESPToolbox::handle_root() {
  led_on();
  http_server.send(200,"text/html",ESPToolbox::my_homepage); // HTTP respcode 200
  led_off();
}

#endif //ifdef WEBSERVER


/****** HELPER functions ******************************************************/

// build in LED on
void ESPToolbox::led_on() {
  led_on(led_log_pin, led_pos_logic);
}

// build in LED on, set logic
void ESPToolbox::led_on(bool pos_logic) {
  led_pos_logic = pos_logic;
  led_on(led_log_pin, led_pos_logic);
}

// any LED on (default logic)
void ESPToolbox::led_on(byte led_pin) {
  led_on(led_pin, led_pos_logic);
}

// any LED on, pass pos_logic parameter
void ESPToolbox::led_on(byte led_pin, bool pos_logic) {
  if (pos_logic) {
    digitalWrite(led_pin,HIGH); // LED on (positive logic)
  }
  else {
    digitalWrite(led_pin,LOW); // LED on (negative logic)
  }
}

// build in LED off
void ESPToolbox::led_off() {
  led_off(led_log_pin, led_pos_logic);
}

// build in LED off, set logic
void ESPToolbox::led_off(bool pos_logic) {
  led_pos_logic = pos_logic;
  led_on(led_log_pin, led_pos_logic);
}

// any LED off (default logic)
void ESPToolbox::led_off(byte led_pin) {
  led_on(led_pin, led_pos_logic);
}

// any LED off, pass pos_logic parameter
void ESPToolbox::led_off(byte led_pin, bool pos_logic) {
  if (pos_logic) {
    digitalWrite(led_pin,LOW); // LED off (positive logic)
  }
  else {
    digitalWrite(led_pin,HIGH); // LED off (negative logic)
  }
}


// toggle LED_BUILTIN
void ESPToolbox::led_toggle() {
  digitalRead(led_log_pin) ? digitalWrite(led_log_pin, LOW) : digitalWrite(led_log_pin, HIGH);
}

// toggle any LED
void ESPToolbox::led_toggle(byte led_pin) {
  digitalRead(led_pin) ? digitalWrite(led_pin, LOW) : digitalWrite(led_pin, HIGH);
}



// blink LED_BUILTIN x times (LED was on)
void ESPToolbox::blink_led_x_times(byte x) {
  ESPToolbox::blink_led_x_times(x, ESPToolbox::default_delay_time_ms);
}

// blink LED_BUILTIN x times (LED was on) with delay_time_ms
void ESPToolbox::blink_led_x_times(byte x, word delay_time_ms) {
  for(byte i = 0; i < x; i++) { // Blink x times
    ESPToolbox::led_off();
    delay(delay_time_ms);
    ESPToolbox::led_on();
    delay(delay_time_ms);
  }
}

/*// blink any LED x times (LED was on)
void blink_led_x_times(byte x, word delay_time_ms, byte led_pin);
void ESPToolbox::blink_led_x_times(byte x, word delay_time_ms) {
  for(byte i = 0; i < x; i++) { // Blink x times
    ESPToolbox::led_off();
    delay(delay_time_ms);
    ESPToolbox::led_on();
    delay(delay_time_ms);
  }
}
*/

// non blocking delay using millis(), returns true if time is up
bool ESPToolbox::non_blocking_delay(unsigned long milliseconds) {
  static unsigned long nb_delay_prev_time = 0;
  if(millis() >= nb_delay_prev_time + milliseconds) {
    nb_delay_prev_time += milliseconds;
    return true;
  }
  return false;
}

// non blocking delay using millis(), returns 1 or 2 if time is up
byte ESPToolbox::non_blocking_delay_x2(unsigned long ms_1, unsigned long ms_2){
  static unsigned long nb_delay_prev_time_1 = 0;
  static unsigned long nb_delay_prev_time_2 = 0;
  unsigned long millis_now = millis();
  if(millis_now >= nb_delay_prev_time_1 + ms_1) {
    nb_delay_prev_time_1 += ms_1;
    return 1;
  }
  if(millis_now >= nb_delay_prev_time_2 + ms_2) {
    nb_delay_prev_time_2 += ms_2;
    return 2;
  }
  return 0;
}

// non blocking delay using millis(), returns 1, 2 or 3 if time is up
byte ESPToolbox::non_blocking_delay_x3(unsigned long ms_1, unsigned long ms_2, unsigned long ms_3) {
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

// non blocking delay using millis(), returns 1, 2, 3 or 4 if time is up
byte ESPToolbox::non_blocking_delay_x4(unsigned long ms_1, unsigned long ms_2,
                                       unsigned long ms_3, unsigned long ms_4) {
  static unsigned long nb_delay_prev_time_1 = 0;
  static unsigned long nb_delay_prev_time_2 = 0;
  static unsigned long nb_delay_prev_time_3 = 0;
  static unsigned long nb_delay_prev_time_4 = 0;
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
    if(millis_now >= nb_delay_prev_time_4 + ms_4) {
    nb_delay_prev_time_4 += ms_4;
    return 4;
  }
  return 0;
}

void ESPToolbox::log_time_struct() {
  log_ln("\nt.second: " + String(t.second));
  log_ln("t.minute: " + String(t.minute));
  log_ln("t.hour: " + String(t.hour));
  log_ln("t.day: " + String(t.day));
  log_ln("t.month: " + String(t.month));
  log_ln("t.year: " + String(t.year));
  log_ln("t.weekday: " + String(t.weekday));
  log_ln("t.yearday: " + String(t.yearday));
  log_ln("t.daylight_saving_flag: " + String(t.daylight_saving_flag));
  log_ln("t.name_of_day: " + t.name_of_day);
  log_ln("t.name_of_month: " + t.name_of_month);
  log_ln("t.date: " + t.date);
  log_ln("t.time: " + t.time);
  log_ln("t.datetime: " + t.datetime);
}
