#ifndef ESPTOOLBOX_h
#define ESPTOOLBOX_h

//#define WEBSERVER  // comment if no webserver needed

#include "Arduino.h"

#ifndef LED_BUILTIN
  #define LED_BUILTIN 13
#endif

#ifdef ESP8266
  #include <ESP8266WiFi.h>           // ESP8266
  #include <ESP8266mDNS.h>
  #ifdef WEBSERVER
    #include <ESP8266WebServer.h>
    ESP8266WebServer http_server(80);    // create a web server on port 80
  #endif // ifdef WEBSERVER
#else
  #include <WiFi.h>                  // ESP32
  #include <ESPmDNS.h>
  #ifdef WEBSERVER
    #include <WebServer.h>
    WebServer http_server(80);    // create a web server on port 80
  #endif // ifdef WEBSERVER
#endif // ifdef ESP8266


//#include <WebSocketsServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <Ethernet.h>

/*#ifdef ESP8266
  WebSocketsServer ws_server(81);      // create a ws server on port 81
#else
  WebSocketsServer ws_server(81);      // create a ws server on port 81
#endif // ifdef ESP8266*/


class ESPToolbox {
  public:
    String my_homepage;

    /****** INIT functions ****************************************************/
    void init_led(); // initialise the build in LED and switch it on
    // initialise WiFi, overloaded to add mDNS, hostname and local IP
    void init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD);
    void init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                       const char *NET_MDNSNAME);
    void init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                       const char *NET_MDNSNAME, const char *NET_HOSTNAME);
    void init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                       const char *NET_HOSTNAME, IPAddress NET_LOCAL_IP,
                       IPAddress NET_GATEWAY, IPAddress NET_MASK);
    // initialise WiFi AP
    void init_wifi_ap(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                             IPAddress IP_AP, IPAddress MASP_AP);
    #ifdef WEBSERVER
      // init a http server and handle http requests: http://192.168.168.168/
      void init_http_server();
    #endif //ifdef WEBSERVER
    //void init_ota();
    void init_ota(const char *OTA_NAME, const char *OTA_PASS_HASH);

    /****** GETTER functions **************************************************/
    bool get_led_log();           // get logger flag for LED
    bool get_serial_log();        // get logger flag for Serial
    bool get_udp_log();           // get logger flag for UDP
    bool get_led_pos_logic();     // LED uses positive logic if true
    /****** SETTER functions **************************************************/
    // set logger flag for Serial (LED_BUILTIN, positive logic)
    void set_led_log(bool flag);
    void set_led_log(bool flag, bool pos_logic); // overload to change logic
    void set_led_log(bool flag, byte led_pin); // overload to change LED pin
     // overload to change LED pin and the logic
    void set_led_log(bool flag, byte led_pin, bool pos_logic);
    void set_serial_log(bool flag); // set logger flag for Serial
    // overload to change the serial interface number
    void set_serial_log(bool flag, byte interface_number);
    // set logger flag for UDP and pass IP and port
    void set_udp_log(bool flag,IPAddress UDP_LOG_PC_IP,const word UDP_LOG_PORT);

    /****** LOGGING functions *************************************************/
    // print log line to Serial and/or remote UDP port
    void log(String message);
    void log_ln();
    void log_ln(String message);

    /****** SERVER functions **************************************************/
    #ifdef WEBSERVER
      // pass the webpage as string
      void pass_homepage(String homepage);
      // needed in root loop to wait for http requests
      void http_server_handle_client();
      // if the requested page doesn't exist, return a 404 not found error
      void handle_not_found();
      // handle root (deliver webpage)
      void handle_root();
    #endif //ifdef WEBSERVER

    /****** HELPER functions **************************************************/
    void led_on(); // build in LED on
    void led_off(); // build in LED off
    void blink_led_x_times(byte x); // blink LED_BUILTIN x times (LED was on)
     // blink LED_BUILTIN x times (LED was on)
    void blink_led_x_times(byte x, word delay_time_ms);
    bool non_blocking_delay(unsigned long milliseconds);
    byte non_blocking_delay_x3(unsigned long ms_1, unsigned long ms_2, unsigned long ms_3);

  private:
    bool enable_led_log = false;
    byte led_log_pin = LED_BUILTIN;
    bool led_pos_logic = true;
    word default_delay_time_ms = 100;
    bool enable_serial_log = false;
    byte serial_interface_number = 0;
    bool enable_udp_log = false;
    word udp_log_port = 6666;
    IPAddress udp_log_pc_ip;
    WiFiUDP Udp; // create Udp object
    const char *mdns_name = "myESP";





};

#endif // ESPTOOLBOX_h
