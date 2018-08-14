#define USE_SERIAL_MONITOR 0

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "HX711.h"
#include <ESP8266WiFi.h>
#include "CMMC_OTA.h"
#include "secrets.h"
          // WIFI_SSID = "myWiFiSSID"
          // WIFI_PASSPHRASE = "myWiFiPassword"

// Pins Used:
//  D1 - OLED Shield
//  D2 - OLED Shield
//  D3 - Pushbutton Shield
//  D5 - HX711
//  D6 - HX711

// SCL GPIO5  D1
// SDA GPIO4  D2
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

// HX711.DOUT	- pin #D5
// HX711.PD_SCK	- pin #D6
HX711 scale(D5, D6);		// parameter "gain" is ommited; the default value 128 is used by the library

#if (SSD1306_LCDHEIGHT != 48)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

#define S0 8533.0
#define S219 8612.0
#define S1190 8961.0
#define HX711_SCALE 2.78037
#define M_PER_GRAM 0.3
#define NOMINALSPOOL 219


CMMC_OTA ota;
float weight;
float Tare = S219;
float CurrentWeight = 0;
ulong freeHeap;
long milli;
long h,m,s;


void init_hardware()
{
  WiFi.disconnect(true);

  pinMode(D3, INPUT); // set pushbutton shield port

#if USE_SERIAL_MONITOR
  Serial.flush();
  Serial.println();
  Serial.println();
  Serial.println("being started...");
  delay(100);
#endif

  WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);
  while (WiFi.status() != WL_CONNECTED) {
#if USE_SERIAL_MONITOR
  Serial.printf("connecting %s:%s \r\n", WIFI_SSID, WIFI_PASSPHRASE);
#endif
  delay(100);
 }

#if USE_SERIAL_MONITOR
  Serial.print("READY!! IP address: ");
  Serial.println(WiFi.localIP());
#endif
} // init_hardware

void SetTare() {
  display.clearDisplay();
  display.setCursor(0,20);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Remove");
  display.println("Spool");
  display.display();

  do {
    delay(10);
  } while(digitalRead(D3) == false) ;

  delay(2000);
  display.clearDisplay();
  display.setCursor(0,20);
  display.println("Please");
  display.println("Wait");
  display.display();

  Tare = scale.read_average(10)/1000.0 + S219-S0;
  delay(2000);
}

void loop() {
  freeHeap = ESP.getFreeHeap();
  weight = scale.read_average(2)/1000.0;
  milli = millis();
  h = milli/3600000;
  milli = milli - 3600000*h;
  m = milli/60000;
  milli = milli - 60000*m;
  s = milli/1000;

  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println(freeHeap);
  // display.println(millis());
  display.print(h);
  display.print(":");
  display.print(m);
  display.print(":");
  display.println(s);
  display.println(WiFi.localIP());
  display.setTextSize(2);
  display.print((weight-Tare)*HX711_SCALE,0);
  display.println("g");
  // display.print((weight-S219)*HX711_SCALE*M_PER_GRAM,0);
  // display.println("m");
  display.display();

  if (digitalRead(D3) == false) {
    SetTare();
  }

  delay(100);
#if USE_SERIAL_MONITOR
  Serial.println(scale.read());
#endif
  ota.loop();
} // loop

void setup()   {
#if USE_SERIAL_MONITOR
  Serial.begin(9600);
#endif

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)

  display.setTextSize(1);

  init_hardware();
  ota.init();
  delay(1000);
} // setup
