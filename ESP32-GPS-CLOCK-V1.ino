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
#include <ElegantOTA.h>
#include <esp_wifi.h>
#include "driver/adc.h"

//Data Storage
#include <Preferences.h>
//Preference library object or instance
Preferences pref;

//your wifi name and password (saved in preference library)
String ssid;
String password;

//Wifi Manager HTML Code
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Wi-Fi Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
html {
  font-family: Arial, Helvetica, sans-serif; 
  display: inline-block; 
  text-align: center;
}

h1 {
  font-size: 1.8rem; 
  color: white;
}

p { 
  font-size: 1.4rem;
}

.topnav { 
  overflow: hidden; 
  background-color: #0A1128;
}

body {  
  margin: 0;
}

.content { 
  padding: 5%;
}

.card-grid { 
  max-width: 800px; 
  margin: 0 auto; 
  display: grid; 
  grid-gap: 2rem; 
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
}

.card { 
  background-color: white; 
  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
}

.card-title { 
  font-size: 1.2rem;
  font-weight: bold;
  color: #034078
}

input[type=submit] {
  border: none;
  color: #FEFCFB;
  background-color: #034078;
  padding: 15px 15px;
  text-align: center;
  text-decoration: none;
  display: inline-block;
  font-size: 16px;
  width: 100px;
  margin-right: 10px;
  border-radius: 4px;
  transition-duration: 0.4s;
  }

input[type=submit]:hover {
  background-color: #1282A2;
}

input[type=text], input[type=number], select {
  width: 50%;
  padding: 12px 20px;
  margin: 18px;
  display: inline-block;
  border: 1px solid #ccc;
  border-radius: 4px;
  box-sizing: border-box;
}

label {
  font-size: 1.2rem; 
}
.value{
  font-size: 1.2rem;
  color: #1282A2;  
}
.state {
  font-size: 1.2rem;
  color: #1282A2;
}
button {
  border: none;
  color: #FEFCFB;
  padding: 15px 32px;
  text-align: center;
  font-size: 16px;
  width: 100px;
  border-radius: 4px;
  transition-duration: 0.4s;
}
.button-on {
  background-color: #034078;
}
.button-on:hover {
  background-color: #1282A2;
}
.button-off {
  background-color: #858585;
}
.button-off:hover {
  background-color: #252524;
} 
  </style>
</head>
<body>
  <div class="topnav">
    <h1>Wi-Fi Manager</h1>
  </div>
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <form action="/wifi" method="POST">
          <p>
            <label for="ssid">SSID</label>
            <input type="text" id ="ssid" name="ssid"><br>
            <label for="pass">Password</label>
            <input type="text" id ="pass" name="pass"><br>
            <input type ="submit" value ="Submit">
          </p>
        </form>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";

const String newHostname = "NiniClock";  //any name that you desire

AsyncWebServer server(80);
//Elegant OTA related task
bool updateInProgress = false;
unsigned long ota_progress_millis = 0;

Adafruit_AHTX0 aht;
BH1750 lightMeter;

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, 18, 23, 5, U8X8_PIN_NONE);

#define lcdBrightnessPin 4
#define lcdEnablePin 33
#define buzzerPin 25

char week[7][12] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char monthChar[12][12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// GPS things
static const int RXPin = 16, TXPin = 17;
TinyGPSPlus gps;
#define gpsPin 19

//variables for holding time data globally
byte days = 0, months = 0, hours = 0, minutes = 0, seconds = 0;
int years = 0;

bool isDark = false;

//LUX (BH1750) update frequency
unsigned long lastTime1 = 0;
const long timerDelay1 = 3000;  // LUX delay

//AHT25 update frequency
unsigned long lastTime2 = 0;
const long timerDelay2 = 12000;  // aht update delay

sensors_event_t humidity, temp;

byte pulse = 0;

time_t prevDisplay = 0;  // when the digital clock was displayed

//for creating task attached to CORE 0 of CPU
TaskHandle_t loop1Task;

void onOTAStart() {
  // Log when OTA has started
  updateInProgress = true;
  Serial.println("OTA update started!");
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_luRS08_tr);
  u8g2.setCursor(1, 20);
  u8g2.print("OTA UPDATE");
  u8g2.setCursor(1, 32);
  u8g2.print("HAVE STARTED");
  u8g2.sendBuffer();
  delay(1000);
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log
  if (millis() - ota_progress_millis > 500) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 20);
    u8g2.print("OTA UPDATE");
    u8g2.setCursor(1, 32);
    u8g2.print("UNDER PROGRESS");
    u8g2.setCursor(1, 44);
    u8g2.print("Done: ");
    u8g2.print(current);
    u8g2.print(" bytes");
    u8g2.setCursor(1, 56);
    u8g2.print("Total: ");
    u8g2.print(final);
    u8g2.print(" bytes");
    u8g2.sendBuffer();
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 20);
    u8g2.print("OTA UPDATE");
    u8g2.setCursor(1, 32);
    u8g2.print("COMPLETED!");
    u8g2.sendBuffer();
  } else {
    Serial.println("There was an error during OTA update!");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 20);
    u8g2.print("OTA UPDATE");
    u8g2.setCursor(1, 32);
    u8g2.print("HAVE FAILED");
    u8g2.sendBuffer();
  }
  // <Add your own code here>
  updateInProgress = false;
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(gpsPin, OUTPUT);
  digitalWrite(gpsPin, HIGH); // you can connect the gps directly to 3.3V pin 
  pinMode(lcdBrightnessPin, OUTPUT);
  analogWrite(lcdBrightnessPin, 250);
  pinMode(lcdEnablePin, OUTPUT);
  digitalWrite(lcdEnablePin, HIGH);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH);
  delay(100);
  digitalWrite(buzzerPin, LOW);
  if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE))
    errorMsgPrint("BH1750", "CANNOT FIND");

  if (!pref.begin("database", false))  //open database
    errorMsgPrint("DATABASE", "ERROR INITIALIZE");

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

  if (!aht.begin()) {
    errorMsgPrint("AHT25", "CANNOT FIND");
  }

  setCpuFrequencyMhz(160);
  aht.getEvent(&humidity, &temp);

  //wifi manager
  if (true) {
    bool wifiConfigExist = pref.isKey("ssid");
    if (!wifiConfigExist) {
      pref.putString("ssid", "");
      pref.putString("password", "");
    }

    ssid = pref.getString("ssid", "");
    password = pref.getString("password", "");

    if (ssid == "" || password == "") {
      Serial.println("No values saved for ssid or password");
      // Connect to Wi-Fi network with SSID and password
      Serial.println("Setting AP (Access Point)");
      // NULL sets an open Access Point
      WiFi.softAP("WIFI_MANAGER", "WIFImanager");

      IPAddress IP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(IP);
      wifiManagerInfoPrint();

      // Web Server Root URL
      server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", index_html);
      });

      server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest* request) {
        int params = request->params();
        for (int i = 0; i < params; i++) {
          const AsyncWebParameter* p = request->getParam(i);
          if (p->isPost()) {
            // HTTP POST ssid value
            if (p->name() == PARAM_INPUT_1) {
              ssid = p->value();
              Serial.print("SSID set to: ");
              Serial.println(ssid);
              ssid.trim();
              pref.putString("ssid", ssid);
            }
            // HTTP POST pass value
            if (p->name() == PARAM_INPUT_2) {
              password = p->value();
              Serial.print("Password set to: ");
              Serial.println(password);
              password.trim();
              pref.putString("password", password);
            }
            //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
          }
        }
        request->send(200, "text/plain", "Done. Device will now restart.");
        delay(3000);
        ESP.restart();
      });
      server.begin();
      WiFi.onEvent(WiFiEvent);
      while (true)
        ;
    }

    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname("Nini_GClock");
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("");

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 20);
    u8g2.print("WAITING FOR WIFI");
    u8g2.setCursor(1, 32);
    u8g2.print("TO CONNECT");
    u8g2.sendBuffer();
    /*
  count variable stores the status of WiFi connection. 0 means NOT CONNECTED. 1 means CONNECTED
  */
    bool count = 1;
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_luRS08_tr);
      u8g2.setCursor(1, 20);
      u8g2.print("COULD NOT CONNECT");
      u8g2.setCursor(1, 32);
      u8g2.print("CHECK CONNECTION");
      u8g2.setCursor(1, 44);
      u8g2.print("OR, RESET AND");
      u8g2.setCursor(1, 56);
      u8g2.print("TRY AGAIN");
      u8g2.sendBuffer();
      Serial.println("Connection Failed");
      delay(6000);
      count = 0;
      pref.putString("ssid", "");
      pref.putString("password", "");
      break;
    }
    if (count) {  //if wifi is connected
      Serial.println(ssid);
      Serial.println(WiFi.localIP());
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_luRS08_tr);
      u8g2.setCursor(1, 20);
      u8g2.print("WIFI CONNECTED");
      u8g2.setCursor(1, 42);
      u8g2.print(WiFi.localIP());
      u8g2.sendBuffer();
      delay(4000);

      server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Hi! Please add "
                                         "/update"
                                         " on the above address.");
      });

      ElegantOTA.begin(&server);  // Start ElegantOTA
      // ElegantOTA callbacks
      ElegantOTA.onStart(onOTAStart);
      ElegantOTA.onProgress(onOTAProgress);
      ElegantOTA.onEnd(onOTAEnd);

      server.begin();
      Serial.println("HTTP server started");
    }
  }
  //Wifi related stuff END

  xTaskCreatePinnedToCore(
    loop1,        // Task function.
    "loop1Task",  // name of task.
    10000,        // Stack size of task
    NULL,         // parameter of the task
    1,            // priority of the task
    &loop1Task,   // Task handle to keep track of created task
    0);           // pin task to core 0
  pref.end();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(44, 30);
  u8g2.print("HELLO!");
  u8g2.setCursor(36, 52);
  u8g2.print("NINI");
  u8g2.setFont(u8g2_font_streamline_food_drink_t);
  u8g2.drawUTF8(80, 54, "U+4");  //birthday cake icon
  u8g2.sendBuffer();
  delay(2500);
}
//humidity.relative_humidity
//RUNS ON CORE 0
void loop1(void* pvParameters) {
  for (;;) {
    if ((millis() - lastTime1) > timerDelay1) {  // light sensor based power saving operations
      lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
      float lux;
      while (!lightMeter.measurementReady(true)) {
        yield();
      }
      lux = lightMeter.readLightLevel();

      Serial.println("LUXRaw: ");
      Serial.println(lux);

      if (true) {  //is mute on dark enabled by user
        if (lux <= 2)
          isDark = true;
        else
          isDark = false;
      }
      // Brightness control
      if (true) {
        if (lux == 0)
          analogWrite(lcdBrightnessPin, 1);
        else {
          byte val1 = constrain(lux, 1, 120);      // constrain(number to constrain, lower end, upper end)
          byte val3 = map(val1, 1, 120, 20, 255);  // map(value, fromLow, fromHigh, toLow, toHigh);  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;

          Serial.println("LUX: ");
          Serial.println(val3);
          analogWrite(lcdBrightnessPin, val3);
        }
      }

      lastTime1 = millis();
    }
    if ((millis() - lastTime2) > timerDelay2) {

      aht.getEvent(&humidity, &temp);

      lastTime2 = millis();
    }
    if (!isDark) {  //if mute on dark is not active (or false)
      if (true) {
        if ((minutes == 0) && (seconds == 0)) {
          buzzer(600, 1);
        }
      }
      if (true) {
        if ((minutes == 30) && (seconds == 0)) {
          buzzer(500, 2);
        }
      }
    }
    delay(100);
  }
}

//RUNS ON CORE 1
void loop(void) {
  if (true)
    ElegantOTA.loop();

  while (gps.hdop.hdop() > 30 && gps.satellites.value() < 4) {
    gpsInfo("Waiting for GPS...");
  }

  while (Serial1.available()) {
    if (gps.encode(Serial1.read())) {  // process gps messages
      // when TinyGPSPlus reports new data...
      unsigned long age = gps.time.age();
      if (age < 500) {
        // set the Time according to the latest GPS reading
        setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
        adjustTime(19800);
      }
    }
  }

  days = day();
  months = month();
  years = year();
  hours = hourFormat12();
  minutes = minute();
  seconds = second();

  if (!updateInProgress) {
    if (timeStatus() != timeNotSet) {
      if (now() != prevDisplay) {  //update the display only if the time has changed
        prevDisplay = now();
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
        if (days < 10)
          u8g2.print("0");
        u8g2.print(days);
        byte x = days % 10;
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.setCursor(22, 25);
        if (days == 11 || days == 12 || days == 13)
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
        u8g2.print(monthChar[months - 1]);
        u8g2.setCursor(58, 30);
        u8g2.print(years);
        u8g2.setCursor(102, 30);
        u8g2.print(week[weekday() - 1]);

        u8g2.drawLine(0, 31, 127, 31);

        if (days == 6 && months == 9) {  //special message on birthday
          u8g2.setFont(u8g2_font_6x13_tr);
          u8g2.setCursor(5, 43);
          u8g2.print("HAPPY BIRTHDAY NINI!");

          u8g2.setFont(u8g2_font_logisoso16_tr);
          u8g2.setCursor(15, 63);
          if (hours < 10)
            u8g2.print("0");
          u8g2.print(hours);
          if (pulse == 0)
            u8g2.print(":");
          else
            u8g2.print("");

          u8g2.setCursor(41, 63);
          if (minutes < 10)
            u8g2.print("0");
          u8g2.print(minutes);

          if (pulse == 0)
            u8g2.print(":");
          else
            u8g2.print("");

          u8g2.setCursor(67, 63);
          if (seconds < 10)
            u8g2.print("0");
          u8g2.print(seconds);

          u8g2.setCursor(95, 63);
          if (!isAM())
            u8g2.print("PM");
          else
            u8g2.print("AM");

          u8g2.setFont(u8g2_font_waffle_t_all);
          if (!isDark)                       //if mute on dark is not active (or false)
            u8g2.drawUTF8(5, 54, "\ue271");  //symbol for hourly/half-hourly alarm

          if (true)
            u8g2.drawUTF8(5, 64, "\ue2b5");  //wifi-active symbol

          u8g2.sendBuffer();
        } else {
          u8g2.setFont(u8g2_font_logisoso30_tn);
          u8g2.setCursor(15, 63);
          if (hours < 10)
            u8g2.print("0");
          u8g2.print(hours);
          if (pulse == 0)
            u8g2.print(":");
          else
            u8g2.print("");

          u8g2.setCursor(63, 63);
          if (minutes < 10)
            u8g2.print("0");
          u8g2.print(minutes);

          u8g2.setFont(u8g2_font_tenthinnerguys_tu);
          u8g2.setCursor(105, 42);
          if (seconds < 10)
            u8g2.print("0");
          u8g2.print(seconds);

          u8g2.setCursor(105, 63);
          if (!isAM())
            u8g2.print("PM");
          else
            u8g2.print("AM");

          u8g2.setFont(u8g2_font_waffle_t_all);
          if (!isDark)                         //if mute on dark is not active (or false)
            u8g2.drawUTF8(103, 52, "\ue271");  //symbol for hourly/half-hourly alarm

          if (true)
            u8g2.drawUTF8(112, 52, "\ue2b5");  //wifi-active symbol

          u8g2.sendBuffer();
        }

        if (pulse == 1)
          pulse = 0;
        else if (pulse == 0)
          pulse = 1;
      }
    }
  }
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}

// int delay value, byte count value
void buzzer(int Delay, byte count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(Delay);
    digitalWrite(buzzerPin, LOW);
    delay(Delay);
  }
}

//helper function for retreiving and displaying gps data along with title ("msg")
void gpsInfo(String msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_luRS08_tr);
  u8g2.setCursor(1, 9);
  u8g2.print(msg);
  u8g2.setFont(u8g2_font_5x7_mr);
  u8g2.setCursor(1, 24);
  u8g2.print("Satellites");
  u8g2.setCursor(25, 36);
  u8g2.print(gps.satellites.value());

  u8g2.setCursor(58, 24);
  u8g2.print("HDOP");
  u8g2.setCursor(58, 36);
  u8g2.print(gps.hdop.hdop());

  u8g2.setCursor(92, 24);
  u8g2.print("Speed");
  u8g2.setCursor(86, 36);
  u8g2.print(int(gps.speed.kmph()));
  u8g2.setFont(u8g2_font_micro_tr);
  u8g2.print("kmph");
  u8g2.setFont(u8g2_font_5x7_mr);

  u8g2.setCursor(1, 51);
  u8g2.print("Fix Age");
  u8g2.setCursor(6, 63);
  u8g2.print(gps.time.age());
  u8g2.print("ms");  //

  u8g2.setCursor(42, 51);
  u8g2.print("Altitude");
  u8g2.setCursor(42, 63);
  u8g2.print(gps.altitude.meters());
  u8g2.print("m");

  u8g2.setCursor(88, 51);
  u8g2.print("Lat & Lng");
  u8g2.setCursor(88, 57);
  u8g2.setFont(u8g2_font_4x6_tn);
  u8g2.print(gps.location.lat(), 7);
  u8g2.setCursor(88, 64);
  u8g2.print(gps.location.lng(), 7);

  u8g2.sendBuffer();
  smartDelay(900);
}

//related to wifi manager helping screen
void wifiManagerInfoPrint() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_luRS08_tr);
  u8g2.setCursor(1, 10);
  u8g2.print("Turn ON WiFi");
  u8g2.setCursor(1, 22);
  u8g2.print("on your phone/laptop.");
  u8g2.setCursor(1, 34);
  u8g2.print("Connect to->");
  u8g2.setCursor(1, 46);
  u8g2.print("SSID: WIFI_MANAGER");
  u8g2.setCursor(1, 58);
  u8g2.print("Password: WIFImanager");
  u8g2.sendBuffer();
}
//related to wifi manager helping screen, triggers when user connect to access point of esp32
void WiFiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 10);
    u8g2.print("On browser, go to");
    u8g2.setCursor(1, 22);
    u8g2.print("192.168.4.1/wifi");
    u8g2.setCursor(1, 34);
    u8g2.print("Enter the your Wifi");
    u8g2.setCursor(1, 46);
    u8g2.print("credentials of 2.4Ghz");
    u8g2.setCursor(1, 58);
    u8g2.print("network. Then Submit. ");
    u8g2.sendBuffer();
  }
}

void errorMsgPrint(String device, String msg) {
  while (true) {
    delay(50);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("ERROR: " + device);
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("msg");

    byte i = 5;
    while (i > 0) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_luRS08_tr);
      u8g2.setCursor(60, 51);
      u8g2.print(i);
      u8g2.sendBuffer();
      delay(1000);
      i--;
    }
    break;
  }
}