/* 
This program was written by Dean McGee on and is free for any use

This program determines the amount of remaining filament for a 3D printer.
 It is inspired by the thingiverse make: https://www.thingiverse.com/thing:2798423/files
 3D Printer Filament Scale, by Steven Westerfeld, aka Kisssys.
 The mounting mechanism and load cell are unchanged but everything else is different.
 The hardware required is:
   5kg load cell with HX711 interface
   0.96" IIC Oled display
   WeMos D1 Mini tripler base, standard size, with connector headers
   One-Button Shield for WeMos D1 Mini
   Dupont connector jumper wires
   Follow manufacturer instructions for connecting HX711 to load cell. 
   Soldering is required for Tripler Base and HX711 


The Oled and Load cell can be connected to either 3.3 or 5v
Oled SCl connects to D1
Oled SDA connects to D2
HX711 SDA connects to D5
HX711 SCL connects to D6

You will need to calibrate your load cell to the filament weight
Measure the following conditions:
1) no contact reading -- pivot arm not touching load cell
   Record load cell value: NO_CONTACT_U
2) no spool reading -- pivot arm touching load cell spool holder installed but no spool installed
   Record load cell value: NO_SPOOL_U
3) Maximum load reading -- full sized spool installed and pivot arm contacting load cell  
   Record load cell value: FULL_LOAD_U
   Measure full spool on kitchen scale and record value (grams): FULL_LOAD_G
4) If desired modify the default spool selections, size and weight

Library dependencies (for platformio) are:
lib_deps =
  HX711           ; by bogde
  ESP8266_SSD1306 ; by Daniel Eichhorn
  OneButton       ; by Matthias Hertel
*/

/* 
Change Log
20180913 hdm initial code release
*/

// user defined parameters
#define NO_CONTACT_U 8522.0 // load cell reading: pivot arm is not in contact with load cell
#define NO_SPOOL_U   8594.5 // load cell reading: spool holder installed but no spool included
#define FULL_LOAD_U  9011.6 // load cell reading at maximum load
#define FULL_LOAD_G  1117.0 // maximum load in grams

#define STR_SPOOL_1 "Solid 50x200x90" // hole diameter x spool diameter x thickness
#define STR_SPOOL_2 "Open 50x200x76"
#define SPOOL_1_G 219.0  // gram
#define SPOOL_2_G 196.0  // gram 
#define FILAMENT_M_PER_GRAM 0.33 // default is for PLA

#include <Arduino.h>
#include <Wire.h>
#include "SH1106.h" // for 1.3" OLED from ESP8266_SSD1306_ID562
#include "HX711.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include "ArduinoOTA.h"
#include "secrets.h"
          // WIFI_SSID = "myWiFiSSID"
          // WIFI_PASSPHRASE = "myWiFiPassword"

// Pins Used:
//  D1 - OLED Shield - SCL
//  D2 - OLED Shield - SDA
//  D3 - Pushbutton Shield
//  D5 - HX711 - SDA
//  D6 - HX711 - SCL

// SCL GPIO5  D1
// SDA GPIO4  D2
SH1106 display(0x3c, D2, D1);

// HX711.DOUT	- pin #D5
// HX711.PD_SCK	- pin #D6
HX711 scale(D5, D6);		// parameter "gain" is ommited; the default value 128 is used by the library

#define MAXMENU 2
#define MAX_MENU2 4

#define BUTTON_NO_PRESS 0
#define BUTTON_SHORT_PRESS 1
#define BUTTON_LONG_PRESS 2

float slope_g_u; // change in grams / change in u
float no_force_g; // g value at u=0
float no_contact_u; // current no-contact value

float weight_u;  // units of HX711/1000
float weight_g;  // weight_u converted to grams including spool
float currentSpool_g;  // weight of current empty spool in g
float noLoadWeight_g;  // noload reading HX711/1000 units
float no_contact_g;    // grams at zero contact
String strCurrentSpool = "                          ";

// menu related items
int curMenu = 1;
int curMenu2 = 1;
int buttonPress;
bool longPress = false;
bool clicked = false;
bool doubleClicked = false;

float units2grams(float x) 
// convert HX711 units to grams of filament
{
  // subtract off zero contact compensation
  x = x + NO_CONTACT_U - no_contact_u;
  // subtract off current spool weight
  return(slope_g_u*x + no_force_g - currentSpool_g);
} // units2grams

void init_hardware()
{
  WiFi.disconnect(true);

  pinMode(D3, INPUT); // set pushbutton shield port

  WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
} // init_hardware

int checkClick() {
  long ms = millis();
  if (digitalRead(D3) == false) {
    delay(20);
  } else {
    delay(10);
    if (digitalRead(D3))
      return(0);
  }
  if (digitalRead(D3) == false) { // button is pressed
    
    while(digitalRead(D3) == false) {
      delay(1);
    }
    ms = millis() - ms;
    if (ms > 500) {
      buttonPress = BUTTON_LONG_PRESS;
    } else {
      buttonPress = BUTTON_SHORT_PRESS;
    }
  }
  return(buttonPress);
} // checkClick

void displayMenu3() {

  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0,10,"     Filament Scale");
  display.drawString(0,10,"slope_g_u: " + String(slope_g_u,5));
  display.drawString(0,20,"no_contact_g: " + String(no_contact_g,5));
  display.drawString(0,30,"NO_FORCE_G: " + String(no_force_g,10));
  display.drawString(0,30,"IP: " + String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + 
                        String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]));

  display.drawString(0,50,"Raw: " + String(weight_u,2) + " g: " + String(units2grams(weight_u),1));
  display.display();
} // displayMenu3

void displayMenu1() { // Main menu
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(0,25,String(weight_g,0) + "g, " + String(weight_g * FILAMENT_M_PER_GRAM,0) + "m"  );
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,0, "Load Cell: " + String(weight_u,2));
  display.drawString(0,54,strCurrentSpool + ": " + String(currentSpool_g,0) + "g");
  display.display();
} // displayMenu1

void displayMenu2() { // selection menu
  String s[4] = {""};
  s[curMenu2-1] = ">";
  display.clear();  
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,0 ,s[0] + "Set Zero"); // menu 21
  display.drawString(0,10,s[1] + "Spool Solid 50x200x90"); // menu 22
  display.drawString(0,20,s[2] + "Spool Open 50x200x76");  // menu 23
  display.drawString(0,30,s[3] + "Next Menu"); // menu 3
  display.drawString(0,50,"long click to select");
  display.display();
} // displayMenu2

void displayMenu21(){ // set zero
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,0, "     Set Zero");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,16, "Lift up spool to");
  display.drawString(0,26,"remove all weight");
  display.drawString(0,40,"            " + String((units2grams(weight_u)-no_contact_g),1) + "g");
  display.drawString(0,54,"Click when steady state");
  display.display();
} // displayMenu21

void displayMenu22(){ // set Solid 50x200x90
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,0, "Solid 50x200x90");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,16, "Setting spool type to");
  display.drawString(0,26,"Solid 50x200x90");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,40,"Weight:" + String(SPOOL_1_G,0));
  display.display();
  currentSpool_g = SPOOL_1_G;
  strCurrentSpool = STR_SPOOL_1;
  delay(2000);
  curMenu = 1;
} // displayMenu22

void displayMenu23(){ // set Open 50x200x76
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,0, "Open 50x200x76");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,16, "Setting spool type to");
  display.drawString(0,26,"Open 50x200x76");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,40,"Weight:" + String(SPOOL_2_G,0));
  display.display();
  currentSpool_g = SPOOL_2_G;
  strCurrentSpool = STR_SPOOL_2;
  delay(2000);
  curMenu = 1;
} // displayMenu23

void displayMenu() {
  switch (curMenu) {
    case 1:
      displayMenu1();
      break;
    case 2:
      displayMenu2();
      break;
    case 3:
      displayMenu3();
      break;
    case 21:
      displayMenu21();
      break;
    case 22:
      displayMenu22();
      break;
    case 23:
      displayMenu23();
      break;
    default:
      displayMenu1();
  } // switch
} // displayMenu

void doMenuClick() {
  switch (curMenu) {
    case 1:
      curMenu = 2;
      curMenu2 = 1;
      break;
    case 2:
      if (buttonPress == BUTTON_LONG_PRESS) 
        switch (curMenu2) {
          case 1: // Set Zero
            curMenu = 21;
            break;
          case 2: // Spool Solid 50x200x90
            curMenu = 22;
            break;
          case 3: // Spool Open 50x200x76
            curMenu = 23;
            break;
          case 4: // Next Menu
            curMenu = 3;
            break;
          default:
            curMenu = 1;
        } // switch curMenu2
      curMenu2++;
      if (curMenu2 > MAX_MENU2) 
        curMenu2 = 1;
      break;
    case 3:
      displayMenu3();
      curMenu = 1;
      curMenu2 = 1;
      break;
    case 21: // display set zero menu
      displayMenu21();
      no_contact_u = weight_u;
      no_contact_g = units2grams(no_contact_u);
      curMenu = 1;
      break;
    default:
      displayMenu1();
  } // switch curMenu
  buttonPress = BUTTON_NO_PRESS;
} // doMenuClick

void setup()   {
#if USE_SERIAL_MONITOR
  Serial.begin(115200);
#endif

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0,10,"Starting Up");
  display.display();

  init_hardware();

  delay(1000);

  // setup scale parameters
  weight_u = scale.read()/1000.0;
  currentSpool_g = SPOOL_1_G;
  strCurrentSpool = STR_SPOOL_1;
  slope_g_u = FULL_LOAD_G/(FULL_LOAD_U-NO_SPOOL_U);
  no_force_g = -NO_SPOOL_U*slope_g_u;
  no_contact_u = NO_CONTACT_U;
  no_contact_g = units2grams(NO_CONTACT_U);

  ArduinoOTA.begin();
} // setup

void loop() {

  weight_u = weight_u + (scale.read()/1000.0-weight_u)*0.2;
  weight_g = units2grams(weight_u);
  displayMenu();
  if (checkClick()) {
    doMenuClick();
  }
  ArduinoOTA.handle();
  delay(5);
} // loop
