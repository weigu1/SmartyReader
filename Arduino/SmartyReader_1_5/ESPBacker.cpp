#include "Arduino.h"
#include "ESPBacker.h"

/****** INIT functions ********************************************************/

// initialise the build in LED and switch it on
void ESPBacker::init_led() {
  pinMode(ESPBacker::led_log_pin,OUTPUT);
  led_on();
}

// initialise Wlan for station
void ESPBacker::init_wifi_sta(const char *WIFI_SSID,
                              const char *WIFI_PASSWORD) {
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    if (enable_serial_log) {
      log("Connection Failed! Rebooting...\n");
    }
    delay(5000);
    ESP.restart();
  }
  log("Connected to SSID " + WiFi.SSID() + " with IP " + \
      WiFi.localIP().toString() + "\nSignal strength is " \
      + WiFi.RSSI() + " dBm\n");
  blink_led_x_times(3);              // blink 3x to show WiFi is initialised
}

// initialise Wlan for station, overloaded method to use mDNS
void ESPBacker::init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                              const char *NET_MDNSNAME) {
  ESPBacker::mdns_name = NET_MDNSNAME;
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    if (enable_serial_log) {
      log("Connection Failed! Rebooting...\n");
    }
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

// initialise Wlan for station, overloaded method to use mDNS and hostname
void ESPBacker::init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                              const char *NET_MDNSNAME,
                              const char *NET_HOSTNAME) {
  ESPBacker::mdns_name = NET_MDNSNAME;
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
  #ifdef ESP8266
    WiFi.hostname(NET_HOSTNAME);
  #else
    WiFi.setHostname(NET_HOSTNAME);
  #endif // ifdef ESP8266
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    if (enable_serial_log) {
      log("Connection Failed! Rebooting...\n");
    }
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

// initialise Wlan for station, overloaded method to use mDNS and hostname and
// fix local IP
void ESPBacker::init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                              const char *NET_HOSTNAME, IPAddress NET_LOCAL_IP,
                              IPAddress NET_GATEWAY, IPAddress NET_MASK) {
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
  #ifdef ESP8266
    WiFi.hostname(NET_HOSTNAME);
  #else
    WiFi.setHostname(NET_HOSTNAME);
  #endif // ifdef ESP8266
  WiFi.config(NET_LOCAL_IP, NET_GATEWAY, NET_MASK);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    if (enable_serial_log) {
      log("Connection Failed! Rebooting...\n");
    }
    delay(5000);
    ESP.restart();
  }
  log("Connected to SSID " + WiFi.SSID() + " with IP " +
      WiFi.localIP().toString() + "\nSignal strength is " +
      WiFi.RSSI() + " dBm\n");
  blink_led_x_times(3);              // blink 3x to show WiFi is initialised
}

// initialise Wlan for access point, overloaded method to use mDNS and hostname
/*void ESPBacker::init_wifi_ap(const char *WIFI_SSID, const char *WIFI_PASSWORD,
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

/*// initialise ethernet
void init_eth() {
  #ifdef STATIC
    Ethernet.begin(NET_MAC,NET_LOCAL_IP);
  #else
    Ethernet.begin(NET_MAC);
  #endif // STATIC
  delay(200);
  WiFi.mode(WIFI_OFF);
  #ifdef ESP32MK
    WiFi.setHostname(hostname);
  #else
    WiFi.hostname(hostname);
  #endif // ifdef ESP32MK
  #ifdef DEBUG
    Debug_Serial.println("");
    Debug_Serial.println("Ethernet connected");
    Debug_Serial.println("IP address: ");
    Debug_Serial.println(Ethernet.localIP());
  #endif //DEBUG
  randomSeed(micros());
}
*/

#ifdef WEBSERVER
  // init a http server and handle http requests: http://192.168.168.168/
  void ESPBacker::init_http_server() {
    http_server.onNotFound(handle_not_found);
    http_server.on("/", handle_root);  // (don't forget the "/")
    http_server.begin();               // start the HTTP server
    // needed to avoid net:err
    http_server.setContentLength(ESPBacker::my_homepage.length());
  }
#endif //ifdef WEBSERVER


/*// init websocket_server server
void ESPBacker::init_ws_server() {
  ws_server.begin();                 // start the ws server
  ws_server.onEvent(ws_server_event);// if ws message, websocket_server_event()
}            */



// initialise Over The Air update service
void ESPBacker::init_ota(const char *OTA_NAME, const char *OTA_PASS_HASH) {
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
bool ESPBacker::get_led_log() { return ESPBacker::enable_led_log; }

// get logger flag for Serial
bool ESPBacker::get_serial_log() { return ESPBacker::enable_serial_log; }

// get logger flag for UDP
bool ESPBacker::get_udp_log() { return ESPBacker::enable_udp_log; }

// LED uses negative logic if true
bool ESPBacker::get_led_pos_logic() { return ESPBacker::led_pos_logic; }

/****** SET LOGGING functions *************************************************/

// set logger flag for LED
void ESPBacker::set_led_log(bool flag) {
  ESPBacker::enable_led_log = flag;
  if (flag) { ESPBacker::init_led(); }
}

// set logger flag for LED overloaded to add logic
void ESPBacker::set_led_log(bool flag, bool pos_logic) {
  ESPBacker::enable_led_log = flag;
  ESPBacker::led_pos_logic = pos_logic;
  if (flag) { ESPBacker::init_led(); }
}

// set logger flag for LED and change LED pin (overloaded)
void ESPBacker::set_led_log(bool flag, byte led_pin) {
  ESPBacker::enable_led_log = flag;
  ESPBacker::led_log_pin = led_pin;
  if (flag) { ESPBacker::init_led(); }
}

// set logger flag for LED and change LED pin + change logic (overloaded)
void ESPBacker::set_led_log(bool flag, byte led_pin, bool pos_logic) {
  ESPBacker::enable_led_log = flag;
  ESPBacker::led_log_pin = led_pin;
  ESPBacker::led_pos_logic = pos_logic;
  if (flag) { ESPBacker::init_led(); }
}

// set logger flag for Serial
void ESPBacker::set_serial_log(bool flag) {
  ESPBacker::enable_serial_log = flag;
  if (flag) {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\rLogging initialised!");
  }
}

// set serial logger flag and overload to change the serial interface number
void ESPBacker::set_serial_log(bool flag, byte interface_number) {
  ESPBacker::enable_serial_log = flag;
  ESPBacker::serial_interface_number = interface_number;
  if (flag) {
    if (interface_number == 0) { Serial.begin(115200); }
    if (interface_number == 1) { Serial1.begin(115200); }
    #ifndef ESP8266  //Serial2 only available on ESP32!
      if (interface_number == 2) { Serial2.begin(115200); }
    #endif // ifndef ESP8266
    delay(500);
    if (interface_number == 0) { Serial.println("\n\rLogging initialised!"); }
    if (interface_number == 1) { Serial1.println("\n\rLogging initialised!"); }
    #ifndef ESP8266  //Serial2 only available on ESP32!
      if (interface_number == 2) { Serial2.println("\n\rLogging initialised!");}
    #endif // ifndef ESP8266
  }
}

// set logger flag for UDP
void ESPBacker::set_udp_log(bool flag, IPAddress UDP_LOG_PC_IP,
                            const word UDP_LOG_PORT) {
  ESPBacker::enable_udp_log = flag;
  ESPBacker::udp_log_pc_ip = UDP_LOG_PC_IP;
  ESPBacker::udp_log_port = UDP_LOG_PORT;
}


/****** LOGGING functions *****************************************************/

// print log line to Serial and/or remote UDP port
void ESPBacker::log(String message) {
  if (ESPBacker::enable_serial_log) {
    if (ESPBacker::enable_serial_log) {
      /* for line feed add '\n' to your message */
      if (ESPBacker::serial_interface_number == 0) { Serial.print(message); }
      if (ESPBacker::serial_interface_number == 1) { Serial1.print(message); }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (ESPBacker::serial_interface_number == 2) { Serial2.print(message); }
      #endif // ifndef ESP8266
    }
  }
  if (ESPBacker::enable_udp_log) {
    ESPBacker::Udp.beginPacket(ESPBacker::udp_log_pc_ip,
                               ESPBacker::udp_log_port);
    ESPBacker::Udp.print(message);
    ESPBacker::Udp.endPacket();
    delay(2);                        // prevent output buffer overflow
  }
}

// print linefeed to Serial and/or remote UDP port
void ESPBacker::log_ln() {
  if (ESPBacker::enable_serial_log) {
    if (ESPBacker::enable_serial_log) {
      /* for line feed add '\n' to your message */
      if (ESPBacker::serial_interface_number == 0) { Serial.println(); }
      if (ESPBacker::serial_interface_number == 1) { Serial1.println(); }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (ESPBacker::serial_interface_number == 2) { Serial2.println(); }
      #endif // ifndef ESP8266
    }
  }
  if (ESPBacker::enable_udp_log) {
    ESPBacker::Udp.beginPacket(ESPBacker::udp_log_pc_ip,
                               ESPBacker::udp_log_port);
    ESPBacker::Udp.println();
    ESPBacker::Udp.endPacket();
    delay(2);                        // prevent output buffer overflow
  }
}

// print log line with linefeed to Serial and/or remote UDP port
void ESPBacker::log_ln(String message) {
  if (ESPBacker::enable_serial_log) {
    if (ESPBacker::enable_serial_log) {
      /* for line feed add '\n' to your message */
      if (ESPBacker::serial_interface_number == 0) { Serial.println(message); }
      if (ESPBacker::serial_interface_number == 1) { Serial1.println(message); }
      #ifndef ESP8266  //Serial2 only available on ESP32!
        if (ESPBacker::serial_interface_number == 2) {Serial2.println(message);}
      #endif // ifndef ESP8266
    }
  }
  if (ESPBacker::enable_udp_log) {
    ESPBacker::Udp.beginPacket(ESPBacker::udp_log_pc_ip,
                               ESPBacker::udp_log_port);
    ESPBacker::Udp.println(message);
    ESPBacker::Udp.endPacket();
    delay(2);                        // prevent output buffer overflow
  }
}


/****** SERVER functions ******************************************************/
#ifdef WEBSERVER
void ESPBacker::pass_homepage(String homepage) {
  ESPBacker::my_homepage = homepage;
  log(homepage);
}

void http_server_handle_client() {
  http_server.handleClient();
}

// if the requested page doesn't exist, return a 404 not found error
void ESPBacker::handle_not_found() {
  http_server.send(404, "text/plain", "404: Not Found");
}

// handle root (deliver webpage)
void ESPBacker::handle_root() {
  led_on();
  http_server.send(200,"text/html",ESPBacker::my_homepage); // HTTP respcode 200
  led_off();
}

#endif //ifdef WEBSERVER


/****** HELPER functions ******************************************************/

// build in LED on
void ESPBacker::led_on() {
  if (led_pos_logic) {
    digitalWrite(ESPBacker::led_log_pin,HIGH); // LED on (positive logic)
  }
  else {
    digitalWrite(ESPBacker::led_log_pin,LOW); // LED on (negative logic)
  }
}

// build in LED off
void ESPBacker::led_off() {
  if (led_pos_logic) {
    digitalWrite(ESPBacker::led_log_pin,LOW); // LED off (positive logic)
  }
  else {
    digitalWrite(ESPBacker::led_log_pin,HIGH); // LED off (negative logic)
  }
}

// blink LED_BUILTIN x times (LED was on)
void ESPBacker::blink_led_x_times(byte x) {
  ESPBacker::blink_led_x_times(x, ESPBacker::default_delay_time_ms);
}

// blink LED_BUILTIN x times (LED was on) with delay_time_ms
void ESPBacker::blink_led_x_times(byte x, word delay_time_ms) {
  for(byte i = 0; i < x; i++) { // Blink x times
    ESPBacker::led_off();
    delay(delay_time_ms);
    ESPBacker::led_on();
    delay(delay_time_ms);
  }
}

// non blocking delay using millis(), returns true if time is up
bool ESPBacker::non_blocking_delay(unsigned long milliseconds) {
  static unsigned long nb_delay_prev_time = 0;
  if(millis() >= nb_delay_prev_time + milliseconds) {
    nb_delay_prev_time += milliseconds;
    return true;
  }
  return false;
}
      
// non blocking delay using millis(), returns 1, 2 or 3 if time is up    
byte ESPBacker::non_blocking_delay_x3(unsigned long ms_1, unsigned long ms_2, unsigned long ms_3) {
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
