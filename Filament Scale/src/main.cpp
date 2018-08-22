// Measured values:
//  zero load: 8522.6
//  no spool:  8595.3 = 72.7 = load of holder
// Measured values
// condition  trial1  trial2  trial3
// no spool   8595.3  8596.0  8595.6
// 1168g      9028.4  9029.4  9030.0
//  486g      8775.4  8776.2  8776.3
//  900g      8928.0  8930.5  8930.2
//  scale factor: 2.6945 g/units
//  spool holder load: 8595.6 units
#define HX711_SCALE 2.6945 // g/unit
#define SPOOL_HOLDER_WEIGHT_U 71.5 //70.5 //72.633 // unit
#define SPOOL_SOLID_50x200x90 219.0  // gram
#define SPOOL_OPEN_50x200x76 196.0  // gram 
#define STR_SOLID_50x200x90 "Solid 50x200x90"
#define STR_OPEN_50x200x76 "Open 50x200x76"
#define NO_LOAD_U 8523.0 // unit
#define FILAMENT_M_PER_GRAM 0.33



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
#define M_PER_GRAM 0.33  // filament meters per gram

float weight_u;  // units of HX711/1000
float CurrentWeight_u = 0; // units
ulong freeHeap;
long milli;
long h,m,s;
int curMenu = 1;
int curMenu2 = 1;
bool longPress = false;
long menu1Time = 0;
long menu2Time = 0;
bool clicked = false;
bool doubleClicked = false;
int buttonPress;
float currentSpool_u;  // weight of current empty spool in units
float currentSpool_g;  // weight of current empty spool in g
float noLoadWeight_u;  // noload reading HX711/1000 units
String strCurrentSpool;

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
      buttonPress = 2;
    } else {
      buttonPress = 1;
    }
  }
  return(buttonPress);
} // checkClick

void setup()   {
#if USE_SERIAL_MONITOR
  Serial.begin(9600);
#endif

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0,10,"Starting Up");
  display.display();

  init_hardware();
  delay(1000);
  weight_u = scale.read()/1000.0;
  currentSpool_g = SPOOL_SOLID_50x200x90;
  currentSpool_u = SPOOL_SOLID_50x200x90 / HX711_SCALE;
  noLoadWeight_u = NO_LOAD_U;
  strCurrentSpool = STR_SOLID_50x200x90;
  ArduinoOTA.begin();
} // setup

void displayMenu3() {
  menu2Time = millis();
   freeHeap = ESP.getFreeHeap();
  milli = millis();
  h = milli/3600000;
  milli = milli - 3600000*h;
  m = milli/60000;
  milli = milli - 60000*m;
  s = milli/1000;

  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0,0,"     Filament Scale");
  display.drawString(0,10,"Free Heap: " + String(freeHeap));
  display.drawString(0,20,"On time: " + String(h) + "h " + String(m) + "m " + String(s) + "s");
  display.drawString(0,30,"IP: " + String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + 
                        String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]));

  display.drawString(0,40,"Raw: " + String(weight_u,2));
  display.drawString(0,50,"Actual: " + String(((weight_u-noLoadWeight_u-currentSpool_u-SPOOL_HOLDER_WEIGHT_U)*HX711_SCALE),0) + "g");
  display.display();
  menu2Time = millis() - menu2Time;
} // displayMenu3

void displayMenu1() { // Main menu
float weight_g;
  weight_g = ((weight_u-noLoadWeight_u-currentSpool_u-SPOOL_HOLDER_WEIGHT_U)*HX711_SCALE);
  display.setFont(ArialMT_Plain_16);
  display.clear();
  display.drawString(0,0,"Filament Weight");  
  display.setFont(ArialMT_Plain_24);
  display.drawString(0,25,String(weight_g,0) + "g, " + String(weight_g * FILAMENT_M_PER_GRAM,0) + "m"  );
  display.setFont(ArialMT_Plain_10);
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
  display.drawString(0,40,"            " + String(((weight_u-noLoadWeight_u)*HX711_SCALE),1) + "g");
  display.drawString(0,54,"Click when steady state");
  display.display();
  noLoadWeight_u = weight_u;
} // displayMenu21

void displayMenu22(){ // set Solid 50x200x90
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,0, "Solid 50x200x90");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,16, "Setting spool type to");
  display.drawString(0,26,"Solid 50x200x90");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,40,"Weight:" + String(SPOOL_SOLID_50x200x90,0));
  display.display();
  currentSpool_g = SPOOL_SOLID_50x200x90;
  currentSpool_u = SPOOL_SOLID_50x200x90 / HX711_SCALE;
  strCurrentSpool = STR_SOLID_50x200x90;
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
  display.drawString(0,40,"Weight:" + String(SPOOL_OPEN_50x200x76,0));
  display.display();
  currentSpool_g = SPOOL_OPEN_50x200x76;
  currentSpool_u = SPOOL_OPEN_50x200x76 / HX711_SCALE; // displayMenu23
  strCurrentSpool = STR_OPEN_50x200x76;
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
      if (buttonPress == 2) 
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
    case 21:
      displayMenu21();
      // need to set reference value
      curMenu = 1;
      break;
    default:
      displayMenu1();
  } // switch curMenu
  buttonPress = 0;
} // doMenuClick

void loop() {

  weight_u = weight_u + (scale.read()/1000.0-weight_u)*0.2;
  displayMenu();
  if (checkClick()) {
    doMenuClick();
  }
  ArduinoOTA.handle();
  delay(5);
} // loop
