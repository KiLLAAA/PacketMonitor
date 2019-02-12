/*
  ===========================================
       Copyright (c) 2018 Stefan Kremser
              github.com/spacehuhn
  ===========================================
       Copyright (c) 2019 Lukas Vyhnalek
              github.com/KiLLAAA
  ===========================================
*/

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <EEPROM.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
#include "user_interface.h"
}

/*===== SETTINGS =====*/
/* create display(SCL-> D1, SDA-> D2) */
Adafruit_SSD1306 display(-1); // Display reset pin set to -1 for i2c as its present on SPI version.

/* select the button for your board */
#define btn 0         // GPIO 0 = FLASH BUTTON 

#define maxCh 13       // max Channel -> US = 11, EU = 13, Japan = 14
#define ledPin 2       // led pin ( 2 = built-in LED)
#define packetRate 5   // min. packets before it gets recognized as an attack

/* Display settings */
#define minRow       0              /* default =   0 */
#define maxRow     127              /* default = 127 */
#define minLine      0              /* default =   0 */
#define maxLine     63              /* default =  63 */

/* render settings */
#define Row1         0
#define Row2        30
#define Row3        35
#define Row4        80
#define Row5        85
#define Row6       125

#define LineText     0
#define Line        12
#define LineVal     47

//===== Run-Time variables =====//
unsigned long prevTime   = 0;
unsigned long curTime    = 0;
unsigned long pkts       = 0;
unsigned long no_deauths = 0;
unsigned long deauths    = 0;
int curChannel           = 1;
unsigned long maxVal     = 0;
double multiplicator     = 0.0;
bool canBtnPress         = true;

unsigned int val[128];

void sniffer(uint8_t *buf, uint16_t len) {
  pkts++;
  if (buf[12] == 0xA0 || buf[12] == 0xC0) {
    deauths++;
  }
}

void getMultiplicator() {
  maxVal = 1;
  for (int i = 0; i < maxRow; i++) {
    if (val[i] > maxVal) maxVal = val[i];
  }
  if (maxVal > LineVal) multiplicator = (double)LineVal / (double)maxVal;
  else multiplicator = 1;
}

//===== SETUP =====//
void setup() {
  /* start Display */
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
  // init done
  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(2000);

  /* show start screen */
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(Row1, LineText);
  display.println("Packet-");
  display.println("Monitor");
  display.setTextSize(1);
  display.setCursor(Row1, LineVal);
  display.println("Copyright (c) 2018");
  display.println("Stefan Kremser");
  display.display();
  delay(2500);
  /* show start screen */
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(Row1, LineText);
  display.println("Packet-");
  display.println("Monitor");
  display.setTextSize(1);
  display.setCursor(Row1, LineVal);
  display.println("Mod to Adafruit by");
  display.println("Lukas Vyhnalek");
  display.display();
  delay(2500);

  /* start Serial */
//  Serial.begin(115200);

  /* load last saved channel */
  EEPROM.begin(4096);
  curChannel = EEPROM.read(2000);
  if (curChannel < 1 || curChannel > maxCh) {
    curChannel = 1;
    EEPROM.write(2000, curChannel);
    EEPROM.commit();
  }

  /* set pin modes for buttons */
  pinMode(btn, INPUT_PULLUP);

  /* set pin modes for LED */
  pinMode(ledPin, OUTPUT);

  /* setup wifi */
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
  WiFi.disconnect();
  wifi_set_promiscuous_rx_cb(sniffer);
  wifi_set_channel(curChannel);
  wifi_promiscuous_enable(1);

  Serial.println("starting!");
}

//===== LOOP =====//
void loop() {
  curTime = millis();

  //on button release
  if (digitalRead(btn) == LOW) {
    if (canBtnPress) canBtnPress = false;
  } else if (!canBtnPress) {
    canBtnPress = true;

    //switch channel
    curChannel++;
    if (curChannel > maxCh) curChannel = 1;
    wifi_set_channel(curChannel);
    for (int i = 0; i < maxRow; i++) val[i] = 0;
    pkts = 0;
    multiplicator = 1;

    //save changes
    EEPROM.write(2000, curChannel);
    EEPROM.commit();

    if (pkts == 0) pkts = deauths;
    no_deauths = pkts - deauths;

    //draw display
    display.clearDisplay();
    display.drawLine(minRow, Line, maxRow, Line, WHITE);
    display.setCursor(Row1, LineText);
    display.printf("Ch:%2d", curChannel);
    display.setCursor(Row3, LineText);
    display.printf("Pkts:%3d", no_deauths);
    display.setCursor(Row5, LineText);
    display.printf("DA:%3d", deauths);
    for (int i = 0; i < maxRow; i++) display.drawLine(i, maxLine, i, maxLine - val[i]*multiplicator, WHITE);
    display.display();
  }

  //every second
  if (curTime - prevTime >= 1000) {
    prevTime = curTime;

    //move every packet bar one pixel to the left
    for (int i = 0; i < maxRow; i++) {
      val[i] = val[i + 1];
    }
    val[127] = pkts;

    //recalculate scaling factor
    getMultiplicator();

    //deauth alarm
    if (deauths > packetRate) digitalWrite(ledPin, LOW);
    else digitalWrite(ledPin, HIGH);

    if (pkts == 0) pkts = deauths;
    no_deauths = pkts - deauths;

    //draw display
    display.clearDisplay();
    display.drawLine(minRow, Line, maxRow, Line, WHITE);
    display.setCursor(Row1, LineText);
    display.printf("Ch:%2d", curChannel);
    display.setCursor(Row3, LineText);
    display.printf("Pkts:%3d", no_deauths);
    display.setCursor(Row5, LineText);
    display.printf("DA:%3d", deauths);
    for (int i = 0; i < maxRow; i++) display.drawLine(i, maxLine, i, maxLine - val[i]*multiplicator, WHITE);
    display.display();

    //reset counters
    deauths    = 0;
    pkts       = 0;
  }

}
