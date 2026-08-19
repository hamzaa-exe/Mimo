/*
  ============================================================================
  MIMO — Cute Desktop Robot Firmware  (v2 — SSD1309 / U8g2)
  Board: Seeed XIAO ESP32-S3
  ============================================================================

  HARDWARE (matches your schematic):
    - OLED:      SSD1309, 128x64, I2C
                 SDA = GPIO5, SCL = GPIO6
    - Touch:     TTP223 digital output -> GPIO1 (D0)
    - Amp:       MAX98357A (I2S)
                 BCLK = GPIO7, LRC = GPIO8, DIN = GPIO9
                 (AMP_GAIN tied to GND, AMP_SD tied to 3V3 on your board —
                  both hardwired, no GPIO control needed)
    - Audio:     WAV files (mono, 16-bit PCM, 16000 Hz) stored in LittleFS
                 under /audio/*.wav

  REQUIRED LIBRARIES (Arduino Library Manager):
    - U8g2 (by olikraus)              <-- replaces Adafruit_GFX/SSD1306
    - ArduinoJson (by Benoit Blanchon)
    - ESP8266Audio (by Earle Philhower) -> AudioFileSourceLittleFS,
      AudioGeneratorWAV, AudioOutputI2S (works fine on ESP32)
    - Built-in: WiFi, HTTPClient, LittleFS, time.h

  NOTE ON THE U8G2 CONSTRUCTOR:
    Some SSD1309 panels need the "NONAME0" init sequence, others need
    "NONAME2" (garbled/mirrored display = wrong variant). If your screen
    looks wrong, just swap the class name below to:
      U8G2_SSD1309_128X64_NONAME2_F_HW_I2C
    everything else stays identical.

  UPLOADING AUDIO FILES:
    Use the "ESP32 Sketch Data Upload" tool (LittleFS uploader plugin) to
    push a /data folder (containing /audio/*.wav) to the board.

  STATE MACHINE:
    IDLE  --touch--> TIME  --touch--> WEATHER  --touch--> IDLE ...
  ============================================================================
*/

#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <LittleFS.h>

#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

// ============================================================================
// CONFIG — matches your schematic. Only the WiFi/weather values need editing.
// ============================================================================

// ---- OLED ----
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_SDA_PIN    5
#define OLED_SCL_PIN    6

// ---- Touch sensor ----
#define TOUCH_PIN       1

// ---- I2S / MAX98357A ----
#define I2S_BCLK_PIN    7
#define I2S_LRC_PIN     8
#define I2S_DIN_PIN     9

// ---- Wi-Fi ----
// NOTE: for a real product, move these into a separate secrets.h (gitignored)
// or a captive-portal WiFiManager. Kept here for simplicity of this sketch.
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---- Weather (OpenWeatherMap) ----
const char* WEATHER_API_KEY = "YOUR_OPENWEATHERMAP_API_KEY";
const char* WEATHER_CITY    = "Rawalpindi";
const char* WEATHER_COUNTRY = "PK";
const char* WEATHER_UNITS   = "metric";  // "metric" = Celsius, "imperial" = F

// ---- Time / NTP ----
const char* NTP_SERVER      = "pool.ntp.org";
const long  GMT_OFFSET_SEC  = 5 * 3600;   // Pakistan Standard Time = UTC+5
const int   DST_OFFSET_SEC  = 0;

// ---- Timings (ms) ----
const unsigned long TOUCH_DEBOUNCE_MS        = 250;
const unsigned long TIME_MODE_DURATION_MS    = 6000;
const unsigned long WEATHER_MODE_DURATION_MS = 8000;
const unsigned long WEATHER_CACHE_MS         = 5UL * 60UL * 1000UL; // 5 min
const unsigned long IDLE_SOUND_MIN_MS        = 15000;
const unsigned long IDLE_SOUND_MAX_MS        = 45000;

// ============================================================================
// DISPLAY — U8g2, full frame buffer, hardware I2C
// ============================================================================
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ============================================================================
// GLOBALS
// ============================================================================
enum RobotState { STATE_IDLE, STATE_TIME, STATE_WEATHER };
RobotState currentState = STATE_IDLE;
unsigned long stateEnteredAt = 0;

// ---- Touch handling ----
bool touchLastRaw = false;
bool touchStable = false;
unsigned long touchLastChangeAt = 0;

// ---- Wi-Fi state ----
bool wifiConnected = false;
unsigned long lastWifiAttemptAt = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 15000;

// ---- Weather cache ----
struct WeatherData {
  bool valid = false;
  float tempC = 0;
  String condition = "";
  String icon = "";
  unsigned long fetchedAt = 0;
};
WeatherData weather;

// ---- Eye animation state ----
enum EyeMood { EYE_NORMAL, EYE_HAPPY, EYE_SLEEPY, EYE_SURPRISED };
EyeMood eyeMood = EYE_NORMAL;

float eyeOpenAmount = 1.0;      // 0 = fully closed, 1 = fully open (for blinking)
int   eyeOffsetX = 0;           // pupil look direction
int   eyeOffsetY = 0;
unsigned long nextBlinkAt = 0;
unsigned long nextLookAt = 0;
unsigned long nextMoodChangeAt = 0;
bool  blinking = false;
unsigned long blinkStartedAt = 0;
const unsigned long BLINK_DURATION_MS = 180;

// ---- Idle random sound ----
unsigned long nextIdleSoundAt = 0;

// ---- Audio playback (non-blocking) ----
AudioGeneratorWAV *audioGen = nullptr;
AudioFileSourceLittleFS *audioFile = nullptr;
AudioOutputI2S *audioOut = nullptr;
bool audioBusy = false;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void connectWiFi();
void handleTouch();
void onTouchDetected();
void updateEyes();
void drawEyes();
void enterState(RobotState newState);
void showTime();
void fetchAndShowWeather();
bool fetchWeather();
void showWeather();
void drawWeatherGlyph(int cx, int cy);
void playSound(const char* path);
void updateAudio();
void maybePlayIdleSound();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(TOUCH_PIN, INPUT);

  // ---- OLED init ----
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  // ---- LittleFS for audio ----
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed. Audio playback will be disabled.");
  }

  // ---- Audio output object (created once, reused) ----
  audioOut = new AudioOutputI2S();
  audioOut->SetPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
  audioOut->SetGain(0.6); // adjust volume 0.0–1.0

  // ---- Wi-Fi (bounded-wait attempt kicked off here) ----
  WiFi.mode(WIFI_STA);
  connectWiFi();

  // ---- Seed random for organic eye behavior ----
  randomSeed(esp_random());
  nextBlinkAt      = millis() + random(2000, 5000);
  nextLookAt       = millis() + random(3000, 7000);
  nextMoodChangeAt = millis() + random(8000, 15000);
  nextIdleSoundAt  = millis() + random(IDLE_SOUND_MIN_MS, IDLE_SOUND_MAX_MS);

  stateEnteredAt = millis();

  // Greeting sound on boot (non-blocking; silently skipped if file missing)
  playSound("/audio/hello.wav");
}

// ============================================================================
// MAIN LOOP — everything here must be non-blocking
// ============================================================================
void loop() {
  unsigned long now = millis();

  if (!wifiConnected && (now - lastWifiAttemptAt > WIFI_RETRY_INTERVAL_MS)) {
    connectWiFi();
  }

  handleTouch();
  updateAudio();

  switch (currentState) {
    case STATE_IDLE:
      updateEyes();
      drawEyes();
      maybePlayIdleSound();
      break;

    case STATE_TIME:
      showTime();
      if (now - stateEnteredAt > TIME_MODE_DURATION_MS) {
        enterState(STATE_IDLE);
      }
      break;

    case STATE_WEATHER:
      showWeather();
      if (now - stateEnteredAt > WEATHER_MODE_DURATION_MS) {
        enterState(STATE_IDLE);
      }
      break;
  }
}

// ============================================================================
// STATE MACHINE HELPERS
// ============================================================================
void enterState(RobotState newState) {
  currentState = newState;
  stateEnteredAt = millis();

  if (newState == STATE_TIME) {
    playSound("/audio/touch.wav");
  } else if (newState == STATE_WEATHER) {
    playSound("/audio/weather.wav");
    fetchAndShowWeather(); // uses cache if fresh, else fetches
  }
}

// ============================================================================
// TOUCH HANDLING (debounced, edge-triggered, one action per physical touch)
// ============================================================================
void handleTouch() {
  bool raw = digitalRead(TOUCH_PIN) == HIGH;
  unsigned long now = millis();

  if (raw != touchLastRaw) {
    touchLastChangeAt = now;
    touchLastRaw = raw;
  }

  if ((now - touchLastChangeAt) > TOUCH_DEBOUNCE_MS && raw != touchStable) {
    touchStable = raw;

    // Fire only on rising edge (finger just touched down) so holding
    // the sensor doesn't repeatedly cycle modes.
    if (touchStable == true) {
      onTouchDetected();
    }
  }
}

void onTouchDetected() {
  switch (currentState) {
    case STATE_IDLE:
      enterState(STATE_TIME);
      break;
    case STATE_TIME:
      enterState(STATE_WEATHER);
      break;
    case STATE_WEATHER:
      enterState(STATE_IDLE);
      break;
  }
}

// ============================================================================
// WI-FI
// ============================================================================
void connectWiFi() {
  lastWifiAttemptAt = millis();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    return;
  }

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Bounded wait so we never freeze animations for long; a few hundred ms
  // is an acceptable one-time hit at boot, retries afterward are non-blocking.
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(50);
  }

  wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (wifiConnected) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
  } else {
    Serial.println("WiFi connection failed — will retry later.");
  }
}

// ============================================================================
// TIME MODE
// ============================================================================
void showTime() {
  u8g2.clearBuffer();

  if (!wifiConnected) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(20, 34, "No WiFi :(");
    u8g2.sendBuffer();
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(10, 34, "Getting time...");
    u8g2.sendBuffer();
    return;
  }

  int hour12 = timeinfo.tm_hour % 12;
  if (hour12 == 0) hour12 = 12;
  bool isPM = timeinfo.tm_hour >= 12;

  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, timeinfo.tm_min, isPM ? "PM" : "AM");

  u8g2.setFont(u8g2_font_logisoso18_tr);
  int w = u8g2.getStrWidth(buf);
  u8g2.drawStr((OLED_WIDTH - w) / 2, 40, buf);

  u8g2.sendBuffer();
}

// ============================================================================
// WEATHER MODE
// ============================================================================
void fetchAndShowWeather() {
  unsigned long now = millis();
  if (!weather.valid || (now - weather.fetchedAt > WEATHER_CACHE_MS)) {
    fetchWeather();
  }
  showWeather();
}

bool fetchWeather() {
  if (!wifiConnected) return false;

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=";
  url += WEATHER_CITY;
  url += ",";
  url += WEATHER_COUNTRY;
  url += "&units=";
  url += WEATHER_UNITS;
  url += "&appid=";
  url += WEATHER_API_KEY;

  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("Weather fetch failed, HTTP code: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("Weather JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  weather.tempC = doc["main"]["temp"] | 0.0;
  weather.condition = String((const char*)(doc["weather"][0]["main"] | "Unknown"));
  weather.icon = String((const char*)(doc["weather"][0]["icon"] | ""));
  weather.valid = true;
  weather.fetchedAt = millis();
  return true;
}

// Draws a tiny procedural weather glyph based on the OWM icon code prefix.
void drawWeatherGlyph(int cx, int cy) {
  bool sunny  = weather.icon.startsWith("01");
  bool cloudy = weather.icon.startsWith("02") || weather.icon.startsWith("03") || weather.icon.startsWith("04");
  bool rainy  = weather.icon.startsWith("09") || weather.icon.startsWith("10");

  if (sunny) {
    u8g2.drawDisc(cx, cy, 8);
    for (int i = 0; i < 8; i++) {
      float a = i * (PI / 4);
      int x1 = cx + cos(a) * 12, y1 = cy + sin(a) * 12;
      int x2 = cx + cos(a) * 16, y2 = cy + sin(a) * 16;
      u8g2.drawLine(x1, y1, x2, y2);
    }
  } else if (rainy) {
    u8g2.drawRBox(cx - 12, cy - 8, 24, 12, 5);
    for (int i = -8; i <= 8; i += 8) {
      u8g2.drawLine(cx + i, cy + 6, cx + i - 2, cy + 12);
    }
  } else if (cloudy) {
    u8g2.drawRBox(cx - 12, cy - 6, 24, 12, 6);
    u8g2.drawDisc(cx - 6, cy - 6, 7);
    u8g2.drawDisc(cx + 4, cy - 8, 8);
  } else {
    u8g2.drawCircle(cx, cy, 10);
  }
}

void showWeather() {
  u8g2.clearBuffer();

  if (!wifiConnected) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(20, 34, "No WiFi :(");
    u8g2.sendBuffer();
    return;
  }

  if (!weather.valid) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(5, 34, "Fetching weather...");
    u8g2.sendBuffer();
    return;
  }

  drawWeatherGlyph(OLED_WIDTH / 2, 16);

  // Temperature, e.g. "31" drawn big, degree circle + "C" drawn manually
  // (avoids relying on a font's degree-symbol glyph being present)
  char tempBuf[8];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", weather.tempC);

  u8g2.setFont(u8g2_font_logisoso18_tr);
  int tempW = u8g2.getStrWidth(tempBuf);
  int cW = u8g2.getStrWidth("C");
  int totalW = tempW + 14 + cW; // + gap for degree circle
  int startX = (OLED_WIDTH - totalW) / 2;

  u8g2.drawStr(startX, 46, tempBuf);
  u8g2.drawCircle(startX + tempW + 6, 30, 3);      // degree mark
  u8g2.drawStr(startX + tempW + 14, 46, "C");

  u8g2.setFont(u8g2_font_6x10_tf);
  int condW = u8g2.getStrWidth(weather.condition.c_str());
  u8g2.drawStr((OLED_WIDTH - condW) / 2, 60, weather.condition.c_str());

  u8g2.sendBuffer();
}

// ============================================================================
// EYE ANIMATION (idle mode) — all timing based on millis(), nothing blocks
// ============================================================================
void updateEyes() {
  unsigned long now = millis();

  // --- Blinking ---
  if (!blinking && now >= nextBlinkAt) {
    blinking = true;
    blinkStartedAt = now;
  }
  if (blinking) {
    unsigned long elapsed = now - blinkStartedAt;
    float half = BLINK_DURATION_MS / 2.0;
    if (elapsed < half) {
      eyeOpenAmount = 1.0 - (elapsed / half);
    } else if (elapsed < BLINK_DURATION_MS) {
      eyeOpenAmount = (elapsed - half) / half;
    } else {
      blinking = false;
      eyeOpenAmount = 1.0;
      unsigned long nextGap = random(0, 10) > 8 ? random(150, 400) : random(2500, 7000);
      nextBlinkAt = now + nextGap;
    }
  }

  // --- Occasional look left/right/up/down ---
  if (now >= nextLookAt) {
    int dir = random(0, 5);
    switch (dir) {
      case 0: eyeOffsetX = 0;  eyeOffsetY = 0;  break; // center
      case 1: eyeOffsetX = -6; eyeOffsetY = 0;  break; // left
      case 2: eyeOffsetX = 6;  eyeOffsetY = 0;  break; // right
      case 3: eyeOffsetX = 0;  eyeOffsetY = -4; break; // up
      case 4: eyeOffsetX = 0;  eyeOffsetY = 4;  break; // down
    }
    nextLookAt = now + random(1500, 4000);
  }

  // --- Occasional mood change ---
  if (now >= nextMoodChangeAt) {
    int m = random(0, 10);
    if (m < 6) eyeMood = EYE_NORMAL;
    else if (m < 8) eyeMood = EYE_HAPPY;
    else if (m < 9) eyeMood = EYE_SLEEPY;
    else eyeMood = EYE_SURPRISED;
    nextMoodChangeAt = now + random(8000, 18000);
  }
}

void drawEyes() {
  u8g2.clearBuffer();

  int eyeW = 26;
  int eyeH = 34;
  int gap  = 20;
  int cx = OLED_WIDTH / 2;
  int cy = OLED_HEIGHT / 2;

  int leftX  = cx - gap / 2 - eyeW / 2 + eyeOffsetX;
  int rightX = cx + gap / 2 - eyeW / 2 + eyeOffsetX;
  int y = cy - eyeH / 2 + eyeOffsetY;

  float openness = eyeOpenAmount;
  if (eyeMood == EYE_SLEEPY) openness *= 0.45;      // droopy eyes
  if (eyeMood == EYE_SURPRISED) openness = min(1.3f, openness * 1.3f);

  int drawH = (int)(eyeH * openness);
  int drawY = y + (eyeH - drawH) / 2;

  int radius = 8;
  if (eyeMood == EYE_SURPRISED) radius = 12; // rounder = more "wide-eyed"
  radius = min(radius, max(1, drawH / 2)); // keep radius sane when nearly closed

  u8g2.drawRBox(leftX,  drawY, eyeW, max(2, drawH), radius);
  u8g2.drawRBox(rightX, drawY, eyeW, max(2, drawH), radius);

  // Happy mood: carve a curved "smile bump" out of the bottom of each eye
  if (eyeMood == EYE_HAPPY && openness > 0.5) {
    u8g2.setDrawColor(0);
    u8g2.drawRBox(leftX - 2,  drawY + drawH - 6, eyeW + 4, 10, 6);
    u8g2.drawRBox(rightX - 2, drawY + drawH - 6, eyeW + 4, 10, 6);
    u8g2.setDrawColor(1);
  }

  u8g2.sendBuffer();
}

// ============================================================================
// AUDIO PLAYBACK — non-blocking WAV playback from LittleFS
// ============================================================================
void playSound(const char* path) {
  if (!LittleFS.exists(path)) {
    // Silently skip if the audio asset hasn't been uploaded yet.
    return;
  }

  if (audioGen && audioGen->isRunning()) {
    audioGen->stop();
  }
  delete audioGen;
  delete audioFile;

  audioFile = new AudioFileSourceLittleFS(path);
  audioGen  = new AudioGeneratorWAV();
  audioGen->begin(audioFile, audioOut);
  audioBusy = true;
}

void updateAudio() {
  if (audioGen && audioGen->isRunning()) {
    if (!audioGen->loop()) {
      audioGen->stop();
      audioBusy = false;
    }
  } else {
    audioBusy = false;
  }
}

void maybePlayIdleSound() {
  unsigned long now = millis();
  if (now >= nextIdleSoundAt && !audioBusy) {
    playSound("/audio/happy.wav");
    nextIdleSoundAt = now + random(IDLE_SOUND_MIN_MS, IDLE_SOUND_MAX_MS);
  }
}
