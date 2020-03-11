/*
 *  OpenGarden sensor platform for Arduino Yún/YúnShield.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see http://www.gnu.org/licenses/.
 *
 *  Version:           3.1
 *  Implementation:    Federico Pietro Briata, Torino June 2019
 *
 *  The original time parser code come from TimeCheck example of Tom Igoe but the code was taken from ebolisa (TIA) that added a
 *  parser also for get the date https://forum.arduino.cc/index.php?topic=366614.msg2527019#msg2527019
 *  The logic of main loop is inspired on "Sensors Indoor and Outdoor" example from Victor Boria, Luis Martin & Jorge Casanova (Cooking-hacks).
 *  
 *  Pinout
 *  ATMEL ATMEGA328P / ARDUINO UNO / Open Garden  Shield  ||  ATMEL ATMEGA32U4 / ARDUINO Yún
 *  
 *  Digital Pins
 *  -------------
 *  PIN0                           Not used (UART)               PIN0
 *  PIN1                           Not used (UART)               PIN1
 *  PIN2                           RF Transceiver (IRQ)          PIN2
 *  PIN3                           RF Power Strip Data In/Out    Not tested
 *  PIN4                           DHT22 Data In                 PIN4
 *  PIN5                           DS18B20 Data In               Not tested
 *  PIN6                           Irrigation 1(PWM)             PIN6
 *  PIN7                           Irrigation 2                  PIN7
 *  PIN8                           Vcc Sensors                   PIN8
 *  PIN9                           Irrigation 3(PWM)             PIN9
 *  PIN10                          RF Transceiver (SPI-SS)       PIN10
 *  Not exist                      Free                          PIN12
 *  Not exist                      Free                          PIN13
 *  PIN11                          RF Transceiver (SPI-SDI)      PIN16/ICSP-4
 *  PIN12                          RF Transceiver (SPI-SDO)      PIN14/ICSP-1
 *  PIN13                          RF Transceiver (SPI-SCK)      PIN15/ICSP-3
 *  Not exist                      Irrigation 4                  PIN17/RXLED
 *  
 *  Analog Pins
 *  -----------
 *  ANALOG0                        EC sensor                     ANALOG0
 *  ANALOG1                        pH sensor                     ANALOG1
 *  ANALOG2                        Soil moisture sensor          ANALOG2
 *  ANALOG3                        LDR sensor                    ANALOG3
 *  ANALOG4                        RTC (I2C-SDA)                 Not usable
 *  ANALOG5                        RTC (I2C-SCL)                 Not usable
 *  Not exist                      Free                          ANALOG4
 *  Not exist                      Free                          ANALOG5
 */

#include <Process.h>
#include <Console.h>
//#include <Bridge.h>
#include <OpenGarden.h>
//#include <Wire.h>
#include <HttpClient.h>

#define SEND_EMONDATA
//#define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEBUG_PRINT(x) Console.print(x)       // open Console via Network Bridge
#define DEBUG_PRINTLN(x) Console.println(x)
//#define DEBUG_PRINT(x) Serial.print(x)      // open Serial via USB-Serial
//#define DEBUG_PRINTLN(x) Serial.println(x)
//#define DEBUG_PRINT(x) Serial1.print(x)     // open Serial1 to Linux /dev/ttyATH0
//#define DEBUG_PRINTLN(x) Serial1.println(x) // Do not use it when Bridge is enabled
//#define DEBUG_WIFI
#define DEBUG_NTP
#define DEBUG_DATA
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

Process date;                 // process used to get the datetime
String dayOfWeek;
int days, months, years, hours, minutes, seconds;  // for the results
int lastSecond = -1;          // need an impossible value for comparison

// Calibratie EC
//#define point_1_cond 40000 // Write here your EC calibration value of the solution 1 in µS/cm
//#define point_1_cal 40 // Write here your EC value measured in resistance with solution 1
//#define point_2_cond 10500 // Write here your EC calibration value of the solution 2 in µS/cm
//#define point_2_cal 120 // Write here your EC value measured in resistance with solution 2

// Calibratie pH
//#define calibration_point_4 2246 //Write here your measured value in mV of pH 4
//#define calibration_point_7 2080 //Write here your measured value in mV of pH 7
//#define calibration_point_10 1894 //Write here your measured value in mV of pH 10

HttpClient www;
String updateURL;

void getDateTime() {
    if (!date.running()) {
        date.begin("date");
        date.addParameter("+%d/%m/%Y-%a-%T");
        date.run();
    }
}

void setDateTime() {
    //if there's a result from the date process, parse it:
    while (date.available() > 0) {

        // get the result of the date process (should be hh:mm:ss):
        String timeString = date.readString();
        timeString.trim();

        // find the slashes /
        int firstSlash = timeString.indexOf("/");
        int secondSlash = timeString.lastIndexOf("/");

        // find the dashes -
        int firstDash = timeString.indexOf("-");
        int secondDash = timeString.lastIndexOf("-");

        // find the colons:
        int firstColon = timeString.indexOf(":");
        int secondColon = timeString.lastIndexOf(":");

        // get the substrings for date and time
        String dayString = timeString.substring(0, firstSlash);
        String monthString = timeString.substring(firstSlash + 1, secondSlash);
        String yearString = timeString.substring(secondSlash + 1, firstDash);
        dayOfWeek = timeString.substring(firstDash + 1, secondDash);
        String hourString = timeString.substring(secondDash + 1, firstColon);
        String minString = timeString.substring(firstColon + 1, secondColon);
        String secString = timeString.substring(secondColon + 1);

        // convert to ints
        days = dayString.toInt();
        months = monthString.toInt();
        years = yearString.toInt();
        hours = hourString.toInt();
        minutes = minString.toInt();

        // saving the previous seconds to do a time comparison
        lastSecond = seconds;
        seconds = secString.toInt();
    }
}

void setup() {
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    Bridge.begin();    // Bridge takes about two seconds to start up
#ifdef DEBUG_MODE
    Console.begin();
    //Console.begin(115200);
    //Console.begin(230400);
    //while (!Console); // Wait for Console port to connect
#endif
    DEBUG_PRINTLN("Console OK");

    OpenGarden.initSensors();     //Initialize sensors power
    OpenGarden.sensorPowerON();   //Turn On the sensors
    OpenGarden.initIrrigation(1); //Initializing necessary for irrigation number 1
    //OpenGarden.calibrateEC(point_1_cond,point_1_cal,point_2_cond,point_2_cal);
    //OpenGarden.calibratepH(calibration_point_4,calibration_point_7,calibration_point_10);
    OpenGarden.initRF();
    OpenGarden.initIrrigation(1); //Initialize irrigation 1
    //OpenGarden.initIrrigation(2); //Initialize irrigation 2
    //OpenGarden.initIrrigation(3); //Initialize irrigation 3

    // run an initial date process. Should return:
    // m/d/yyyy - hh:mm:ss European format
    getDateTime();
    setDateTime();
    delay(250);
    DEBUG_PRINTLN("Setup done");
    digitalWrite(13, HIGH);  // tun on on-board LED when Setup has done
}

void loop() {

#ifdef DEBUG_WIFI
    DEBUG_PRINTLN();
    DEBUG_PRINT("->Wifi ");
    Process wifiCheck;  // initialize a new process

    wifiCheck.runShellCommand("/usr/bin/pretty-wifi-info.lua | grep Signal");  // command you want to run

    // while there's any characters coming back from the
    // process, print them to the serial monitor:
    while (wifiCheck.available() > 0) {
        char c = wifiCheck.read();
        DEBUG_PRINT(c);
    }
    Console.flush();
    DEBUG_PRINTLN();
#endif

    // if a second has passed
    if (lastSecond != seconds) {
        DEBUG_PRINTLN();
        DEBUG_PRINT("->a second has passed");
#ifdef DEBUG_NTP
        // display date and time
        DEBUG_PRINTLN();
        DEBUG_PRINT("Date and time from NTP: "); //as separate variables
        DEBUG_PRINT(days);
        DEBUG_PRINT("/");
        DEBUG_PRINT(months);
        DEBUG_PRINT("/");
        DEBUG_PRINT(years);
        DEBUG_PRINT("-");
        DEBUG_PRINT(dayOfWeek);
        DEBUG_PRINT("-");
        if (hours <= 9) {
            DEBUG_PRINT("0");  // adjust for 0-9
        }
        DEBUG_PRINT(hours);
        DEBUG_PRINT(":");
        if (minutes <= 9) {
            DEBUG_PRINT("0");  // adjust for 0-9
        }
        DEBUG_PRINT(minutes);
        DEBUG_PRINT(":");
        if (seconds <= 9) {
            DEBUG_PRINT("0");  // adjust for 0-9
        }
        DEBUG_PRINTLN(seconds);
#endif
        //Only enter 1 time each minute (Yun, with this setup and debug enabled, take ~1.5s maximum to back here)
        if (((seconds == 0) || (seconds == 1)) || ((seconds == 15) || (seconds == 16)) || ((seconds == 30) || (seconds == 31)) || ((seconds == 45) || (seconds == 46))) {

            //Receive data from nodes
            OpenGarden.receiveFromNode();

            int soilMoisture0 = 0;
            float airTemperature0 = 0;
            float airHumidity0 = 0;
            int luminosity0 = 0;

            //Get Gateway Sensors
            soilMoisture0 = OpenGarden.readSoilMoisture();
            airTemperature0 = OpenGarden.readAirTemperature();
            airHumidity0 = OpenGarden.readAirHumidity();
            //float soilTemperature0 = OpenGarden.readSoilTemperature();
            luminosity0 = OpenGarden.readLuminosity();
            //float resistanceEC = OpenGarden.readResistanceEC(); //EC Value in resistance
            //float EC = OpenGarden.ECConversion(resistanceEC); //EC Value in µS/cm
            //int mvpH = OpenGarden.readpH(); //Value in mV of pH
            //float pH = OpenGarden.pHConversion(mvpH); //Calculate pH value

            //Get the last data received from node1
            Payload node1Packet = OpenGarden.getNodeData(node1);

            int soilMoisture1 = 0;
            float airTemperature1 = 0;
            float airHumidity1 = 0;
            int luminosity1 = 0;
            int battery1 = 0;

            soilMoisture1 = node1Packet.moisture;
            airTemperature1 = node1Packet.temperature;
            airHumidity1 = node1Packet.humidity;
            luminosity1 = node1Packet.light;
            battery1 = node1Packet.supplyV;

#ifdef DEBUG_DATA
            DEBUG_PRINTLN("***************************");
            DEBUG_PRINTLN("Gateway");
            DEBUG_PRINT("Water Moisture: ");
            DEBUG_PRINTLN(soilMoisture0);
            DEBUG_PRINT("Air Temperature: ");
            DEBUG_PRINT(airTemperature0);
            DEBUG_PRINTLN(" *C");
            DEBUG_PRINT("Air Humidity: ");
            DEBUG_PRINT(airHumidity0);
            DEBUG_PRINTLN(" % RH");
            //DEBUG_PRINT("Water Temperature: ");
            //DEBUG_PRINT(soilTemperature0);
            //DEBUG_PRINTLN(" *C");
            DEBUG_PRINT("Luminosity: ");
            DEBUG_PRINT(luminosity0);
            DEBUG_PRINTLN(" %");
            //DEBUG_PRINT(F("EC Value in resistance = "));
            //DEBUG_PRINT(resistanceEC);
            //DEBUG_PRINT(F(" // EC Value = "));
            //DEBUG_PRINT(EC);
            //DEBUG_PRINTLN(F(" uS/cm"));
            //DEBUG_PRINT(F("pH Value in mV = "));
            //DEBUG_PRINT(mvpH);
            //DEBUG_PRINT(F(" // pH = "));
            //DEBUG_PRINTLN(pH);

            DEBUG_PRINTLN("*********");
            DEBUG_PRINTLN("Node 1");
            DEBUG_PRINT("Water Moisture: ");
            DEBUG_PRINTLN(soilMoisture1);
            DEBUG_PRINT("Air Temperature: ");
            DEBUG_PRINT(airTemperature1);
            DEBUG_PRINTLN(" *C");
            DEBUG_PRINT("Air Humidity: ");
            DEBUG_PRINT(airHumidity1);
            DEBUG_PRINTLN(" % RH");
            DEBUG_PRINT("Luminosity: ");
            DEBUG_PRINT(luminosity1);
            DEBUG_PRINTLN(" %");
            DEBUG_PRINT("Battery: ");
            DEBUG_PRINT(battery1);
            DEBUG_PRINTLN(" mV");
#endif
            /*
            Created a updateURL string for opengarden web app with this structure: "node_id:sensor_type:value;node_id2:sensor_type2:value2;...."
             Note the ";" between different structures

             Where node_id:
             0 - Gateway
             1 - Node 1
             2 - Node 2
             3 - Node 3

             And where sensor_type:
             0 - Soil moisture
             1 - Soil temperature
             2 - Air Humidity
             3 - Air Temperature
             4 - Light level
             5 - Battery level
             6 - pH level
             7 - Electrical conductivity

             For example: "0:0:56;0:1:17.54;0:2:56.45"
             This means that you send data of the gateway: Soil moisture=56, Soil temperature=17.54 and Air humidity=56.45

             */
            if ((seconds == 0) || (seconds == 1)) {
                DEBUG_PRINT("->Sending data string for opengarden web app...");
                // We now create a URI for the request
                updateURL = "http://my.opengarden.org";
                updateURL += "/set_sensors.php?data=";
                updateURL += "0:0:";
                updateURL += soilMoisture0;
                updateURL += ";0:2:";
                updateURL += airHumidity0;
                //updateURL += ";0:3:";           // NC
                //updateURL += airTemperature0;
                updateURL += ";0:4:";
                updateURL += luminosity0;
                updateURL += ";1:0:";
                updateURL += soilMoisture1;
                updateURL += ";1:2:";
                updateURL += airHumidity1;
                updateURL += ";1:3:";
                updateURL += airTemperature1;
                updateURL += ";1:4:";
                updateURL += luminosity1;
                updateURL += ";1:5:";
                updateURL += battery1;

                DEBUG_PRINTLN(updateURL);
                www.get(updateURL);
                DEBUG_PRINTLN("Status update sent");
                while (www.available()>0) {
                    char c = www.read();
                    DEBUG_PRINT(c);
                }
                Console.flush();
                updateURL ="";
                www.close();

                DEBUG_PRINTLN();
                DEBUG_PRINTLN("closing HTTP connection");
                DEBUG_PRINT("->Done");
            }
            else if ((seconds == 15) || (seconds == 16)) {
                DEBUG_PRINT("->Get actuators data from opengarden web app");

                // We now create a URI for the request
                updateURL = "http://my.opengarden.org";
                updateURL += "/get_actuators.php?actuators";

                DEBUG_PRINTLN(updateURL);
                www.get(updateURL);

                // Read incoming bytes from the server
                int cont=0;
                char recv[8];
                while (www.available()>0) {
                    recv[cont] = www.read();
                    cont++;
                }
                recv[cont]='\0';
                updateURL ="";
                www.close();
                DEBUG_PRINTLN();

                DEBUG_PRINTLN("closing HTTP connection");

                DEBUG_PRINT("Relay setup Received:");
                DEBUG_PRINTLN(recv);

                DEBUG_PRINT("irrigation1:");
                DEBUG_PRINTLN(recv[5]);
                DEBUG_PRINT("irrigation2:");
                DEBUG_PRINTLN(recv[6]);
                DEBUG_PRINT("irrigation3:");
                DEBUG_PRINTLN(recv[7]);

                DEBUG_PRINTLN();

                /*
                 *  Warter moisture values
                 *  Higer value means ground weet, lower values means ground dry.
                 *  my sensor value when dry, out of moisture: ~150
                 *  super dry moisture: 444
                 *  just wet moisture: 456
                 *  super wet moisture: 597
                 *  those value migth be different on different setup and enviroment
                */

                // NOTE:  48 in ASCII it's 0 & 49 it's 1, we don't have enough memory to allocate another string
                // Turn On Irrigation 1 for a minute at 9.30 PM every days when check of soil moisture falls under value: 430 or if Actuator 1 is On
                if (((soilMoisture1 != 0) && (soilMoisture1 < 430 ) && ((hours==6) && (minutes>10) && (minutes<13))) || (recv[5] == 49)) {

                // Turn On Irrigation 1 for a minute every Monday, Wednesday and Saturday or if Actuator 1 is On
                //if (((hours==23) && (minutes==0) && ((dayOfWeek = "Mon") || (dayOfWeek = "Wed") || (dayOfWeek = "Sat"))) || (recv[5] == 49)) {

                // Turn On Irrigation 1 for a minute every days or if Actuator 1 is On
                //if (((hours==6) && (minutes==0)) || (recv[5] == 49)) {

                // Turn On Irrigation 1 if Actuator 1 is On
                //if (recv[5] == 49) {

                    DEBUG_PRINT("->Relay.. ");
                    OpenGarden.irrigationON(1);
                    //    OpenGarden.irrigationON(2);
                    //    OpenGarden.irrigationON(3);
                    //    digitalWrite(12,HIGH); // irrigationON(4)
                    //    digitalWrite(13,HIGH); // irrigationON(5)
                    //    digitalWrite(17,HIGH); // irrigationON(17) on RXLED
                    DEBUG_PRINT("->ON");
                    Console.flush();
                }
                else {
                    DEBUG_PRINT("->Relay.. ");
                    OpenGarden.irrigationOFF(1);
                    //    OpenGarden.irrigationOFF(2);
                    //    OpenGarden.irrigationOFF(3);
                    //    digitalWrite(12,LOW); // irrigationON(4)
                    //    digitalWrite(13,LOW); // irrigationON(5)
                    //    digitalWrite(17,LOW); // irrigationON(17) on RXLED
                    DEBUG_PRINTLN("->OFF");
                }
                DEBUG_PRINT("->Done");
                Console.flush();
            }
#ifdef SEND_EMONDATA
            else if ((seconds == 30) || (seconds == 31)) {
                DEBUG_PRINT("->Sending data from opengarden gateway for emoncms web app...");

                // We now create a URI for the request
                updateURL = "http://my.emoncms.org";
                updateURL += "/input/post.json?node=1";
                updateURL += "&apikey=fffffffffffffffffffffffffffff";
                updateURL += "&json={";
                updateURL += "watermoisture0:";
                updateURL += soilMoisture0;
                //updateURL += ",airtemperature0:";
                //updateURL += airTemperature0;
                updateURL += ",airhumidity0:";
                updateURL += airHumidity0;
                updateURL += ",luminosity0:";
                updateURL += luminosity0;
                updateURL += "}";

                DEBUG_PRINTLN(updateURL);
                www.get(updateURL);
                DEBUG_PRINTLN("Status update sent");
                while (www.available()>0) {
                    char c = www.read();
                    DEBUG_PRINT(c);
                }
                Console.flush();
                updateURL ="";
                www.close();

                DEBUG_PRINTLN();
                DEBUG_PRINTLN("closing HTTP connection");
                DEBUG_PRINT("->Done");
            }
            else if ((seconds == 45) || (seconds == 46)) {
                DEBUG_PRINT("->Sending data from opengarden node1 for emoncms web app...");

                // We now create a URI for the request
                updateURL = "http://tempo.briata.org";
                updateURL += "/input/post.json?node=1";
                updateURL += "&apikey=fffffffffffffffffffffffffffff";
                updateURL += "&json={";
                updateURL += "watermoisture1:";
                updateURL += soilMoisture1;
                updateURL += ",airtemperature1:";
                updateURL += airTemperature1;
                updateURL += ",airhumidity1:";
                updateURL += airHumidity1;
                updateURL += ",luminosity1:";
                updateURL += luminosity1;
                updateURL += ",battery1:";
                updateURL += battery1;
                updateURL += "}";

                DEBUG_PRINTLN(updateURL);
                www.get(updateURL);
                DEBUG_PRINTLN("Status update sent");
                while (www.available()>0) {
                    char c = www.read();
                    DEBUG_PRINT(c);
                }
                Console.flush();
                updateURL ="";
                www.close();

                DEBUG_PRINTLN();
                DEBUG_PRINTLN("closing HTTP connection");
                DEBUG_PRINT("->Done");
            }
#endif
        }
        //Get next date and time
        getDateTime();
        DEBUG_PRINT("->Get next date and time");
    }
    //Set new date and time
    setDateTime();
    //delay(5000);   //Wait 5 seconds
    DEBUG_PRINT("->Set new date and time");
    Console.flush();
}
