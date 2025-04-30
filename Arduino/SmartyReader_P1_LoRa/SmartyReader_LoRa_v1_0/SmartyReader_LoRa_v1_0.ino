/*
  SmartyReader_LoRa_p2p.ino

  Read P1 port from Luxemburgian smartmeter.
  Decode and publish with LoRa p2p using ESP8266 (LOLIN/WEMOS D1 mini pro)

  V0.1 2021-09-17
  
  ---------------------------------------------------------------------------
  Copyright (C) 2021 Guy WEILER www.weigu.lu
  
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
  More information on LoRa can be found on:
  http://weigu.lu/microcontroller/lora_p2p/index.html
  http://weigu.lu/tutorials/sensors2bus/09_lorawan/index.html
  
  Data from Smarty are on Serial (Serial0) RxD on ESP.
  If you want to debug over Serial, use Serial1 for LOLIN/WEMOS D1 mini pro:
  Transmit-only UART TxD on D4 (LED!)
  
  !! You have to remove the LOLIN/WEMOS from its socket to program it! because the
  hardware serial port is also used during programming !!

  Serial1 uses UART1 which is a transmit-only UART. UART1 TX pin is D4 (GPIO2,
  LED!!). If you use serial (UART0) to communicate with hardware, you can't use
  the Arduino Serial Monitor at the same time to debug your program! The best
  way to debug is to use Serial1.println() and connect RX of an USB2Serial
  adapter (FTDI, Profilic, CP210, ch340/341) to D4 and use a terminal program
  like CuteCom or CleverTerm to listen to D4.

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

  LoRa/WAN infos: http://weigu.lu/tutorials/sensors2bus/09_lorawan/index.html
  LoRa lib: LoRa (arduino-LoRa) by sandeepmistry
  https://github.com/sandeepmistry/arduino-LoRa
  LoRaWAN MCCI LoRaWAN LMIC library by Terry Moore:
  https://github.com/mcci-catena/arduino-lorawan/
  Both libs can be found in the Library Manager (Tools)  
*/
/* Publishes every in milliseconds. To respect the 1% duty rate,
 *  
 */
const long PUBLISH_TIME = 60000;

// Comment or uncomment the following lines suiting your needs
#define LORA_P2P        // LORA_P2P
//#define LORAWAN         // LORAWAN with TTN

#include "SmartyReader.h"
#include <SPI.h>
#ifdef LORA_P2P
  #include <LoRa.h>
#endif // #ifdef LORA_P2P  
#ifdef LORAWAN
  #include <lmic.h>
  #include <hal/hal.h>
#endif // #ifdef LORAWAN

// LoRa settings and variables
#ifdef ESP8266
  const byte PIN_SS = 16;         // LoRa radio chip select
  const byte PIN_RST = NOT_A_PIN; // LoRa radio reset
  const byte PIN_IRQ = 15 ;       // hardware interrupt pin!
#else
  const byte PIN_SS = 26;
  const byte PIN_RST = NOT_A_PIN;
  const byte PIN_IRQ = 5 ;
#endif // #ifdef ESP8266

#ifdef LORA_P2P
  const byte NODE_ADDR = 0x05;                 // address of this device
  const byte GATEWAY_ADDR = 0xFE;              // destination 0xFE = gateway 0xFF = broadcast
  byte msg_out_id = 0;   
  byte addr_in_rec, addr_in_sender, msg_in_id, msg_in_length;
  String msg_out, msg_in, lora_rssi, lora_snr;
  volatile bool flag_message_received = false; // flag set by callback
#endif // #ifdef LORA_P2P  


#ifdef LORAWAN
// APPEUI and DEVEUI LSB first! APPKEY MSB first!
  static const u1_t PROGMEM APPEUI[8]={ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}
  static const u1_t PROGMEM DEVEUI[8]={ 0xB4, 0x61, 0x04, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
  void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}
  static const u1_t PROGMEM APPKEY[16] = { 0xA1, 0x28, 0xE4, 0x46, 0xBA, 0x02, 0xEF, 0xD4, 0xEC, 0xDA, 0x47, 0xEE, 0x69, 0x1D, 0x15, 0xF9 };
  void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}  
  static osjob_t sendjob;  
  const lmic_pinmap lmic_pins = {  // Pin mapping
    .nss = 16,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = LMIC_UNUSED_PIN,
    .dio = {15, 15, LMIC_UNUSED_PIN},
  };
  
#endif // #ifdef LORAWAN


SmartyReader SR; // create SmartyReader object

/********** SETUP *************************************************************/

void setup() {  
  SR.set_serial_log(1);
  SR.init_serial4smarty();
  #ifdef LORA_P2P
    init_lora_p2p();
  #endif // #ifdef LoRa_P2P  
  #ifdef LORAWAN
    init_lorawan();
  #endif // #ifdef LoRaWAN
  memset(SR.telegram, 0, MAX_SERIAL_STREAM_LENGTH);
  delay(2000); // give it some time
  SR.log_ln("setup done");  
}

/********** LOOP  **************************************************************/

void loop() {
  SR.read_telegram();  
  if (SR.non_blocking_delay(PUBLISH_TIME)) { // Publish every PUBLISH_TIME
    SR.log_ln("Serial_data_length is :" + String(SR.serial_data_length));
    SR.decrypt();
    #ifdef LORA_P2P
      lora_p2p_send_message(SR.lora_cook_message());
    #endif // #ifdef LoRa_P2P    
  }  
  yield();
  #ifdef LORAWAN
    os_runloop_once();
  #endif // #ifdef LoRaWAN
}


/********** LoRa functions ***************************************************/
#ifdef LORA_P2P
  void init_lora_p2p() {
    //SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);   //SPI LoRa pins
    LoRa.setPins(PIN_SS, PIN_RST, PIN_IRQ);  // setup LoRa transceiver module
    if (!LoRa.begin(868E6)) {
      SR.log_ln("Error starting LoRa!");
      while (true) { // endless loop
        yield();
      }
    }
    LoRa.onReceive(lora_p2p_onReceive); // callback
  }
  
  void lora_p2p_onReceive(int packetSize) {
    if (packetSize == 0) {         // if there's no packet, return
      return;
    }
    flag_message_received = true;  //Set flag to perform read in main loop
  }
  
  void lora_p2p_send_message(String message_out) {
    lora_p2p_tx_mode();                       // set tx mode
    LoRa.beginPacket();                   // start packet
    LoRa.write(GATEWAY_ADDR);             // add destination address
    LoRa.write(NODE_ADDR);                // add sender address
    LoRa.write(msg_out_id);               // add message ID (counter)
    LoRa.write(message_out.length());     // add payload length
    LoRa.print(message_out);              // add payload  
    LoRa.endPacket();                     // finish packet and send it
    msg_out_id++;                         // increment message counter (ID)
    lora_p2p_rx_mode();                       // go back into receive mode
    SR.log("----\nLoRa message out (Gateway address, Node address,");
    SR.log_ln("Message ID (3 byte), \"text\"): ");  
    SR.log_ln("0x" + String(GATEWAY_ADDR,HEX) + ",0x" + String(NODE_ADDR,HEX) +
              ",0x" + String(msg_out_id,HEX) + ",\"" + message_out + "\"");
  }
  
  void lora_p2p_rx_mode() {
    LoRa.enableInvertIQ();                // active invert I and Q signals
    LoRa.receive();                       // set receive mode
  }
  
  void lora_p2p_tx_mode() {
    LoRa.idle();                          // set standby mode
    LoRa.disableInvertIQ();               // normal mode
  }    
#endif // #ifdef LORA_P2P  

/********** LoRaWAN functions ***************************************************/
#ifdef LORAWAN
  void init_lorawan() {
    LMIC_setLinkCheckMode(0); //Disable link-check mode and ADR.
    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
    os_init(); // LMIC init    
    LMIC_reset(); // Reset the MAC state.    
    do_send(&sendjob); // Start job (sending automatically starts OTAA too)
  }
  
  void onEvent (ev_t ev) {
    SR.log(String(os_getTime()));
    SR.log(": ");
    switch(ev) {
      case EV_SCAN_TIMEOUT:
        SR.log_ln(F("EV_SCAN_TIMEOUT"));
        break;
      case EV_BEACON_FOUND:
        SR.log_ln(F("EV_BEACON_FOUND"));
        break;
      case EV_BEACON_MISSED:
        SR.log_ln(F("EV_BEACON_MISSED"));
        break;
      case EV_BEACON_TRACKED:
        SR.log_ln(F("EV_BEACON_TRACKED"));
        break;
      case EV_JOINING:
        SR.log_ln(F("EV_JOINING"));
        break;
      case EV_JOINED:
        SR.log_ln(F("EV_JOINED")); {
          u4_t netid = 0;
          devaddr_t devaddr = 0;
          u1_t nwkKey[16];
          u1_t artKey[16];
          LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
          SR.log("netid: ");
          SR.log_ln(String(netid, DEC));
          SR.log("devaddr: ");
          SR.log_ln(String(devaddr, HEX));
          SR.log("AppSKey: ");
          for (size_t i=0; i<sizeof(artKey); ++i) {
            if (i != 0) {
              SR.log("-");
            }  
            printHex2(artKey[i]);
            }
            SR.log_ln("");
            SR.log("NwkSKey: ");
            for (size_t i=0; i<sizeof(nwkKey); ++i) {
              if (i != 0) {
                SR.log("-");
              }  
              printHex2(nwkKey[i]);
            }
            SR.log_ln();
        }
        LMIC_setLinkCheckMode(0);
        break;
      case EV_JOIN_FAILED:
        SR.log_ln(F("EV_JOIN_FAILED"));
        break;
      case EV_REJOIN_FAILED:
        SR.log_ln(F("EV_REJOIN_FAILED"));
        break;
      case EV_TXCOMPLETE:
        SR.log_ln(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
        if (LMIC.txrxFlags & TXRX_ACK) {
          SR.log_ln(F("Received ack"));
        }  
        if (LMIC.dataLen) {
          SR.log(F("Received "));
          SR.log(String(LMIC.dataLen));
          SR.log_ln(F(" bytes of payload"));
        }
        // Schedule next transmission
        os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(PUBLISH_TIME/1000), do_send);
        break;
      case EV_LOST_TSYNC:
        SR.log_ln(F("EV_LOST_TSYNC"));
        break;
      case EV_RESET:
        SR.log_ln(F("EV_RESET"));
        break;
      case EV_RXCOMPLETE:      
        SR.log_ln(F("EV_RXCOMPLETE")); // data received in ping slot
        break;
      case EV_LINK_DEAD:
        SR.log_ln(F("EV_LINK_DEAD"));
        break;
      case EV_LINK_ALIVE:
        SR.log_ln(F("EV_LINK_ALIVE"));
        break;
      case EV_TXSTART:
        SR.log_ln(F("EV_TXSTART"));
        break;
      case EV_TXCANCELED:
        SR.log_ln(F("EV_TXCANCELED"));
        break;
      case EV_RXSTART:
        break; /* do not print anything -- it wrecks timing */
      case EV_JOIN_TXCOMPLETE:
        SR.log_ln(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
        break;
      default:
        SR.log(F("Unknown event: "));
        SR.log_ln(String((unsigned) ev));
        break;
    }
  }
  
  void do_send(osjob_t* j) {  
    if (LMIC.opmode & OP_TXRXPEND) {  // Check if there is not a current TX/RX job running
      SR.log_ln(F("OP_TXRXPEND, not sending"));
    } else {
      // Prepare upstream data transmission at the next possible time.
      String MyData_S = SR.lora_cook_message();
      byte mydata_a[MyData_S.length()+1];
      MyData_S.getBytes(mydata_a, MyData_S.length()+1);      
      LMIC_setTxData2(1, mydata_a, sizeof(mydata_a)-1, 0);
      SR.log_ln(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
  }
#endif // #ifdef LORA_WAN

/****** HELPER functions *************************************************/

void printHex2(unsigned v) {
  v &= 0xff;
  if (v < 16)
      SR.log("0");
  SR.log(String(v, HEX));
}
