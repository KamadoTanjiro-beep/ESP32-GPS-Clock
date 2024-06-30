#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <Adafruit_AHTX0.h>
#include <BH1750.h>
#include <TimeLib.h>
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <esp_wifi.h>
#include "driver/adc.h"

#include "global.h" //remove this

#ifndef STASSID
#define STASSID "YOUR_SSID"    // WIFI NAME/SSID
#define STAPSK "YOUR_PASSWORD" // WIFI PASSWORD
#endif

const char *ssid = pssid;     // remove "pssid" and write "STASSID"
const char *password = ppass; // remove "ppass" and write "STAPSK"

const String newHostname = "NiniClock";

AsyncWebServer server(80);

Adafruit_AHTX0 aht;
BH1750 lightMeter;

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, 18, 23, 5, U8X8_PIN_NONE);

#define lcdBrightnessPin 4
#define lcdEnablePin 33
#define buzzerPin 25

char week[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char monthChar[12][12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// GPS things
static const int RXPin = 16, TXPin = 17;
TinyGPSPlus gps;
#define gpsPin 19

unsigned long lastTime = 0;
const long timerDelay = 180000; // AHT25 delay

unsigned long lastTime1 = 0;
const long timerDelay1 = 15000; // LUX delay

unsigned long lastTime2 = 0;
const long timerDelay2 = 10000; // LUX inside delay

sensors_event_t humidity, temp;

byte pulse = 0, powSave = 0, checker = 0, timeGuard = 0;

void setup()
{
  pinMode(gpsPin, OUTPUT);
  digitalWrite(gpsPin, HIGH);
  pinMode(lcdBrightnessPin, OUTPUT);
  analogWrite(lcdBrightnessPin, 250);
  pinMode(lcdEnablePin, OUTPUT);
  digitalWrite(lcdEnablePin, HIGH);
  pinMode(buzzerPin, OUTPUT);
  Wire.begin();
  lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.drawLine(0, 17, 127, 17);
  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(12, 30);
  u8g2.print("GPS Clock");
  u8g2.drawLine(0, 31, 127, 31);
  u8g2.sendBuffer();
  delay(1000);
  u8g2.clearBuffer();
  u8g2.drawBox(0, 0, 127, 63);
  u8g2.sendBuffer();
  delay(500);

  Serial1.begin(9600, SERIAL_8N1, RXPin, TXPin);

  if (!aht.begin())
  {
    u8g2.clearBuffer();
    u8g2.drawLine(0, 17, 127, 17);
    u8g2.setFont(u8g2_font_7x14B_mr);
    u8g2.setCursor(12, 30);
    u8g2.print("AHT25 Failed");
    u8g2.drawLine(0, 31, 127, 31);
    u8g2.sendBuffer();
    buzzer(200, 4);
    delay(10000);
  }

  setCpuFrequencyMhz(80);
  aht.getEvent(&humidity, &temp);

  u8g2.clearBuffer();
  u8g2.drawLine(0, 17, 127, 17);
  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(5, 30);
  u8g2.print("Connecting...");
  u8g2.drawLine(0, 31, 127, 31);
  u8g2.sendBuffer();
  Serial.println("Connecting");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(newHostname.c_str());
  WiFi.begin(ssid, password);

  // Wait for connection
  /*
    count variable stores the status of WiFi connection. 1 means NOT CONNECTED. 0 means CONNECTED
  */
  int count = 0;
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    u8g2.clearBuffer();
    u8g2.drawLine(0, 17, 127, 17);
    u8g2.setFont(u8g2_font_7x14B_mr);
    u8g2.setCursor(5, 30);
    u8g2.print("Connection Failed");
    u8g2.drawLine(0, 31, 127, 31);
    u8g2.sendBuffer();
    Serial.println("Connection Failed");
    delay(2000);
    count = 1;
    break;
  }
  if (count == 0)
  {
    u8g2.clearBuffer();
    u8g2.drawLine(0, 17, 127, 17);
    u8g2.setFont(u8g2_font_7x14B_mr);
    u8g2.setCursor(5, 30);
    u8g2.print(WiFi.localIP());
    u8g2.drawLine(0, 31, 127, 31);
    u8g2.sendBuffer();
    delay(2000);
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", "Hi! I am ESP32, the Nini GPS Clock."); });

    AsyncElegantOTA.begin(&server); // Start ElegantOTA
    server.begin();
    Serial.println("HTTP server started");
  }
}

void loop()
{

  while (!gps.hdop.isValid())
  {
    u8g2.clearBuffer();
    u8g2.drawLine(0, 17, 127, 17);
    u8g2.setFont(u8g2_font_7x14B_mr);
    u8g2.setCursor(12, 30);
    u8g2.print("Waiting for GPS");
    u8g2.drawLine(0, 31, 127, 31);
    u8g2.sendBuffer();
    smartDelay(1000);
  }
  while (checker > 5)
  {
    u8g2.clearBuffer();
    u8g2.drawLine(0, 17, 127, 17);
    u8g2.setFont(u8g2_font_7x14B_mr);
    u8g2.setCursor(12, 30);
    u8g2.print("GPS error !!");
    u8g2.drawLine(0, 31, 127, 31);
    u8g2.sendBuffer();
    buzzer(200, 4);
    delay(5000);
    ESP.restart();
  }

  byte days = gps.date.day();
  byte months = gps.date.month();
  int years = gps.date.year();
  byte hours = gps.time.hour();
  byte minutes = gps.time.minute();
  byte seconds = gps.time.second();
  setTime(hours, minutes, seconds, days, months, years);
  adjustTime(19800); // 5.5*60*60

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_pixzillav1_tr);
  u8g2.setCursor(5, 15);
  u8g2.print(temp.temperature, 2);
  u8g2.setFont(u8g2_font_threepix_tr);
  u8g2.setCursor(40, 8);
  u8g2.print("o");
  u8g2.setFont(u8g2_font_pixzillav1_tr);
  u8g2.setCursor(45, 15);
  u8g2.print("C ");
  u8g2.setCursor(64, 15);
  u8g2.print(humidity.relative_humidity, 2);
  u8g2.setCursor(100, 15);
  u8g2.print("%rH");

  u8g2.drawLine(0, 17, 127, 17);
  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(8, 30);
  if (day() < 10)
    u8g2.print("0");
  u8g2.print(day());
  byte x = day() % 10;
  u8g2.setFont(u8g2_font_profont10_mr);
  u8g2.setCursor(22, 25);
  if (day() == 11 || day() == 12 || day() == 13)
    u8g2.print("th");
  else if (x == 1)
    u8g2.print("st");
  else if (x == 2)
    u8g2.print("nd");
  else if (x == 3)
    u8g2.print("rd");
  else
    u8g2.print("th");

  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(34, 30);
  u8g2.print(monthChar[month() - 1]);
  u8g2.setCursor(58, 30);
  u8g2.print(year());
  u8g2.setCursor(102, 30);
  u8g2.print(week[weekday() - 1]);

  u8g2.drawLine(0, 31, 127, 31);
  u8g2.setFont(u8g2_font_logisoso30_tn); // u8g2_font_helvB24_tn);
  u8g2.setCursor(15, 63);

  if (checker == 0)
    timeGuard = seconds;

  if (second() == 0 && (minute() == 0 || minute() == 30) && (hour() >= 6 && hour() < 24))
  {
    if (minute() == 0)
      buzzer(500, 1);
    else if (minute() == 30)
      buzzer(300, 2);
  }
  if (hourFormat12() < 10)
    u8g2.print("0");
  u8g2.print(hourFormat12());
  if (pulse == 0)
    u8g2.print(":");
  else
    u8g2.print("");

  u8g2.setCursor(63, 63);
  if (minute() < 10)
    u8g2.print("0");
  u8g2.print(minute());

  u8g2.setFont(u8g2_font_tenthinnerguys_tu);
  u8g2.setCursor(105, 42);
  if (second() < 10)
    u8g2.print("0");
  u8g2.print(second());
  u8g2.setCursor(105, 63);
  if (!isAM())
    u8g2.print("PM");
  else
    u8g2.print("AM");
  u8g2.sendBuffer();

  if (pulse == 1)
    pulse = 0;
  else if (pulse == 0)
    pulse = 1;

  smartDelay(900);
  if ((millis() - lastTime) > timerDelay)
  {
    aht.getEvent(&humidity, &temp);
    lastTime = millis();
  }

  if ((millis() - lastTime1) > timerDelay1)
  { // light sensor based power saving operations
    lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
    float lux;
    while (!lightMeter.measurementReady(true))
    {
      yield();
    }
    lux = lightMeter.readLightLevel();
    /*while (lux < 0.01) {
      if ((millis() - lastTime2) > timerDelay2) {
        lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
        while (!lightMeter.measurementReady(true)) {
          yield();
        }
        lux = lightMeter.readLightLevel();
        if (powSave != 1) {
          u8g2.setPowerSave(1);
          delay(200);

          adc_power_off();
          WiFi.disconnect(true);  // Disconnect from the network
          WiFi.mode(WIFI_OFF);

          digitalWrite(lcdBrightnessPin, LOW);
          digitalWrite(gpsPin, LOW);
          digitalWrite(lcdEnablePin, LOW);
          setCpuFrequencyMhz(10);
          powSave = 1;
        }
        lastTime2 = millis();
      }
      yield();
    }
    if (lux > 0.00 && powSave != 0) {
      ESP.restart();
    }*/

    // Brightness control
    if (lux == 0)
      analogWrite(lcdBrightnessPin, 5);
    else
    {
      byte val1 = constrain(lux, 1, 100);     // constrain(number to constrain, lower end, upper end)
      byte val2 = map(val1, 1, 100, 50, 255); // map(value, fromLow, fromHigh, toLow, toHigh);  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
      byte val3 = constrain(val2, 50, 255);
      analogWrite(lcdBrightnessPin, val3);
    }
    lastTime1 = millis();
  }
  if (timeGuard != seconds)
    checker = 0;
  else
    checker++;
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}

// int delay value, byte count value
void buzzer(int Delay, byte count)
{
  for (int i = 0; i < count; i++)
  {
    digitalWrite(buzzerPin, HIGH);
    delay(Delay);
    digitalWrite(buzzerPin, LOW);
    delay(Delay);
  }
}