/*  
 *  OpenGarden sensor platform for Arduino from Cooking-hacks.
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
 *  Version:           3.0
 *  Design:            David Gascón 
 *  Implementation:    Victor Boria & Jorge Casanova
 */

#include <OpenGarden.h>
#include "Wire.h" 

//Enter here your data
const char server[] = "http://YOUR_SERVER_IP";
const char server_port[] = "YOUR_SERVER_PORT";
const char wifi_ssid[] = "YOUR_WIFI_SSID";
const char wifi_password[] = "YOUR_WIFI_PASSWORD";
const char GET[] = "opengarden/get_actuators.php?actuators\r";

char recv[512];
int cont;

char irrigation1_wf;
char irrigation2_wf;
char irrigation3_wf;

int8_t answer;
char response[300];
char aux_str[90];

#define DATA_BUFFER      1024
String inputLine = ""; // a string to hold incoming line

void setup() {
  Serial.begin(115200);
  OpenGarden.initIrrigation(1); //Initialize irrigation number 1 
  OpenGarden.initIrrigation(2); //Initialize irrigation number 2
  OpenGarden.initIrrigation(3); //Initialize irrigation number 3
  OpenGarden.irrigationOFF(1);
  OpenGarden.irrigationOFF(2);
  OpenGarden.irrigationOFF(3);
  cleanVector();
  wifireset();
  wificonfig(); 
}


void loop() {
  getActuators();

  Serial.println();
  if (irrigation1_wf == '0'){
    OpenGarden.irrigationOFF(1); //Turn OFF the irrigation number 1 
    Serial.println("Irrigation 1 OFF");
  }
  else if (irrigation1_wf == '1'){
    OpenGarden.irrigationON(1); //Turn ON the irrigation number 1 
    Serial.println("Irrigation 1 ON");
  }
  else{
    Serial.println("Irrigation 1 BAD DATA");
  }


  if (irrigation2_wf == '0'){
    OpenGarden.irrigationOFF(2); //Turn OFF the irrigation number 2 
    Serial.println("Irrigation 2 OFF");
  }
  else if (irrigation2_wf == '1'){
    OpenGarden.irrigationON(2); //Turn ON the irrigation number 2
    Serial.println("Irrigation 2 ON");  
  }
  else{
    Serial.println("Irrigation 2 BAD DATA");
  }


  if (irrigation3_wf == '0'){
    OpenGarden.irrigationOFF(3); //Turn OFF the irrigation number 3
    Serial.println("Irrigation 3 OFF");
  }
  else if (irrigation3_wf == '1'){
    OpenGarden.irrigationON(3); //Turn ON the irrigation number 3
    Serial.println("Irrigation 3 ON");
  }
  else{
    Serial.println("Irrigation 3 BAD DATA");
  }

  cleanVector();

  delay(5000);
}


//*********************************************************************
//*********************************************************************


void cleanVector(){
  recv[0] = 0; 
  recv[1] = 0;
  recv[2] = 0;
}


//**********************************************
void wifireset() {
  
  Serial.println(F("Setting Wifi parameters"));
  sendCommand("AT+iFD\r","I/OK",2000);
  delay(1000);  
  sendCommand("AT+iDOWN\r","I/OK",2000);
  delay(6000);  
}


//**********************************************
void wificonfig() {
  
  Serial.println(F("Setting Wifi parameters"));
  sendCommand("AT+iFD\r","I/OK",2000);
  delay(2000);  

  snprintf(aux_str, sizeof(aux_str), "AT+iWLSI=%s\r", wifi_ssid);
  answer = sendCommand(aux_str,"I/OK",10000);

  if (answer == 1){

    snprintf(aux_str, sizeof(aux_str), "Joined to \"%s\"", wifi_ssid);
    Serial.println(aux_str);
    delay(5000);
  }

  else {
    snprintf(aux_str, sizeof(aux_str), "Error joining to: \"%s\"", wifi_ssid);
    Serial.println(aux_str);
    delay(1000);
  }

  snprintf(aux_str, sizeof(aux_str), "AT+iWPP0=%s\r", wifi_password);
  sendCommand(aux_str,"I/OK",20000);
  delay(1000);

  if (answer == 1){

    snprintf(aux_str, sizeof(aux_str), "Connected to \"%s\"", wifi_ssid);
    Serial.println(aux_str);
    delay(5000);
  }

  else {
    snprintf(aux_str, sizeof(aux_str), "Error connecting to: \"%s\"", wifi_ssid);
    Serial.println(aux_str);
    delay(1000);
  }

  sendCommand("AT+iDOWN\r","I/OK",2000);
  delay(6000); 


}


//**********************************************

void getActuators(){
  
  snprintf(aux_str, sizeof(aux_str), "AT+iRLNK:\"%s:%s/%s\"\r", server, server_port, GET);
  sendCommand(aux_str,"I/OK",5000); 
  delay(100);
  checkData();
}



//**********************************************
void checkData(){
  cont=0;   
  delay(3000);
  while (Serial.available()>0)
  {
    recv[cont]=Serial.read(); 
    delay(10);
    cont++;
  }
  recv[cont]='\0';

  irrigation1_wf= recv[12]; 
  irrigation2_wf= recv[13];
  irrigation3_wf= recv[14];

  //Serial.print(recv);
  //Serial.println(irrigation1_wf);
  //Serial.println(irrigation2_wf);
  //Serial.println(irrigation3_wf);
 
  delay(100);
}

//**********************************************
int8_t sendCommand(const char* Command, const char* expected_answer, unsigned int timeout){

  uint8_t x=0,  answer=0;
  unsigned long previous;

  memset(response, 0, 300);    // Initialize the string

  delay(100);

  while( Serial.available() > 0) Serial.read();    // Clean the input buffer

  Serial.println(Command);    // Send Command 

    x = 0;
  previous = millis();

  // this loop waits for the answer
  do{
    if(Serial.available() != 0){    
      // if there are data in the UART input buffer, reads it and checks for the asnwer
      response[x] = Serial.read();
      x++;
      // check if the desired answer  is in the response of the module
      if (strstr(response, expected_answer) != NULL)    
      {
        answer = 1;
      }
    }
  }
  // Waits for the asnwer with time out
  while((answer == 0) && ((millis() - previous) < timeout));    

  return answer;
}


//**********************************************
