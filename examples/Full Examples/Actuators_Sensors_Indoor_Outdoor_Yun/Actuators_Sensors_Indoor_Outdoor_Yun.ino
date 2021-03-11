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
 *  Version:           3.3
 *  Implementation:    Federico Pietro Briata, Torino June 2019
 *
 *  The original time parser code come from TimeCheck example of Tom Igoe but the code was taken from ebolisa (TIA) that added a
 *  parser also for get the date https://forum.arduino.cc/index.php?topic=366614.msg2527019#msg2527019
 *  The logic of main loop is inspired on "Sensors Indoor and Outdoor" example from Victor Boria, Luis Martin & Jorge Casanova (Cooking-hacks).
 *
 *  About Bistable Valve connection use two relays for Straight and Reverse Polarity
 *  Bistable valves have ground in common and use other two relays for control each Bistable Valves
 *  See https://www.logicaprogrammabile.it/come-pilotare-elettrovalvola-bistabile-usando-2-rele/amp/
 *  
 *  Pinout
 *  ARDUINO UNO          ||     Open Garden  Shield     ||     ARDUINO Yún/YúnShield
 *  
 *  Digital Strip
 *  -------------
 *  PIN0                           Not used (UART)               PIN0
 *  PIN1                           Not used (UART)               PIN1
 *  PIN2                           RF Transceiver (IRQ)          PIN2
 *  PIN3                           RF Power Strip Data In/Out    Not tested
 *  PIN4                           DHT22 Data In                 PIN4
 *  PIN5                           DS18B20 Data In               Not tested
 *  PIN6                           Actuator 1(PWM)               PIN6
 *  PIN7                           Actuator 1                    PIN7
 *  PIN8                           Vcc Sensors                   PIN8
 *  PIN9                           Actuator 3(PWM)               PIN9
 *  PIN10                          RF Transceiver (SPI-SS)       PIN10
 *  Not exist                      Free                          PIN11
 *  Not exist                      Free                          PIN12/ANALOG11
 *  Not exist                      Free                          PIN13/LED13
 *  PIN11                          RF Transceiver (SPI-SDI)      PIN16/ICSP-4
 *  PIN12                          RF Transceiver (SPI-SDO)      PIN14/ICSP-1
 *  PIN13                          RF Transceiver (SPI-SCK)      PIN15/ICSP-3
 *  Not exist                      Free                          PIN17/RXLED
 *  
 *  Analog Strip
 *  -----------
 *  ANALOG0                        EC sensor                     ANALOG0
 *  ANALOG1                        pH sensor                     ANALOG1
 *  ANALOG2                        Soil moisture sensor          ANALOG2
 *  ANALOG3                        LDR sensor                    ANALOG3
 *  ANALOG4                        RTC (I2C-SDA)                 Not usable
 *  ANALOG5                        RTC (I2C-SCL)                 Not usable
 *  Not exist                      Actuator 2                    ANALOG4/PIN22
 *  Not exist                      Actuator 2                    ANALOG5/PIN23
 */

#include <Process.h>
#include <Console.h>
//#include <Bridge.h>
#include <OpenGarden.h>
//#include <Wire.h>
#include <HttpClient.h>

//#define DEBUG_MODE

#ifdef DEBUG_MODE                               // Open Serial Console for debug
  #define DEBUG_PRINT(x) Console.print(x)       // Console via Network Bridge
  #define DEBUG_PRINTLN(x) Console.println(x)
  //#define DEBUG_PRINT(x) Serial.print(x)      // Console via USB-Serial
  //#define DEBUG_PRINTLN(x) Serial.println(x)
  //#define DEBUG_PRINT(x) Serial1.print(x)     // Console via Serial1 to Linux /dev/ttyATH0, do not use it when Bridge is enabled
  //#define DEBUG_PRINTLN(x) Serial1.println(x)
  //#define DEBUG_WIFI                          // Print Wifi Signal Strength
  #define DEBUG_NTP                             // Print Time
  #define DEBUG_DATA                            // Print Data collected
#else
  #define SEND_EMONDATA                         // Send Data over the Emon Server
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

int SWITCH_VALVE   =     0;                     // Keep memory of the Valve that need to turn OFF

Process date;                 // process used to get the datetime
char dayOfWeek[4];
int days, months, years, hours, minutes, seconds;  // for the results
int lastSecond = -1;          // need an impossible value for comparison

// Calibrate EC
//#define point_1_cond 40000 // Write here your EC calibration value of the solution 1 in µS/cm
//#define point_1_cal 40 // Write here your EC value measured in resistance with solution 1
//#define point_2_cond 10500 // Write here your EC calibration value of the solution 2 in µS/cm
//#define point_2_cal 120 // Write here your EC value measured in resistance with solution 2

// Calibrate pH
//#define calibration_point_4 2246 //Write here your measured value in mV of pH 4
//#define calibration_point_7 2080 //Write here your measured value in mV of pH 7
//#define calibration_point_10 1894 //Write here your measured value in mV of pH 10

HttpClient www;
String updateURL;

void setup() {
    pinMode(6, OUTPUT);           //Initialize pin for DEV+
    digitalWrite(6, LOW);
    pinMode(7, OUTPUT);           //Initialize pin for DEV-
    digitalWrite(7, LOW);
    pinMode(22, OUTPUT);          //Initialize pin for EV-1
    digitalWrite(22, LOW);
    pinMode(23, OUTPUT);          //Initialize pin for EV-2
    digitalWrite(23, LOW);
    OpenGarden.initIrrigation(3); //Initialize Actuator 3

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
    //OpenGarden.calibrateEC(point_1_cond,point_1_cal,point_2_cond,point_2_cal);
    //OpenGarden.calibratepH(calibration_point_4,calibration_point_7,calibration_point_10);
    OpenGarden.initRF();

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
        if (((seconds == 0) || (seconds == 1)) || ((seconds == 15) || (seconds == 16)) || ((seconds == 40) || (seconds == 41)) || ((seconds == 55) || (seconds == 56))) {

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
             Means that you send data of the gateway: Soil moisture=56, Soil temperature=17.54 and Air humidity=56.45

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
                updateURL += ";0:3:";
                updateURL += airTemperature0;
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

                DEBUG_PRINT("Actuator 1: ");
                DEBUG_PRINTLN(recv[5]);
                DEBUG_PRINT("Actuator 2: ");
                DEBUG_PRINTLN(recv[6]);
                DEBUG_PRINT("Actuator 3: ");
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

                // Turn On Actuator 1 when is On
                //if (recv[5] == '1') {

                // Turn On Actuator 1 for a minute every week at Sunday OR if Actuator 1 is On
                //if (((hours==23) && (minutes==0) && (strcmp(dayOfWeek, "Sun") == 0)) || (recv[5] == '1')) {

                // Turn On Actuator 1 for a minute every week at Sunday when soil moisture falls under value: 430 OR if Actuator 1 is On
                //if (((soilMoisture1 != 0) && (soilMoisture1 < 430 ) && (hours==23) && (minutes==0) && (strcmp(dayOfWeek, "Sun") == 0)) || (recv[5] == '1')) {

                // Turn On Actuator 1 for a minute at 6.00 AM every days when soil moisture falls under value: 430 OR if Actuator 1 is On
                //if (((soilMoisture1 != 0) && (soilMoisture1 < 430 ) && ((hours==6) && (minutes==0))) || (recv[5] == '1')) {

                // Turn On Actuator 1 for 3 minutes at 11.06 PM every Wednesday and Sunday OR if Actuator 1 is On
                //if (((hours==23) && (minutes>5) && (minutes<9)) && ((strcmp(dayOfWeek, "Wed") == 0) || (strcmp(dayOfWeek, "Sun") == 0)) || (recv[5] == '1')) {

                // Turn On Actuator 1 for 2 minutes every Monday, Wednesday and Saturday OR if Actuator 1 is On
                if (((hours==23) && (minutes>5) && (minutes<8)) && ((strcmp(dayOfWeek, "Mon") == 0) || (strcmp(dayOfWeek, "Wed") == 0) || (strcmp(dayOfWeek, "Sat") == 0)) || (recv[5] == '1')) {

                // Turn On Actuator 1 for 2 minutes at 6.06 AM and 8.05 PM every days OR if Actuator 1 is On
                //if (((hours==6) && (minutes>5) && (minutes<8)) || ((hours==20) && (minutes>5) && (minutes<8)) || (recv[5] == '1')) {

                    DEBUG_PRINT("Actuator 1 ");
                    valvePwrON(1);
                    valvePolarity(0);
                    DEBUG_PRINTLN("->Open ");
                    SWITCH_VALVE = 1;
                    //delay(180000);   //Wait 3Min
                    //valvePolarity(1);
                    //valvePwrOFF();
                    //DEBUG_PRINTLN("->Close ");

                }
                else {
                      if(SWITCH_VALVE == 1) {
                          DEBUG_PRINT("Actuator 1 ");
                          valvePolarity(1);
                          valvePwrOFF();
                          DEBUG_PRINTLN("->Close ");
                          SWITCH_VALVE = 0;
                      }
                }

                //if (recv[6] == 49) {
                if (((soilMoisture0 < 475 ) && ((hours==22) && (minutes==0))) || (recv[6] == '1')) {

                    DEBUG_PRINT("Actuator 2 ");
                    valvePwrON(2);
                    valvePolarity(0);
                    DEBUG_PRINTLN("->Open ");
                    //SWITCH_VALVE = 2;
                    delay(10000);   //Wait 10sec/1lt
                    valvePolarity(1);
                    valvePwrOFF();
                    DEBUG_PRINTLN("->Close ");

                }
                //else {
                      //if(SWITCH_VALVE == 2) {
                          //DEBUG_PRINT("Actuator 2 ");
                          //valvePolarity(1);
                          //valvePwrOFF();
                          //DEBUG_PRINTLN("->Close ");
                          //SWITCH_VALVE = 0;
                      //}
                //}

                if (((hours > 1) && (hours < 22)) || (recv[7] == '1')) {
                    DEBUG_PRINT("Actuator 3 ");
                    OpenGarden.irrigationON(3);
                    DEBUG_PRINTLN("->Open");
                }
                else {
                    DEBUG_PRINT("Actuator 3 ");
                    OpenGarden.irrigationOFF(3);
                    DEBUG_PRINTLN("->Close");
                }
                DEBUG_PRINTLN("->Done");
                Console.flush();
            }
#ifdef SEND_EMONDATA
            else if ((seconds == 40) || (seconds == 41)) {
                DEBUG_PRINT("->Sending data from opengarden gateway for emoncms web app...");

                // We now create a URI for the request
                updateURL = "http://my.emoncms.org";
                updateURL += "/input/post.json?node=1";
                updateURL += "&apikey=fffffffffffffffffffffffffffff";
                updateURL += "&json={";
                updateURL += "watermoisture0:";
                updateURL += soilMoisture0;
                updateURL += ",airtemperature0:";
                updateURL += airTemperature0;
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
            else if ((seconds == 55) || (seconds == 56)) {
                DEBUG_PRINT("->Sending data from opengarden node1 for emoncms web app...");

                // We now create a URI for the request
                updateURL = "http://my.emoncms.org";
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
        String dayOfWeekString = timeString.substring(firstDash + 1, secondDash);
        String hourString = timeString.substring(secondDash + 1, firstColon);
        String minString = timeString.substring(firstColon + 1, secondColon);
        String secString = timeString.substring(secondColon + 1);

        // convert to ints
        days = dayString.toInt();
        months = monthString.toInt();
        years = yearString.toInt();
        dayOfWeekString.toCharArray(dayOfWeek,4);
        hours = hourString.toInt();
        minutes = minString.toInt();

        // saving the previous seconds to do a time comparison
        lastSecond = seconds;
        seconds = secString.toInt();
    }
}

//Solenoid ON
void valvePwrON(int out) {
  if (out == 1) {           //Turn ON by closing the circuit of Valve 1
    digitalWrite(22, HIGH);
    digitalWrite(23, LOW);  // (1 - 0)
    delay(50);              // Wait 50msec
  }
  if (out == 2) {           // Turn ON by closing the circuit of Valve 2
    digitalWrite(22, LOW);
    digitalWrite(23, HIGH); // (0 - 1)
    delay(50);              // Wait 50msec
  }
}

//Solenoid OFF
void valvePwrOFF() {        // Turn OFF All Valves by opening the circuit
  digitalWrite(22, LOW);
  digitalWrite(23, LOW);    // (0 - 0) 
  delay(50);                // Wait 50msec
}

//Solenoid Polarity  NOTE: This function it's needed only for Bistable Valve, so not needed for Monostable Valve
void valvePolarity(int out) {
  if (out == 0) {           // Straight Polarity
    digitalWrite(6, LOW);
    digitalWrite(7, HIGH);  // Opening (0 - 1) 
    delay(50);              // Wait 50msec
    digitalWrite(6, HIGH);
    digitalWrite(7, HIGH);  // OFFH, Open (1 - 1) 
    delay(50);              // Wait 50msec
    digitalWrite(6, HIGH);
    digitalWrite(7, LOW);   // Closing (1 - 0) 
    delay(50);              // Wait 50msec
  }
  if (out == 1) {           // Reverse Polarity
    digitalWrite(6, LOW);
    digitalWrite(7, LOW);   // OFFL, Closed (0 - 0) 
    delay(50);              // Wait 50msec
  }
}
