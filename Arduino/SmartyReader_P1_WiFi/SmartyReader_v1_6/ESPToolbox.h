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
#include <Ethernet.h>
#include <WiFiUdp.h>
#include <EthernetUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>

/*#ifdef ESP8266
  WebSocketsServer ws_server(81);      // create a ws server on port 81
#else
  WebSocketsServer ws_server(81);      // create a ws server on port 81
#endif // ifdef ESP8266*/


class ESPToolbox {
  public:
    char *NTP_SERVER = "lu.pool.ntp.org"; // NTP server
    // Time zone for Luxembourg (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
    char * TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
    time_t now = 0;
    tm timeinfo; // time structure
    struct My_Timeinfo {
      byte second;
      byte minute;
      byte hour;
      byte day;
      byte month;
      unsigned int year;
      byte weekday;
      unsigned int yearday;
      bool daylight_saving_flag;
      String name_of_day;
      String name_of_month;
      String date;
      String time;
      String datetime;
    } t;
    String my_homepage;

    ESPToolbox() {}
    ESPToolbox(char *ntp_server, char *tz_info) { // overloaded
      NTP_SERVER = ntp_server;
      TZ_INFO = tz_info;
    }

    /****** INIT functions ****************************************************/
    void init_led(); // initialise the build in LED and switch it on
    void init_led(bool pos_logic); // init. build in LED, sw. on, set logic
    void init_led(byte led_pin);   // initialise any LED and switch it on
    void init_led(byte led_pin, bool pos_logic); // init. any LED, set logic
    void init_ntp_time(); // initialise ESP to get time from NTP server
    // initialise WiFi, overloaded to add mDNS, hostname
    void init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD);
    void init_wifi_sta(const char *WIFI_SSID, const char *WIFI_PASSWORD,
                       const char *NET_MDNSNAME, const char *NET_HOSTNAME);
    // initialise WiFi AP
    void init_wifi_ap(const char *ETH_IP, const char *WIFI_PASSWORD,
                             IPAddress IP_AP, IPAddress MASP_AP);
    // initialise Ethernet DHCP
    void init_eth(byte *NET_MAC);
    // initialise OTA
    void init_ota(const char *OTA_NAME, const char *OTA_PASS_HASH);
    #ifdef WEBSERVER
      // init a http server and handle http requests: http://192.168.168.168/
      void init_http_server();
    #endif //ifdef WEBSERVER
    /****** GETTER functions **************************************************/
    bool get_led_log();           // get logger flag for LED
    bool get_serial_log();        // get logger flag for Serial
    bool get_udp_log();           // get logger flag for UDP
    bool get_led_pos_logic();     // LED uses positive logic if true
    void get_time();              // get the time now
    bool get_static_ip();         // get flag for static ip
    bool get_ethernet();          // get flag for static ip
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
    void set_static_ip(bool flag, IPAddress NET_LOCAL_IP, IPAddress NET_GATEWAY,
                       IPAddress NET_MASK, IPAddress NET_DNS);
    void set_ethernet(bool flag);

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
    void led_on(bool pos_logic); // build in LED on, set logic
    void led_on(byte led_pin); // any LED on (default logic)
    void led_on(byte led_pin, bool pos_logic); // any LED on, any logic
    void led_off(); // build in LED off
    void led_off(bool pos_logic); // build in LED off, set logic
    void led_off(byte led_pin); // any LED off (default logic)
    void led_off(byte led_pin, bool pos_logic); // any LED off, any logic
    void led_toggle(); // toggle LED_BUILTIN
    void led_toggle(byte led_pin); // toggle any LED
    void blink_led_x_times(byte x); // blink LED_BUILTIN x times (LED was on, default time)
    // blink LED_BUILTIN x times (LED was on)
    void blink_led_x_times(byte x, word delay_time_ms);
    // blink any LED x times (LED was on)
    void blink_led_x_times(byte x, word delay_time_ms, byte led_pin);
    // non blocking delay using millis(), returns true if time is up
    bool non_blocking_delay(unsigned long milliseconds);
    // non blocking delay using millis(), returns 1 or 2 if time is up
    byte non_blocking_delay_x2(unsigned long ms_1, unsigned long ms_2);
    // non blocking delay using millis(), returns 1, 2 or 3 if time is up
    byte non_blocking_delay_x3(unsigned long ms_1, unsigned long ms_2, unsigned long ms_3);
    void log_time_struct(); // log the time structure to serial or UDP

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
    WiFiUDP Udp;         // create Udp object
    EthernetUDP Eth_Udp;    // create Udp for Ethernet
    const char *mdns_name = "myESP";
    bool enable_static_ip = false;
    bool enable_ethernet = false;
    IPAddress net_local_ip;
    IPAddress net_gateway;
    IPAddress net_mask;
    IPAddress net_dns;






};

#endif // ESPTOOLBOX_h
