# SmartyReader: Reading encrypted Luxembourgish smartmeter data

## The Software (MQTT via WiFi) to use with an ESP8266

### Version 1.6 Beta

This new version got a lot of changes:

A new config file (`config.h`) contains all the things that must be changed or are safe to change.

In this new version I changed the publishing format. Every item (DSMR and new calculated values) is published under its own topic. No more JSON strings but directly the values (easier to handle wit home automation software). In `config.h` (or secrets.h) you can decide with 'y/n' if you want to publish the parameter or not.

To be better able to write simpler rules I added calculated values described below.

#### Define lines in the main program

The main program (.ino) contains still the define lines that can be commented or uncommented. Removing the two slashes (`//`) means the line is active (uncommented). To make a line inactive, add two slashes at the beginning of the line. 

+ `USE_SECRETS` will use a file named `secrets.h` instead of the `config.h` file. This file must contain the same things than the config file (`config.h`) but must be located in the sketchbook `libraries` folder of your Arduino IDE  in a folder named `Secrets`. This makes it easier to test and distribute the software without sharing the WiFi password and so compromising security.

+ `OTA` will enable the Over The Air update possibility (security risk!). This comes handy to reprogram your SmartyReader while connected to the Smartmeter. In the config file you can change the name you see in the port menu of the Arduino IDE, and the password. To use a minimum of security we calculate an MD5 hash of our password, so that the password is not contained in cleartext in the Flash of the microcontroller. You can calculate MD5 hashes from a password with online tools.

`OLD_HARDWARE` must be used for boards before V2.0, because here the serial signal is inverted with hardware (transistor) instead of software.

`MQTTPASSWORD` enables an MQTT connection protected with a password (recommended!!). Check the config file to set the password.

`STATIC` can be used if you want a static IP instead of a changing IP. The IP address is set in the config file.

`ETHERNET` if you don't want to use WiFi. For this you must add a Funduino (W5100) breakout board to your SmartyReader.

`BME280_I2C` to add a temperature sensor to your SmartyReader. The data is published under the topic....(not implemented yet)

`PUBLISH_COOKED` is used to get only one JSON string with all the calculated values. In this new version every item (DSMR and calculated) is published under its own topic. No more JSON strings but directly the values (easier to handle wit home automation software). In `config.h` (or secrets.h) you can decide with 'y/n' if you want to publish the parameter or not.

#### The config file (`config.h`). 

Mandatory things to change are your WiFi credentials, your Smartmeter key (get it from your electricity distribution system operator) and the IP address of your MQTT server (e.g. Raspberry Pi).

The following things can be changed:

+ `PUBLISH_TIME`: in ms. The minimum is 10 seconds and it makes sense to use a multiple of this. A good value is 60000, to get data once a minute.

+ `SAMPLE_TIME_MIN`: The mean value of the power and tne min and max values are calculated over this time. If we set this time to 10 min, 60 values are used to calculate the mean power.

+ `MQTT`: The client ID must be unique, so change it if you use more than one SmartyReader!! Choose your own topic. User and Password is only needed if you uncommented `MQTTPASSWORD`. Default port is 1883.

+ `WiFi`: With an MDNS name you can access your SmartyReader with the name .local instead the IP. With a hostname it is easier to find your SmartyReader IP with the router or nmap. If you use UDP to debug (look below) you can change here the IP address of your client and the port. You can also change the NTP server and your time zone.

+ `OTA`: If you use OTA (uncomment `OTA` in the main file (.ino)). 

+ `STATIC`: If you use a static IP address (uncomment `STATIC` in the main file (.ino)). 

+ `ETHERNET`: If you use an ethernet breakout board (uncomment `ETHERNET` in the main file (.ino)) you can change the MAC to get a unique address.

+ `AUTH_DATA`: In Austria this data is not hard coded, so here you can change it (17 byte!).

+ `PUBLISH`: In the two following tables you can define if a parameter is published or not. To prevent publishing, repalce the lowercase 'y' with another char e.g. 'n'.

#### Debugging

Debugging options are enabled in the main file (.ino) in the `setup()` function. By default all 3 logging options are enabled (true). 

+ `Tb.set_led_log(true)` uses the built-in LED for debugging (different blink patterns).
+ `Tb.set_serial_log(true,1)` uses the second (1 = Serial1) or third (ESP32, (2 = Serial2)) hardware serial interface to debug. The first (Serial or Serial0) is used by the Smartmeter and can't be used to debug. You can connect a Serial to USB adapter to this hardware interface pins (D4 (Tx1) and GND for ESP8266) and use a terminal program to log the stream.

+ `Tb.set_udp_log(true, UDP_LOG_PC_IP, UDP_LOG_PORT)` is the coolest option. Use a computer to get the logging over an UDP stream. On a Linux system (e.g. Raspberry Pi) you can use the `netcat` command (nc) in a terminal: `nc -kulw 0 6666`. The port and IP address are set in `config.h` under "WiFi and network settings".

To add your own debug strings use `Tb.log("mytext")`or `Tb.log_ln("mytext")`.

#### The calculated values

##### The smartmeter is a balancing counter

Smartmeter are balancing counter (german: saldierender Zähler). This can lead to confusion when looking at the power values from your Smartmeter.

###### A little example:

If your solar plant is giving you 6&#8239;kW on 3 phases (L1, L2, L3), you get 2&#8239;kW per phase. If you are charging your EV at the same time with 4&#8239;kW on phase L1, the SmartyReader will show you the following values:

`act_pwr_imported_p_plus` (1.7.0) = 2&#8239;kW
`act_pwr_exported_p_minus` (2.7.0) = 4&#8239;kW
`act_pwr_imp_p_plus_l1` (21.7.0) = 2&#8239;kW
`act_pwr_imp_p_plus_l2"` (41.7.0) = 0&#8239;kW
`act_pwr_imp_p_plus_l3"` (61.7.0) = 0&#8239;kW
`act_pwr_exp_p_minus_l1` (22.7.0) = 0&#8239;kW
`act_pwr_exp_p_minus_l2` (42.7.0) = 2&#8239;kW
`act_pwr_exp_p_minus_l3` (62.7.0) = 2&#8239;kW

But if we calculate the power from the energy values we see that in fact there is no energy imported and 2&#8239;kW are exported.

To calculate the power from energy we need two energy values (E1 and E2) and the time between these two values (&#916;t): P = (E2-E1)/&#916;t.

Another solution is to calculate the difference between `act_pwr_imported_p_plus` (1.7.0) and `act_pwr_exported_p_minus` (2.7.0). 
P = power exported - power imported. If this difference is positive we get an excess of solar power, and energy is only exported (no energy is imported). If it is negative, we consume energy.

###### SmartyReader calculated values

To get a better overview, the new software is calculating the solar excess power, and the power from energy.

It calculates also the mean power over the last x minutes (x changeable in `config.h`), as the min and max values over the last x minutes. For this all available values (10 second values) are used. With these values it is easier to write rules for days with fast changing solar radiation.


## More infos on: <http://weigu.lu/microcontroller/smartyReader/index.html>

