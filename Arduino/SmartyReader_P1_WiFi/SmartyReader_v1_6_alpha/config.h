
/*!!!!!! Things you must change: !!!!!!*/

/****** WiFi SSID and PASSWORD ******/
const char *MY_WIFI_SSID = "your_ssid";
const char *MY_WIFI_PASSWORD = "your_password";

/****** SmartyReader Key ******/
// Key for SAGxxx (16 byte!)
uint8_t KEY_SMARTY[] = {0xAE, 0xBD, 0x21, 0xB7, 0x69, 0xA6, 0xD1, 0x3C,
                        0x0D, 0xF0, 0x64, 0xE3, 0x83, 0x68, 0x2E, 0xFF};

/****** MQTT settings ******/
const char *MQTT_SERVER = "192.168.178.222";


/*+++++++ Things you can change: +++++++*/

/****** Publishes every in milliseconds ******/
const long PUBLISH_TIME = 10000;

const int SAMPLES = 60; //in the cooked version you can choose to get an average the power
                        // over a certain time (PUBLISH_TIME*SAMPLES, here 10 minutes)

/****** MQTT settings ******/
const char *MQTT_CLIENT_ID = "SmartyReader_LAM_1"; // this must be unique!!!
String MQTT_TOPIC_OUT = "lamsmarty";                        
const short MY_MQTT_PORT = 1883; // or 8883
// only if you use MQTTPASSWORD (uncomment //#define MQTTPASSWORD in ino file)
const char *MY_MQTT_USER = "me";
const char *MY_MQTT_PASS = "meagain";

/****** WiFi and network settings ******/
const char *NET_MDNSNAME = "smartyReaderLAM";      // optional (access with SmartyReaderLAM.local)
const char *NET_HOSTNAME = "smartyReaderLAM";      // optional
const word UDP_LOG_PORT = 6666;                    // UDP logging settings if enabled in setup()
const byte UDP_LOG_PC_IP_BYTES[4] = {192, 168, 1, 50};
const char *NTP_SERVER = "lu.pool.ntp.org"; // NTP settings
// your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
const char *TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";

// only if you use OTA (uncomment //#define OTA in ino file)
const char *MY_OTA_NAME = "smartyReaderLAM";                       // optional (access with SmartyReaderLAM.local)
const char *MY_OTA_PASS_HASH = "d890e11b5e6a9fd74221ae65b1d39a3c"; //Hash for password: LetMeOTA2?

// only if you use a static address (uncomment //#define STATIC in ino file)
const byte NET_LOCAL_IP_BYTES[4] = {192, 168, 178, 155};
const byte NET_GATEWAY_BYTES[4] = {192, 168, 178, 1};
const byte NET_MASK_BYTES[4] = {255,255,255,0};  

// only if you use ethernet (uncomment //#define ETHERNET in ino file)
const byte NET_MAC[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEG }; //only for ethernet

// Auth data: hard coded in Lux (no need to change it!) but not in Austria :)  (17 byte!)
uint8_t AUTH_DATA[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                       0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
                       0xFF};
