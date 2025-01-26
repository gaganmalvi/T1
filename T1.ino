// T1 by Kailash (kaliash@ampere.works), Amit (amit@absurd.industries) and Amartha (amartha@absurd.industries)
// Based off of GxEPD2_HelloWorld.ino by Jean-Marc Zingg

#include <ESP32Time.h>
#include <WiFi.h>
#include <GxEPD2_BW.h>
#include <HTTPClient.h>
#include <Preferences.h> // https://github.com/espressif/arduino-esp32/blob/master/libraries/Preferences/src/Preferences.h

// Fonts
#include "resources/fonts/Outfit_28pt7b.h"
#include "resources/fonts/Outfit_14pt7b.h"
#include "resources/fonts/Outfit_10pt7b.h"

// Icons
#include "resources/icons/all.h"

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 55          /* Time ESP32 will go to sleep (in seconds) */

String ssid;
String password;

// Weather
WiFiClient client;
HTTPClient http;

// Interval for weather updates (24 hours in milliseconds)
const unsigned long updateInterval = 24 * 60 * 60 * 1000;

const char* weatherUrl = "https://wttr.in/Bengaluru?format=%t";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 5.5;
const int daylightOffset_sec = 3600 * 0;

GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT> display(GxEPD2_154_GDEY0154D67(/*CS=5*/ 5, /*DC=*/20, /*RST=*/21, /*BUSY=*/9));  // GDEY0154D67 200x200, SSD1681, (FPC-B001 20.05.21)
ESP32Time rtc(0);

RTC_DATA_ATTR int bootCount = 0;
Preferences preferences;

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void getWeatherData() {
  unsigned long currentTime = millis();
  preferences.begin("t1", false);
  unsigned long lastUpdateTime = preferences.getULong("lastUpdateTime");
  // Check if it's time to update the weather
  if (currentTime - lastUpdateTime >= updateInterval || lastUpdateTime == 0) {
    Serial.println("[WEATHER] Check init");
    // Fetch and print the weather
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(weatherUrl); // Initialize HTTP request
      int httpResponseCode = http.GET();

      if (httpResponseCode == 200) { // HTTP OK
        String weather = http.getString();
        Serial.println("Success update weather");
        preferences.putULong("lastUpdateTime", currentTime);
        preferences.putString("weather", weather);
      } else {
        Serial.print("Error fetching weather: ");
        Serial.println(httpResponseCode);
      }
      http.end(); // Close connection

    } else {
      Serial.println("Wi-Fi disconnected. Reconnecting...");
      WiFi.begin(ssid, password);
    }
  } else {
    Serial.println("[WEATHER] Need to wait 24h");
  }
  preferences.end();
}

void storeWiFiCredentials(const char* ssid, const char* password) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
}

void retrieveWiFiCredentials() {
  preferences.begin("wifi", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
}

void storeTime(unsigned long timeEpoch) {
  preferences.begin("t1", false);
  preferences.putULong("epoch", timeEpoch);
  preferences.putULong("updateOn", timeEpoch + 3600);
  preferences.end();
}

void retrieveTime() {
  preferences.begin("t1", false);
  unsigned long epoch = preferences.getULong("epoch");
  unsigned long updateOn = preferences.getULong("updateOn");
  
  if (epoch < updateOn) {
    // epoch += 60;
    rtc.setTime(epoch);
  }
}

void getTimeOverInternet() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
  }
  storeTime(rtc.getEpoch());
}


void connectToWiFi() {
  retrieveWiFiCredentials(); // Fetch stored credentials
  WiFi.mode(WIFI_STA);
  Serial.println("[WIFI] SETUP");
  WiFi.begin("SSID", "PWD");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
}

void factoryReset() {
  storeWiFiCredentials("SSID", "PWD");
  preferences.begin("t1", false);
  preferences.putULong("epoch", 0);
  preferences.putULong("updateOn", 3600);
  preferences.putULong("lastUpdateTime", 0);
  preferences.end();
}

void setup() {
  // Basic initialization
  Serial.begin(115200);

  Serial.println("[INIT] Initialize watch");
  print_wakeup_reason();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  factoryReset();

  preferences.begin("t1", false);
  unsigned long epoch = preferences.getULong("epoch", 0);
  unsigned long updateOn = preferences.getULong("updateOn", 0);

  if (epoch == 0 || epoch > updateOn) {
    Serial.println("[INIT] Connect to WiFi");
    connectToWiFi();
    Serial.println("[INIT] Get Time");
    getWeatherData();
    getTimeOverInternet();
    WiFi.disconnect();
  }
  
  retrieveTime();  
}

void printLeftString(String buf, int x, int y) {
  display.setCursor(x, y);
  display.print(buf);
}

void printRightString(String buf, int x, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, x, y, &x1, &y1, &w, &h);
  display.setCursor(x - w, y);
  display.print(buf);
}

void printCenterString(String buf, int x, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, x, y, &x1, &y1, &w, &h);
  display.setCursor(x - w / 2, y);
  display.print(buf);
}

void displayTime() {
  // retrieveTime();
  preferences.begin("t1", true);
  String weather = preferences.getString("weather", "+OOC");
  Serial.println(weather);
  preferences.end();
  // Print UI
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    display.setFont(&Outfit_ExtraBold28pt7b);
    printCenterString(rtc.getTime("%R"), 117, 125);

    display.setFont(&Outfit_SemiBold14pt7b);
    printCenterString(rtc.getTime("%A"), 100, 30);    // Wednesday 
    
    display.setFont(&Outfit_SemiBold10pt7b);
    printCenterString(rtc.getTime("%d %b, %Y"), 140, 60);    // 12

    display.setFont(&Outfit_SemiBold10pt7b);
    String jtagConnected = usb_serial_jtag_is_connected() ? "Debug Mode" : "";
    printCenterString(jtagConnected, 140, 155);    // 12

    display.drawBitmap(40, 170, icon_weather_small, 28, 28, GxEPD_BLACK);
    printCenterString(weather, 130, 190);
  } while (display.nextPage());
}

void loop() {
    display.init(115200, true, 2, false);
    displayTime();
    display.hibernate();
    if (!usb_serial_jtag_is_connected()) {
      Serial.println("[DS] USB not plugged in, go to deep sleep");
      esp_deep_sleep_start();
    } else {
      Serial.println("[JTAG] JTAG connected, not deep sleep");
      delay(TIME_TO_SLEEP * 1000); // Simulate deepsleep. Helps us for debugging in case we solder it off. 
    }
}
