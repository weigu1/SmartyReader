/*
 * smartmeter_4xp1_rw_teensy36.ino
 * read 4 (5) P1 ports from smartmeters
 * and rainwater sensor () and publish with MQTT
 *
 * teensy 3.6 with teensyduino 1.37
 *
 * weigu.lu
 *
 * change RX buffer to 1024 for smartmeter n (#define SERIALn_RX_BUFFER_SIZE)
 * in arduino-1.8.0/hardware/teensy/avr/cores/teensy3/serialn.c
 *
 * (changed message size to 256 in
 * /Arduino/libraries/pubsubclient-master/src/PubSubClient.h)
 * 
 * v 11/4/18
*/

#include <Ethernet.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>
#include <Crypto.h>
#include <AES.h>
#include <GCM.h>
#include <string.h>
//#include <Wire.h> // for DS3132

#define MQTT_MAX_PACKET_SIZE 256 //override value in PubSubClient.h
#define MAX_LINE_LENGTH 1024

byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
IPAddress eth_ip     (192,168,1,112); //static IP
IPAddress dns_ip     (192,168,1,20);
IPAddress gateway_ip (192,168,1,20);
IPAddress subnet_mask(255,255,255,0);
EthernetClient ethClient;
PubSubClient client(ethClient);

const char *mqtt_server = "192.168.1.60";
const int  mqttPort = 1883;
const char *clientId = "smartmeters_p1_rw";
const char *topic     = "weigu/basement/storage/smartmeters";
const char *topic_sm1 = "weigu/basement/storage/smartmeter1_p1";
const char *topic_sm2 = "weigu/basement/storage/smartmeter2_p1";
const char *topic_sm3 = "weigu/basement/storage/smartmeter3_p1";
const char *topic_sm4 = "weigu/basement/storage/smartmeter4_p1";
const char *topic_sm5 = "weigu/basement/storage/smartmeter5_p1";
const char *topic_rw  = "weigu/basement/storage/rainwater";

const byte teensy_LED = 13; // PD6 Teensy LED
const byte data_request_SM2 = 2; //active Low!
const byte data_request_SM3 = 3;
const byte data_request_SM4 = 4;
const byte data_request_SM5 = 29;
const byte data_request_SM1 = 30;
const byte data_request_RW6 = 37; //collect data from rainwater sensor
const byte power_LED = 35;
const byte act_LED = 36;

struct TestVector {
    const char *name;
    uint8_t key[16];
    uint8_t ciphertext[MAX_LINE_LENGTH];
    uint8_t authdata[17];
    uint8_t iv[12];
    uint8_t tag[16];
    uint8_t authsize;
    uint16_t datasize;
    uint8_t tagsize;
    uint8_t ivsize; };

TestVector Vector_SM1,Vector_SM2,Vector_SM3,Vector_SM4,Vector_SM5;
GCM<AES128> *gcmaes128 = 0;

//Keys for SAG1030700026xxx
uint8_t key_SM2[] = {0x19, 0xE5, 0xFE, 0x03, 0xC4, 0xB0, 0x65, 0xB4, // Consum.
                     0x25, 0x77, 0xCC, 0x9B, 0xB7, 0x4C, 0x0F, 0x9B};// 430
uint8_t key_SM3[] = {0x5A, 0x9B, 0xDB, 0x8C, 0xE3, 0xFD, 0xB7, 0x02, // PV 10kW
                     0x16, 0x35, 0xFF, 0x6F, 0xB0, 0x2E, 0xE1, 0xDF};// 568
uint8_t key_SM4[] = {0x37, 0xDB, 0x59, 0x3F, 0xBB, 0xDD, 0xE3, 0xC6, // PV 8kW
                     0xA8, 0x28, 0xF3, 0xBF, 0x9A, 0x2E, 0x7F, 0x8B};// 423
uint8_t key_SM5[] = {0x74, 0x5E, 0x2D, 0x96, 0x6E, 0xA0, 0x95, 0xDD, // PV 3kW
                     0x10, 0x46, 0x24, 0xBD, 0x16, 0x6D, 0x6F, 0x2E};// 552

char *identification;
char *p1_version;
char *timestamp;
char *equipment_id;
char *energy_delivered_tariff1;
char *energy_returned_tariff1;
char *reactive_energy_delivered_tariff1;
char *reactive_energy_returned_tariff1;
char *power_delivered;
char *power_returned;
char *reactive_power_delivered;
char *reactive_power_returned;
char *electricity_threshold;
char *electricity_switch_position;
char *electricity_failures;
char *electricity_sags_l1;
char *electricity_sags_l2;
char *electricity_sags_l3;
char *electricity_swells_l1;
char *electricity_swells_l2;
char *electricity_swells_l3;
char *message_short;
char *message2_long;
char *message3_long;
char *message4_long;
char *message5_long;
char *current_l1;
char *current_l2;
char *current_l3;

char *id_p1_version = "1-3:0.2.8(";
char *id_timestamp = "0-0:1.0.0(";
char *id_equipment_id = "0-0:42.0.0(";
char *id_energy_delivered_tariff1 = "1-0:1.8.0(";
char *id_energy_returned_tariff1 = "1-0:2.8.0(";
char *id_reactive_energy_delivered_tariff1 = "1-0:3.8.0(";
char *id_reactive_energy_returned_tariff1 = "1-0:4.8.0(";
char *id_power_delivered = "1-0:1.7.0(";
char *id_power_returned = "1-0:2.7.0(";
char *id_reactive_power_delivered = "1-0:3.7.0(";
char *id_reactive_power_returned = "1-0:4.7.0(";
char *id_electricity_threshold = "0-0:17.0.0(";
char *id_electricity_switch_position = "0-0:96.3.10(";
char *id_electricity_failures = "0-0:96.7.21(";
char *id_electricity_sags_l1 = "1-0:32.32.0(";
char *id_electricity_sags_l2 = "1-0:52.32.0(";
char *id_electricity_sags_l3 = "1-0:72.32.0(";
char *id_electricity_swells_l1 = "1-0:32.36.0(";
char *id_electricity_swells_l2 = "1-0:52.36.0(";
char *id_electricity_swells_l3 = "1-0:72.36.0(";
char *id_message_short = "0-0:96.13.0(";
char *id_message2_long = "0-0:96.13.2(";
char *id_message3_long = "0-0:96.13.3(";
char *id_message4_long = "0-0:96.13.4(";
char *id_message5_long = "0-0:96.13.5(";
char *id_current_l1 = "1-0:31.7.0(";
char *id_current_l2 = "1-0:51.7.0(";
char *id_current_l3 = "1-0:71.7.0(";

const char filename2[13] = "10_04_18.txt";
uint8_t telegram[MAX_LINE_LENGTH];
char rainwater[6] = "E999\n";
char buffer[MAX_LINE_LENGTH-30];

const byte CS_ETH_pin = 24; // MOSI0 11 MISO0 12
const byte SCK_ETH_pin = 14;
const byte CS_SD = BUILTIN_SDCARD;

char msg[MQTT_MAX_PACKET_SIZE];
char msg_sm1[MQTT_MAX_PACKET_SIZE], msg_sm2[MQTT_MAX_PACKET_SIZE];
char msg_sm3[MQTT_MAX_PACKET_SIZE], msg_sm4[MQTT_MAX_PACKET_SIZE];
char msg_sm5[MQTT_MAX_PACKET_SIZE], msg_rw[128], SDline[1024], filename[13];

byte tsec, tmin, thour, tweekd, tday, tmon, tyear;
int test,Tlength;

void setup() {
  delay(10000); //time to connect 2 smartmeter (needs more than 250mA!)

  pinMode(teensy_LED, OUTPUT);    // Initialize outputs
  pinMode(data_request_SM1, OUTPUT);
  pinMode(data_request_SM2, OUTPUT);
  pinMode(data_request_SM3, OUTPUT);
  pinMode(data_request_SM4, OUTPUT);
  pinMode(data_request_SM5, OUTPUT);
  pinMode(data_request_RW6, OUTPUT);
  pinMode(data_request_RW6, OUTPUT);
  pinMode(power_LED, OUTPUT);
  pinMode(act_LED, OUTPUT);
  digitalWrite(power_LED,HIGH);   // On
  digitalWrite(teensy_LED,HIGH);   // On

  setSyncProvider(getTeensy3Time); // set the Time lib for Teensy 3 RTC

  SPI.setSCK(SCK_ETH_pin);

  digitalWrite(teensy_LED,HIGH);   // On
  Serial.begin(115200);  // for debugging
  Serial.println("Serial is working!");

  Serial1.begin(115200);
  Serial2.begin(115200);
  Serial3.begin(115200);
  Serial4.begin(115200);
  Serial5.begin(115200);
  Serial6.begin(9600); //rainwater sensor XL-MaxSonar MB7060

  Serial.print("Initializing SD card...");
  if (!SD.begin(CS_SD)) { // see if the card is present and can be initialized:
    Serial.println("Card failed, or not present");
    return; }
  Serial.println("card initialized.");

  client.setServer(mqtt_server,mqttPort);
  Ethernet.init(CS_ETH_pin); //needed to avoid conflict with SD card!!
  Ethernet.begin(mac, eth_ip);
  delay(1500);  // Allow the hardware to sort itself out
  digitalWrite(teensy_LED,LOW);   // Off, less energy
  }

void loop() {
  if (second()==0) {
    digitalWrite(act_LED,HIGH);   // On
    digitalClockDisplay();

    if (!client.connected()) { reconnect(); }
    client.loop();
    Serial.println("Loop beginns");

    //Rainwater first
    digitalWrite(data_request_RW6,HIGH);   // Request serial data On
    delay(20);
    Tlength = readTelegram(6);
    digitalWrite(data_request_RW6,LOW);  // Request serial data Off
    snprintf (msg, 40, "{\"rainwater\":\"distance\",\"cm\":\"%c%c%c\"}",
              rainwater[1],rainwater[2],rainwater[3]);    
    snprintf (msg_rw, 40, "%c%c%c",rainwater[1],rainwater[2],rainwater[3]);              
    Serial.println(msg);
    Serial.println(msg_rw);
    test=client.publish(topic_rw, msg);
    
    //SM2 on Serial1
    memset(telegram, '1', sizeof(telegram));
    digitalWrite(data_request_SM2,LOW);   // Request serial data On
    delay(10);
    Tlength = readTelegram(1);
    digitalWrite(data_request_SM2,HIGH);   // Request serial data Off
    Serial.print("Message length: ");
    Serial.println(Tlength);
    init_vector(&Vector_SM2,"Vector_SM2",key_SM2);
    if (Vector_SM2.datasize != 1024) {
      decrypt_text(&Vector_SM2);
      parse_dsmr_string(buffer);
      snprintf (msg, MQTT_MAX_PACKET_SIZE, "{\"dt\":\"%s\",\"id\":\"%c%c%c\""
                ",\"c1\":\"%s\",\"p1\":\"%s\",\"c2\":\"%s\",\"p2\":\"%s\"}",
                timestamp,equipment_id[13],equipment_id[14],equipment_id[15],
                energy_delivered_tariff1, energy_returned_tariff1,
                reactive_energy_delivered_tariff1,
                reactive_energy_returned_tariff1);
      snprintf (msg_sm2, 128, "%c%c%c\t%s\t%s\t%s\t%s\t%s",
                equipment_id[13],equipment_id[14],equipment_id[15],timestamp,
                energy_delivered_tariff1, energy_returned_tariff1,
                reactive_energy_delivered_tariff1,
                reactive_energy_returned_tariff1);
      Serial.println(msg);
      Serial.println(msg_sm2);
      test=client.publish(topic_sm2, msg);
    }

    //SM3 on Serial2
    memset(telegram, '1', sizeof(telegram));
    digitalWrite(data_request_SM3,LOW);   // Request serial data On
    delay(10);
    Tlength = readTelegram(2);
    digitalWrite(data_request_SM3,HIGH);   // Request serial data Off
    Serial.print("Message length: ");
    Serial.println(Tlength);
    init_vector(&Vector_SM3,"Vector_SM3",key_SM3);
    if (Vector_SM3.datasize != 1024) {
      decrypt_text(&Vector_SM3);
      parse_dsmr_string(buffer);
      snprintf (msg, MQTT_MAX_PACKET_SIZE, "{\"dt\":\"%s\",\"id\":\"%c%c%c\""
                ",\"c1\":\"%s\",\"p1\":\"%s\",\"c2\":\"%s\",\"p2\":\"%s\"}",
                timestamp,equipment_id[13],equipment_id[14],equipment_id[15],
                energy_delivered_tariff1, energy_returned_tariff1,
                reactive_energy_delivered_tariff1,
                reactive_energy_returned_tariff1);
      snprintf (msg_sm3, 128, "%c%c%c\t%s\t%s\t%s\t%s\t%s",
                equipment_id[13],equipment_id[14],equipment_id[15],timestamp,
                energy_delivered_tariff1, energy_returned_tariff1,
                reactive_energy_delivered_tariff1,
                reactive_energy_returned_tariff1);
      Serial.println(msg);
      Serial.println(msg_sm3);
      test=client.publish(topic_sm3, msg);
    }

    //SM4 on Serial3
    memset(telegram, '1', sizeof(telegram));
    digitalWrite(data_request_SM4,LOW);   // Request serial data On
    delay(10);
    Tlength = readTelegram(3);
    digitalWrite(data_request_SM4,HIGH);   // Request serial data Off
    Serial.print("Message length: ");
    Serial.println(Tlength);
    init_vector(&Vector_SM4,"Vector_SM4",key_SM4);
    if (Vector_SM4.datasize != 1024) {
      decrypt_text(&Vector_SM4);
      parse_dsmr_string(buffer);
      snprintf (msg, MQTT_MAX_PACKET_SIZE, "{\"dt\":\"%s\",\"id\":\"%c%c%c\""
                ",\"c1\":\"%s\",\"p1\":\"%s\",\"c2\":\"%s\",\"p2\":\"%s\"}",
                timestamp,equipment_id[13],equipment_id[14],equipment_id[15],
                energy_delivered_tariff1, energy_returned_tariff1,
                reactive_energy_delivered_tariff1,
                reactive_energy_returned_tariff1);
      snprintf (msg_sm4, 128, "%c%c%c\t%s\t%s\t%s\t%s\t%s",
                equipment_id[13],equipment_id[14],equipment_id[15],timestamp,
                energy_delivered_tariff1, energy_returned_tariff1,
                reactive_energy_delivered_tariff1,
                reactive_energy_returned_tariff1);
      Serial.println(msg);
      Serial.println(msg_sm4);
      test=client.publish(topic_sm4, msg);
    }

    //SM5 on Serial4
    memset(telegram, '1', sizeof(telegram));
    digitalWrite(data_request_SM5,LOW);   // Request serial data On
    delay(10);
    Tlength = readTelegram(4);
    digitalWrite(data_request_SM5,HIGH);   // Request serial data Off
    Serial.print("Message length: ");
    Serial.println(Tlength);
    /*Serial.print("T: ");
    for(int cnt=0; cnt<10;cnt++)
          Serial.print(telegram[cnt],HEX);
    Serial.println();  */
    init_vector(&Vector_SM5,"Vector_SM5",key_SM5);
    //print_vector(&Vector_SM5);
    if (Vector_SM5.datasize != 1024) {
      decrypt_text(&Vector_SM5);
      parse_dsmr_string(buffer);
      //print_dsmr();
      snprintf (msg, MQTT_MAX_PACKET_SIZE, "{\"dt\":\"%s\",\"id\":\"%c%c%c\""
                ",\"c1\":\"%s\",\"p1\":\"%s\",\"c2\":\"%s\",\"p2\":\"%s\"}",
                timestamp,equipment_id[13],equipment_id[14],equipment_id[15],
                energy_delivered_tariff1, energy_returned_tariff1,
                reactive_energy_delivered_tariff1,
                reactive_energy_returned_tariff1);
      snprintf (msg_sm5, 128, "%c%c%c\t%s\t%s\t%s\t%s\t%s",
                equipment_id[13],equipment_id[14],equipment_id[15],timestamp,
                energy_delivered_tariff1, energy_returned_tariff1,
                reactive_energy_delivered_tariff1,
                reactive_energy_returned_tariff1);
      Serial.println(msg);
      Serial.println(msg_sm5);
      test=client.publish(topic_sm5, msg);
    }

    //get_time(&tsec, &tmin, &thour, &tweekd, &tday, &tmon, &tyear);//if DS3231
    snprintf (filename, 13, "%02d_%02d_%02d.txt",day(), month(), year()-2000);
    Serial.println(filename);

    File dataFile = SD.open(filename, FILE_WRITE); // open file
    if (dataFile) {  // if the file is available, write to it:
      snprintf (SDline, 1024, "%s\t%s\t%s\t%s\t%s\n",
                msg_sm2, msg_sm3, msg_sm4, msg_sm5, msg_rw);
      dataFile.println(SDline);
      dataFile.close(); }
    else {
      Serial.println("error opening file.txt");}
    Serial.println("published");  
    delay(1000);
    digitalWrite(act_LED,LOW);   // Off
  }
}


int readTelegram(int nr) {
  int cnt = 0;
  switch (nr) {
    case 1:
      while ((Serial1.available()) && (cnt!= MAX_LINE_LENGTH)) {
        telegram[cnt] = Serial1.read();
        cnt++; }
      break;
    case 2:
      while ((Serial2.available()) && (cnt!= MAX_LINE_LENGTH)) {
        telegram[cnt] = Serial2.read();
        cnt++; }
      break;
    case 3:
      while ((Serial3.available()) && (cnt!= MAX_LINE_LENGTH)) {
        telegram[cnt] = Serial3.read();
        cnt++; }
      break;
    case 4:
      while ((Serial4.available()) && (cnt!= MAX_LINE_LENGTH)) {
        telegram[cnt] = Serial4.read();
        cnt++; }
      break;
    case 5:
      while ((Serial5.available()) && (cnt!= MAX_LINE_LENGTH)) {
        telegram[cnt] = Serial5.read();
        cnt++; }
      break;
    case 6:
      while ((Serial6.available()) && (cnt!= 5)) {
        rainwater[cnt] = Serial6.read();
        cnt++; }
      break;
    default:
      Serial.println("no such serial port");
      break; }
  return (cnt); }

void reconnect() {
  while (!client.connected()) { // Loop until we're reconnected
    if (client.connect(clientId)) { // Attempt to connect publish announcement
      client.publish(topic, "{\"smartmeters_P1\":\"connected\"}"); }
    else { delay(5000); }}}// Wait 5 seconds before retrying

// Convert binary coded decimal to decimal number
byte bcdToDec(byte val) { return((val/16*10) + (val%16)); }

// Convert decimal number to binary coded decimal
byte decToBcd(byte val) { return((val/10*16) + (val%10)); }





time_t getTeensy3Time() { return Teensy3Clock.get(); }

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.println(); }

void printDigits(int digits){
  // utility function clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits); }

  void init_vector(TestVector *vect, char *Vect_name, uint8_t *key_SM) {
    uint16_t Data_Length;
    vect->name = Vect_name;  // vector name
    for (int i = 0; i < 16; i++)
       vect->key[i] = key_SM[i];
    // get length of data
    Data_Length = uint16_t(telegram[11])*256 + uint16_t(telegram[12])-17;
    if (Data_Length>MAX_LINE_LENGTH) Data_Length = MAX_LINE_LENGTH;
    for (int i = 0; i < Data_Length; i++)
       vect->ciphertext[i] = telegram[i+18];
    uint8_t AuthData[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                          0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
                          0xFF};
    for (int i = 0; i < 17; i++)
       vect->authdata[i] = AuthData[i];
    for (int i = 0; i < 8; i++)
       vect->iv[i] = telegram[2+i];
    for (int i = 8; i < 12; i++)
       vect->iv[i] = telegram[6+i];
    for (int i = 0; i < 12; i++)
       vect->tag[i] = telegram[Data_Length+18+i];
    vect->authsize = 17;
    vect->datasize = Data_Length;
    vect->tagsize = 12;
    vect->ivsize  = 12; }

void decrypt_text(TestVector *vect) {
  gcmaes128 = new GCM<AES128>();
  size_t posn, len;
  size_t inc = vect->datasize;
  memset(buffer, 0xBA, sizeof(buffer));
  gcmaes128->setKey(vect->key, gcmaes128->keySize());
  gcmaes128->setIV(vect->iv, vect->ivsize);
  for (posn = 0; posn < vect->datasize; posn += inc) {
    len = vect->datasize - posn;
    if (len > inc) len = inc;
    gcmaes128->decrypt((uint8_t*)buffer + posn, vect->ciphertext + posn, len);
   }
  delete gcmaes128;
  /*for (int i = 0; i < vect->datasize; i++)
    Serial.print(buffer[i]);*/
}

void parse_dsmr_string(char *mystring) {
   bool result;
   char *field;
   identification = strtok(mystring, "\n"); // get the first field
   while ((field[0] != '!') && (field != NULL)) { // walk through other fields
     field = strtok(NULL, "\n");
     result=test_field(field,id_p1_version);
     if (result) {
        get_field(field);
        p1_version = field;
      }
     result=test_field(field,id_timestamp);
     if (result) {
        get_field(field);
        timestamp = field;
      }
     result=test_field(field,id_equipment_id);
     if (result) {
        get_field(field);
        convert_equipment_id(field);
        equipment_id = field;
      }
     result=test_field(field,id_energy_delivered_tariff1);
     if (result) {
        get_field(field);
        energy_delivered_tariff1 = field;
      }
     result=test_field(field,id_energy_returned_tariff1);
     if (result) {
        get_field(field);
        energy_returned_tariff1 = field;
      }
     result=test_field(field,id_reactive_energy_delivered_tariff1);
     if (result) {
        get_field(field);
        reactive_energy_delivered_tariff1 = field;
      }
     result=test_field(field,id_reactive_energy_returned_tariff1);
     if (result) {
        get_field(field);
        reactive_energy_returned_tariff1 = field;
      }
     result=test_field(field,id_power_delivered);
     if (result) {
        get_field(field);
        power_delivered = field;
      }
     result=test_field(field,id_power_returned);
     if (result) {
        get_field(field);
        power_returned = field;
      }
     result=test_field(field,id_reactive_power_delivered);
     if (result) {
        get_field(field);
        reactive_power_delivered = field;
      }
     result=test_field(field,id_reactive_power_returned);
     if (result) {
        get_field(field);
        reactive_power_returned = field;
      }
     result=test_field(field,id_electricity_threshold);
     if (result) {
        get_field(field);
        electricity_threshold = field;
      }
     result=test_field(field,id_electricity_switch_position);
     if (result) {
        get_field(field);
        electricity_switch_position = field;
      }
     result=test_field(field,id_electricity_failures);
     if (result) {
        get_field(field);
        electricity_failures = field;
      }
     result=test_field(field,id_electricity_sags_l1);
     if (result) {
        get_field(field);
        electricity_sags_l1 = field;
      }
     result=test_field(field,id_electricity_sags_l2);
     if (result) {
        get_field(field);
        electricity_sags_l2 = field;
      }
     result=test_field(field,id_electricity_sags_l3);
     if (result) {
        get_field(field);
        electricity_sags_l3 = field;
      }
     result=test_field(field,id_electricity_swells_l1);
     if (result) {
        get_field(field);
        electricity_swells_l1 = field;
      }
     result=test_field(field,id_electricity_swells_l2);
     if (result) {
        get_field(field);
        electricity_swells_l2 = field;
      }
     result=test_field(field,id_electricity_swells_l3);
     if (result) {
        get_field(field);
        electricity_swells_l3 = field;
      }
     result=test_field(field,id_message_short);
     if (result) {
        get_field(field);
        message_short = field;
      }
     result=test_field(field,id_message2_long);
     if (result) {
        get_field(field);
        message2_long = field;
      }
     result=test_field(field,id_message3_long);
     if (result) {
        get_field(field);
        message3_long = field;
      }
     result=test_field(field,id_message4_long);
     if (result) {
        get_field(field);
        message4_long = field;
      }
     result=test_field(field,id_message5_long);
     if (result) {
        get_field(field);
        message5_long = field;
      }
     result=test_field(field,id_current_l1);
     if (result) {
        get_field(field);
        current_l1 = field;
      }
     result=test_field(field,id_current_l2);
     if (result) {
        get_field(field);
        current_l2 = field;
      }
     result=test_field(field,id_current_l3);
     if (result) {
        get_field(field);
        current_l3 = field;
      }
   }
}

void convert_equipment_id(char *mystring) { //coded in HEX
  int len =strlen(mystring);
  for (int i=0; i<len/2; i++)
    mystring[i]=char(((int(mystring[i*2]))-48)*16+(int(mystring[i*2+1])-48));
  mystring[(len/2)]= NULL;
}

void get_field(char *mystring) {
  int a = strcspn (mystring,"(");
  int b = strcspn (mystring,")");
  int j = 0;
  for (int i=a+1; i<b; i++) {
    mystring[j] = mystring[i];
    j++;
  }
  mystring[j]= NULL;
}

bool test_field(char *field2, char *dmsr_field_id) {
  bool result = true;
  for (int i=0; i<10; i++) {
    if(field2[i] == dmsr_field_id[i]) result= result && true;
    else result = false;
  }
  return result;
}

void print_vector(TestVector *vect) {
    Serial.print("\nVector_Name: ");
    Serial.println(vect->name);
    Serial.print("Key: ");
    for(int cnt=0; cnt<16;cnt++)
        Serial.print(vect->key[cnt],HEX);
    Serial.print("\nData (Text): ");
    for(int cnt=0; cnt<vect->datasize;cnt++)
        Serial.print(vect->ciphertext[cnt],HEX);
    Serial.print("\nAuth_Data: ");
    for(int cnt=0; cnt<17;cnt++)
        Serial.print(vect->authdata[cnt],HEX);
    Serial.print("\nInit_Vect: ");
    for(int cnt=0; cnt<12;cnt++)
        Serial.print(vect->iv[cnt],HEX);
    Serial.print("\nAuth_Tag: ");
    for(int cnt=0; cnt<12;cnt++)
        Serial.print(vect->tag[cnt],HEX);
    Serial.print("\nAuth_Data Size: ");
    Serial.println(vect->authsize);
    Serial.print("Data Size: ");
    Serial.println(vect->datasize);
    Serial.print("Auth_Tag Size: ");
    Serial.println(vect->tagsize);
    Serial.print("Init_Vect Size: ");
    Serial.println(vect->ivsize);
    Serial.println(); }

void print_dsmr() {
  Serial.println(identification);
  Serial.println(p1_version);
  Serial.println(timestamp);
  Serial.println(equipment_id);
  Serial.println(energy_delivered_tariff1);
  Serial.println(energy_returned_tariff1);
  Serial.println(reactive_energy_delivered_tariff1);
  Serial.println(reactive_energy_returned_tariff1);
  Serial.println(power_delivered);
  Serial.println(power_returned);
  Serial.println(reactive_power_delivered);
  Serial.println(reactive_power_returned);
  Serial.println(electricity_threshold);
  Serial.println(electricity_switch_position);
  Serial.println(electricity_failures);
  Serial.println(electricity_sags_l1);
  Serial.println(electricity_sags_l2);
  Serial.println(electricity_sags_l3);
  Serial.println(electricity_swells_l1);
  Serial.println(electricity_swells_l2);
  Serial.println(electricity_swells_l3);
  Serial.println(message_short);
  Serial.println(message2_long);
  Serial.println(message3_long);
  Serial.println(message4_long);
  Serial.println(message5_long);
  Serial.println(current_l1);
  Serial.println(current_l2);
  Serial.println(current_l3);
}

/*// get the time from DS3231 (I2C)
void get_time(byte *s, byte *m, byte *h, byte *wd, byte *d, byte *mo, byte *y)
{
  Wire.beginTransmission(DS3231);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.beginTransmission(DS3231);
  Wire.requestFrom(DS3231, 7);
  if (Wire.available() == 7) {
    *s = bcdToDec(Wire.read() & 0x7f);
    *m = bcdToDec(Wire.read());
    *h = bcdToDec(Wire.read() & 0x3f);
    *wd = bcdToDec(Wire.read());
    *d = bcdToDec(Wire.read());
    *mo = bcdToDec(Wire.read());
    *y = bcdToDec(Wire.read()); }
  Wire.endTransmission();}

// get the temperature from DS3231 (I2C)
float get_temp_DS3231() {
  byte TH,TL;
  short T;
  Wire.beginTransmission(DS3231);
  Wire.write(0x11); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.beginTransmission(DS3231);
  Wire.requestFrom(DS3231, 2);
  if (Wire.available() == 2) {
    TH = Wire.read();
    TL = Wire.read();}
  Wire.endTransmission();
  T = TH << 8;
  T |= TL;
  return T/256.0; }

// set the time for DS3231 (I2C)
void set_time(byte s, byte m, byte h, byte wd, byte d, byte mo, byte y) {
  Wire.beginTransmission(DS3231);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(s);  // set seconds
  Wire.write(m);  // set minutes
  Wire.write(h);  // set hours
  Wire.write(wd); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(d);  // set date (1 to 31)
  Wire.write(mo); // set month
  Wire.write(y);  // set year (0 to 99)
  Wire.endTransmission(); }    */
