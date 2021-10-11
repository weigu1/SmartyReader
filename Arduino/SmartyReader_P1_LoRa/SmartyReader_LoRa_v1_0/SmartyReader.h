#pragma once
#ifndef SMARTYREADER_H
#define SMARTYREADER_H

#include "Arduino.h"
#include <Crypto.h>
#include <AES.h>
#include <GCM.h>

// needed to adjust the buffers new data streams > 1024
const uint16_t MAX_SERIAL_STREAM_LENGTH = 1800;

/****** SmartyReader Key ******/
// Key for SAG1030700089067 (16 byte!)
const uint8_t KEY_SMARTY[] = {0xAE, 0xBD, 0x21, 0xB7, 0x69, 0xA6, 0xD1, 0x3C,
                              0x0D, 0xF0, 0x64, 0xE3, 0x83, 0x68, 0x2E, 0xFF}; 


// Auth data: hard coded in Lux (no need to change it!) but not in Austria :)  (17 byte!)
const uint8_t AUTH_DATA[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                             0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
                             0xFF};
    
class SmartyReader {
  public:
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
    /****** INIT functions ****************************************************/
    void init_led(); // initialise the build in LED and switch it on    
    void init_serial4smarty(); // open serial connection to smartmeter on Serial0
    /****** GETTER functions **************************************************/
    bool get_led_log();           // get logger flag for LED
    bool get_serial_log();        // get logger flag for Serial
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
    /****** SmartyReader functions ********************************************/
    void read_telegram();
    void decrypt();
    void init_vector(Vector_GCM *vect, const char *Vect_name,
                     const uint8_t *key_SM, const uint8_t *auth_data);
    void decrypt_text(Vector_GCM *vect);
    void parse_dsmr_string(const String Dmsr);
    String lora_cook_message();
    /****** SmartyReader debug functions **************************************/
    void print_raw_data(unsigned short serial_data_length);
    void print_vector(Vector_GCM *vect);  
    /****** LOGGING functions *************************************************/
    // print log line to Serial1 or Serial2
    void log(String message);
    void log_ln();
    void log_ln(String message);
    /****** HELPER functions **************************************************/
    void led_on(); // build in LED on
    void led_off(); // build in LED off
    void blink_led_x_times(byte x); // blink LED_BUILTIN x times (LED was on)
     // blink LED_BUILTIN x times (LED was on)
    void blink_led_x_times(byte x, word delay_time_ms);
    bool non_blocking_delay(unsigned long milliseconds);
    byte non_blocking_delay_x3(unsigned long ms_1, unsigned long ms_2, unsigned long ms_3);

    /****** Public variables **************************************************/
    uint16_t serial_data_length = 0;         // Data bytes of the raw serial stream     
    uint8_t telegram[MAX_SERIAL_STREAM_LENGTH];
    char buffer[MAX_SERIAL_STREAM_LENGTH-30];
    Dsmr_field dsmr[54] = {
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
      {"device_Type", "0-1:24.4.0", "", ""},
      {"mbus_ch_sw_pos", "0-1:24.1.0", "", ""},
      {"gas_meter_id_hex", "0-1:96.1.0", "", ""}, 
    };

  private:
    bool enable_led_log = false;
    byte led_log_pin = LED_BUILTIN;
    bool led_pos_logic = true;
    word default_delay_time_ms = 100;
    bool enable_serial_log = true;
    byte serial_interface_number = 1;
    const byte LINE_LENGTH_RAW_SERIAL = 20;  // Bytes to print per line if we want to see raw serial  
    Vector_GCM Vector_SM;
    GCM<AES128> *gcmaes128 = 0;
};

#endif // SMARTYREADER_H
