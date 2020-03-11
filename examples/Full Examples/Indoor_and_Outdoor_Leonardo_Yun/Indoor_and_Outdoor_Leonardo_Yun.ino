/*
 *  OpenGarden sensor platform for Arduino Leonardo & Arduino Yún.
 *
 *  Copyright (C) Libelium Comunicaciones Distribuidas S.L. 
 *  http://www.libelium.com 
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
 *  Version:           2.6 
 *  Design:            David Gascón 
 *  Implementation:    Victor Boria, Luis Martin, Jorge Casanova & Federico Pietro Briata
 *  
 *  Pinout
 *  ATMEL ATMEGA328P / ARDUINO UNO / Open Garden  Shield  ||  ATMEL ATMEGA32U4 / ARDUINO Leonardo / ARDUINO Yún
 *  
 *  Digital Pins
 *  -------------
 *  PIN0                           Not used (UART)               PIN0
 *  PIN1                           Not used (UART)               PIN1
 *  PIN2                           RF Transceiver (IRQ)          PIN7
 *  PIN3                           RF Power Strip Data In/Out    Not tested
 *  PIN4                           DHT22 Data In                 PIN4
 *  PIN5                           DS18B20 Data In               Not tested
 *  PIN6                           Irrigation 1(PWM)             PIN6
 *  PIN7                           Irrigation 2                  PIN11
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
 *  ANALOG4                        RTC (I2C-SDA)                 PIN2/SDA
 *  ANALOG5                        RTC (I2C-SCL)                 PIN3/SCL
 *  Not exist                      Free                          ANALOG4
 *  Not exist                      Free                          ANALOG5
 */

#include <Process.h>
#include <OpenGarden.h>

#define DEBUG_MODE
#ifdef DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.print(x)      // open Serial via USB-Serial
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_DATA
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

DateTime myTime;
int flag = 0; // auxiliar variable


void setup() {
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    Serial.begin(115200);      // open Serial connection via USB-Serial
    DEBUG_PRINTLN("Serial OK");

    OpenGarden.initSensors();     //Initialize sensors power
    OpenGarden.sensorPowerON();   //Turn On the sensors
    OpenGarden.initIrrigation(1); //Initializing necessary for irrigation number 1

    OpenGarden.initRF();
    OpenGarden.initRTC();
    OpenGarden.initIrrigation(1); //Initialize irrigation 1
    OpenGarden.initIrrigation(2); //Initialize irrigation 2
    OpenGarden.initIrrigation(3); //Initialize irrigation 3
    DEBUG_PRINTLN("Setup done");
    digitalWrite(13, HIGH);  // tun on on-board LED when Setup has done
}

void loop() {
    //Get date and time
    myTime = OpenGarden.getTime();

    //Receive data from node
    OpenGarden.receiveFromNode();

    //Only enter 1 time each minute (on Yun/Leonardo two secs are needed to back here)
    if (((myTime.second() == 0) || (myTime.second() == 1))  && flag == 0  ) {

    //Get Gateway Sensors
    int soilMoisture0 = OpenGarden.readSoilMoisture();
    float airTemperature0 = OpenGarden.readAirTemperature();
    float airHumidity0 = OpenGarden.readAirHumidity();
    float soilTemperature0 = OpenGarden.readSoilTemperature();
    int luminosity0 = OpenGarden.readLuminosity();

    //Get last data received from Node Sensors
    Payload node1Packet = OpenGarden.getNodeData(node1);
    Payload node2Packet = OpenGarden.getNodeData(node2);
    Payload node3Packet = OpenGarden.getNodeData(node3); 

    int soilMoisture1 = node1Packet.moisture;
    float airTemperature1 = node1Packet.temperature;
    float airHumidity1 = node1Packet.humidity;
    int luminosity1 = node1Packet.light;
    int battery1 = node1Packet.supplyV;

    int soilMoisture2 = node2Packet.moisture;
    float airTemperature2 = node2Packet.temperature;
    float airHumidity2 = node2Packet.humidity;
    int luminosity2 = node2Packet.light;
    int battery2 = node2Packet.supplyV;

    int soilMoisture3 = node3Packet.moisture;
    float airTemperature3 = node3Packet.temperature;
    float airHumidity3 = node3Packet.humidity;
    int luminosity3 = node3Packet.light;
    int battery3 = node3Packet.supplyV;

        //check the moisture, and water twice a week only if the moisture falls under a value of 500
        if ( myTime.dayOfWeek()==1 && soilMoisture1<500 ||  myTime.dayOfWeek()==4 && soilMoisture1<500) {
            OpenGarden.irrigationON(1);
        }

        else {
            OpenGarden.irrigationOFF(1);
        }

        //Turn On Irrigation 2 if node 1 or node 2 soil moisture falls under value: 300
        if( soilMoisture1<300 || soilMoisture2<300){
            OpenGarden.irrigationON(2);
        }
        else{
            OpenGarden.irrigationOFF(2);
        }

        //Turn On Irrigation 3 where a lamp light is connected, if node 1 or node 2 
        //or node 3 luminosity falls under 30%
        if( luminosity1<30 || luminosity2<30 || luminosity3<30 ){
            OpenGarden.irrigationON(3);
        }
        else{
            OpenGarden.irrigationOFF(3);
        }

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
        DEBUG_PRINT("Soil Temperature: ");
        DEBUG_PRINT(soilTemperature0);
        DEBUG_PRINTLN("*C");
        DEBUG_PRINT("Luminosity: ");
        DEBUG_PRINT(luminosity0);
        DEBUG_PRINTLN(" %");

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
        DEBUG_PRINTLN(" V");
        DEBUG_PRINTLN("*********"); 
        DEBUG_PRINTLN("Node 2:");
        OpenGarden.printNode(node2Packet); 
        DEBUG_PRINTLN("*********"); 
        DEBUG_PRINTLN("Node 3:");
        OpenGarden.printNode(node3Packet); 
        DEBUG_PRINTLN("*********"); 
#endif

        flag = 1;
    }
    else if (((myTime.second() == 30) || (myTime.second() == 31)) && flag == 1) {
        flag = 0;
    }
}
