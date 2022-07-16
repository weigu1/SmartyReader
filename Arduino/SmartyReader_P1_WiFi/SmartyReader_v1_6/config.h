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
const long PUBLISH_TIME = 60000;
/****** Time used to calculate the mean, min and max values in minutes ******/
const int SAMPLE_TIME_MIN = 10;
                                                    
/****** MQTT settings ******/
const int MQTT_MAXIMUM_PACKET_SIZE = 2560; // look in setup()
const char *MQTT_CLIENT_ID = "SmartyReader_LAM_1"; // this must be unique!!!
String MQTT_TOPIC_OUT = "lamsmarty";                        
const short MY_MQTT_PORT = 1883; // or 8883
// only if you use MQTTPASSWORD (uncomment //#define MQTTPASSWORD in ino file)
const char *MY_MQTT_USER = "me";
const char *MY_MQTT_PASS = "meagain";

/****** WiFi and network settings ******/
const char *NET_MDNSNAME = "smartyReader";      // optional (access with SmartyReaderLAM.local)
const char *NET_HOSTNAME = "smartyReader";      // optional
const word UDP_LOG_PORT = 6666;                    // UDP logging settings if enabled in setup()
const byte UDP_LOG_PC_IP_BYTES[4] = {192, 168, 1, 50};
const char *NTP_SERVER = "lu.pool.ntp.org"; // NTP settings
// your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
const char *TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";

// only if you use OTA (uncomment //#define OTA in ino file)
const char *MY_OTA_NAME = "SmartyReader"; // optional (access with SmartyReader.local)
const char *MY_OTA_PASS_HASH = "myhash";  // hash for password

// only if you use a static address (uncomment //#define STATIC in ino file)
const byte NET_LOCAL_IP_BYTES[4] = {192, 168, 178, 155};
const byte NET_GATEWAY_BYTES[4] = {192, 168, 178, 1};
const byte NET_MASK_BYTES[4] = {255,255,255,0};  
const byte NET_DNS_BYTES[4] = {8,8,8,8}; //  second dns (first = gateway), 8.8.8.8 = google

// only if you use ethernet (uncomment //#define ETHERNET in ino file)
byte NET_MAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01};  // for ethernet (e.g. Funduino board with W5100)

// Auth data: hard coded in Lux (no need to change it!) but not in Austria :)  (17 byte!)
uint8_t AUTH_DATA[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                       0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
                       0xFF};

/*------ Things you can change: Define which parameter are published ------*/

// replace in the following tables 'y' with 'n' if you don't want to publish the parameter

// DMSR table:

// DMSR variables
struct Dsmr_field {
  const byte nr;
  const String name;
  const String id;
  const String unit;
  String value;
  const char type;    // string s, double f, integer i
  const char publish; // 'y' to publish item
};

// https://electris.lu/files/Dokumente_und_Formulare/Netz_Tech_Dokumente/SPEC_-_E-Meter_P1_specification_20210305.pdf
Dsmr_field dsmr[] = {
  {0,"identification", "", "","NA",'s','y'},
  {1,"p1_version", "1-3:0.2.8", "","NA",'s','y'},
  {2,"timestamp", "0-0:1.0.0", "","NA",'s','y'},
  {3,"equipment_id", "0-0:42.0.0", "","NA",'s','y'},
  {4,"act_energy_imported_p_plus", "1-0:1.8.0", "kWh","NA",'f','y'},
  {5,"act_energy_exported_p_minus", "1-0:2.8.0", "kWh","NA",'f','y'},
  {6,"react_energy_imported_q_plus", "1-0:3.8.0", "kvarh","NA",'f','y'},
  {7,"react_energy_exported_q_minus", "1-0:4.8.0", "kvarh","NA",'f','y'},
  {8,"act_pwr_imported_p_plus", "1-0:1.7.0", "kW","NA",'f','y'},
  {9,"act_pwr_exported_p_minus", "1-0:2.7.0", "kW","NA",'f','y'},
  {10,"react_pwr_imported_q_plus", "1-0:3.7.0", "kvar","NA",'f','y'},
  {11,"react_pwr_exported_q_minus", "1-0:4.7.0", "kvar","NA",'f','y'},
  {12,"active_threshold_smax", "0-0:17.0.0", "kVA","NA",'f','y'},
  {13,"breaker_ctrl_state_0", "0-0:96.3.10", "","NA",'i','y'},
  {14,"num_pwr_failures", "0-0:96.7.21", "","NA",'i','y'},
  {15,"num_volt_sags_l1", "1-0:32.32.0", "","NA",'i','y'},
  {16,"num_volt_sags_l2", "1-0:52.32.0", "","NA",'i','y'},
  {17,"num_volt_sags_l3", "1-0:72.32.0", "","NA",'i','y'},
  {18,"num_volt_swells_l1", "1-0:32.36.0", "","NA",'i','y'},
  {19,"num_volt_swells_l2", "1-0:52.36.0", "","NA",'i','y'},
  {20,"num_volt_swells_l3", "1-0:72.36.0", "","NA",'i','y'},
  {21,"breaker_ctrl_state_0", "0-0:96.3.10", "","NA",'i','y'},
  {22,"msg_long_e_meter", "0-0:96.13.0", "","NA",'s','y'},
  {23,"msg_long_ch2", "0-0:96.13.2", "","NA",'s','y'},
  {24,"msg_long_ch3", "0-0:96.13.3", "","NA",'s','y'},
  {25,"msg_long_ch4", "0-0:96.13.4", "","NA",'s','y'},
  {26,"msg_long_ch5", "0-0:96.13.5", "","NA",'s','y'},
  {27,"curr_l1", "1-0:31.7.0", "A","NA",'f','y'},
  {28,"curr_l2", "1-0:51.7.0", "A","NA",'f','y'},
  {29,"curr_l3", "1-0:71.7.0", "A","NA",'f','y'},
  {30,"act_pwr_imp_p_plus_l1", "1-0:21.7.0", "kW","NA",'f','y'},
  {31,"act_pwr_imp_p_plus_l2", "1-0:41.7.0", "kW","NA",'f','y'},
  {32,"act_pwr_imp_p_plus_l3", "1-0:61.7.0", "kW","NA",'f','y'},
  {33,"act_pwr_exp_p_minus_l1", "1-0:22.7.0", "kW","NA",'f','y'},
  {34,"act_pwr_exp_p_minus_l2", "1-0:42.7.0", "kW","NA",'f','y'},
  {35,"act_pwr_exp_p_minus_l3", "1-0:62.7.0", "kW","NA",'f','y'},
  {36,"react_pwr_imp_q_plus_l1", "1-0:23.7.0", "kvar","NA",'f','y'},
  {37,"react_pwr_imp_q_plus_l2", "1-0:43.7.0", "kvar","NA",'f','y'},
  {38,"react_pwr_imp_q_plus_l3", "1-0:63.7.0", "kvar","NA",'f','y'},
  {39,"react_pwr_exp_q_minus_l1", "1-0:24.7.0", "kvar","NA",'f','y'},
  {40,"react_pwr_exp_q_minus_l2", "1-0:44.7.0", "kvar","NA",'f','y'},
  {41,"react_pwr_exp_q_minus_l3", "1-0:64.7.0", "kvar","NA",'f','y'},
  {42,"apparent_export_pwr", "1-0:10.7.0", "kVA","NA",'f','y'},
  {43,"apparent_import_pwr", "1-0:9.7.0", "kVA","NA",'f','y'},
  {44,"breaker_ctrl_state_1", "0-1:96.3.10", "","NA",'i','y'},
  {45,"breaker_ctrl_state_2", "0-2:96.3.10", "","NA",'i','y'},
  {46,"limiter_curr_monitor", "1-1:31.4.0", "A","NA",'f','y'},
  {47,"volt_l1", "1-0:32.7.0", "V","NA",'f','y'},
  {48,"volt_l2", "1-0:52.7.0", "V","NA",'f','y'},
  {49,"volt_l3", "1-0:72.7.0", "V","NA",'f','y'},
  {50,"gas_index", "0-1:24.2.1", "m3","NA",'f','y'},
  {51,"device_Type", "0-1:24.4.0", "", "NA",' ','y'},
  {52,"mbus_ch_sw_pos", "0-1:24.1.0", "", "NA",' ','y'},
  {53,"gas_meter_id_hex", "0-1:96.1.0", "", "NA",'s','y'},
};

// table with calculated values:

struct calculated {
  const byte nr;
  const String name;
  double value;
  const char publish; // 'y' to publish item
};

calculated calculated_parameter[] = {
  {0,"energy_consumption_calc_kWh",0.0,'y'},
  {1,"energy_production_calc_kWh",0.0,'y'},
  {2,"energy_consumption_calc_cumul_day_kWh",0.0,'y'},
  {3,"energy_production_calc_cumul_day_kWh",0.0,'y'},
  {4,"power_consumption_calc_from_energy_W",0.0,'y'},
  {5,"power_production_calc_from_energy_W",0.0,'y'},
  {6,"power_consumption_calc_W",0.0,'y'},
  {7,"power_consumption_calc_mean_W",0.0,'y'},
  {8,"power_consumption_calc_max_W",0.0,'y'},
  {9,"power_consumption_calc_min_W",0.0,'y'},
  {10,"power_consumption_l1_calc_W",0.0,'y'},
  {11,"power_consumption_l1_calc_mean_W",0.0,'y'},
  {12,"power_consumption_l1_calc_max_W",0.0,'y'},
  {13,"power_consumption_l1_calc_min_W",0.0,'y'},
  {14,"power_consumption_l2_calc_W",0.0,'y'},
  {15,"power_consumption_l2_calc_mean_W",0.0,'y'},
  {16,"power_consumption_l2_calc_max_W",0.0,'y'},
  {17,"power_consumption_l2_calc_min_W",0.0,'y'},
  {18,"power_consumption_l3_calc_W",0.0,'y'},
  {19,"power_consumption_l3_calc_mean_W",0.0,'y'},
  {20,"power_consumption_l3_calc_max_W",0.0,'y'},
  {21,"power_consumption_l3_calc_min_W",0.0,'y'},
  {22,"power_production_calc_W",0.0,'y'},
  {23,"power_production_calc_mean_W",0.0,'y'},
  {24,"power_production_calc_max_W",0.0,'y'},
  {25,"power_production_calc_min_W",0.0,'y'},
  {26,"power_production_l1_calc_W",0.0,'y'},
  {27,"power_production_l1_calc_mean_W",0.0,'y'},
  {28,"power_production_l1_calc_max_W",0.0,'y'},
  {29,"power_production_l1_calc_min_W",0.0,'y'},
  {30,"power_production_l2_calc_W",0.0,'y'},
  {31,"power_production_l2_calc_mean_W",0.0,'y'},
  {32,"power_production_l2_calc_max_W",0.0,'y'},
  {33,"power_production_l2_calc_min_W",0.0,'y'},
  {34,"power_production_l3_calc_W",0.0,'y'},
  {35,"power_production_l3_calc_mean_W",0.0,'y'},
  {36,"power_production_l3_calc_max_W",0.0,'y'},
  {37,"power_production_l3_calc_min_W",0.0,'y'},
  {38,"power_excess_solar_calc_W",0.0,'y'},
  {39,"power_excess_solar_calc_mean_W",0.0,'y'},
  {40,"power_excess_solar_calc_max_W",0.0,'y'},
  {41,"power_excess_solar_calc_min_W",0.0,'y'},
  {42,"power_excess_solar_l1_calc_W",0.0,'y'},
  {43,"power_excess_solar_l2_calc_W",0.0,'y'},
  {44,"power_excess_solar_l3_calc_W",0.0,'y'}
};
