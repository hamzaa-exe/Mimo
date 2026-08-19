#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include "driver/i2s_std.h"


// ========================================================
// USER SETTINGS
// ========================================================

// ---------- WiFi ----------
const char* WIFI_SSID     = "Test";
const char* WIFI_PASSWORD = "12345678";

// ---------- Weather ----------
const char* CITY = "Rawalpindi";
const char* COUNTRY_CODE = "PK";

// Open-Meteo does not require an API key.
// The program obtains latitude/longitude from the
// Open-Meteo geocoding service.

// ---------- Your location ----------
float LATITUDE  = 33.5651;
float LONGITUDE = 73.0169;


// ========================================================
// PIN DEFINITIONS
// ========================================================

// OLED
#define OLED_SDA_PIN 5     // XIAO D4
#define OLED_SCL_PIN 6     // XIAO D5

// Touch
#define TOUCH_PIN 1        // XIAO D0 = GPIO1

// MAX98357A I2S
// Change these three if your physical wiring is different.
#define I2S_BCLK_PIN 43    // XIAO D6
#define I2S_LRC_PIN  44    // XIAO D7
#define I2S_DIN_PIN   7    // XIAO D8


// ========================================================
// OLED
// ========================================================

// SSD1309 128x64 I2C
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oled(
  U8G2_R0,
  U8X8_PIN_NONE
);


// ========================================================
// I2S
// ========================================================

i2s_chan_handle_t tx_handle;


// ========================================================
// MIMO STATES
// ========================================================

enum RobotMode
{
  MODE_ANIMATION,
  MODE_TIME,
  MODE_WEATHER
};

RobotMode mode = MODE_ANIMATION;


// ========================================================
// TIMING
// ========================================================

unsigned long lastTouch = 0;
unsigned long screenStart = 0;
unsigned long lastAnimation = 0;

const unsigned long TOUCH_DELAY = 500;
const unsigned long INFO_TIMEOUT = 8000;


// ========================================================
// ANIMATION
// ========================================================

int animationFrame = 0;

enum Emotion
{
  HAPPY,
  NORMAL,
  LOVE,
  SLEEPY,
  SURPRISED,
  ANGRY
};

Emotion currentEmotion = NORMAL;


// ========================================================
// WEATHER DATA
// ========================================================

String weatherText = "Loading...";
float temperature = 0;

bool weatherAvailable = false;


// ========================================================
// I2S INITIALIZATION
// ========================================================

void setupI2S()
{
  i2s_chan_config_t chan_cfg =
    I2S_CHANNEL_DEFAULT_CONFIG(
      I2S_NUM_AUTO,
      I2S_ROLE_MASTER
    );

  i2s_new_channel(
    &chan_cfg,
    &tx_handle,
    NULL
  );

  i2s_std_config_t std_cfg = {

    .clk_cfg =
      I2S_STD_CLK_DEFAULT_CONFIG(22050),

    .slot_cfg =
      I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_MONO
      ),

    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_BCLK_PIN,
      .ws   = (gpio_num_t)I2S_LRC_PIN,
      .dout = (gpio_num_t)I2S_DIN_PIN,
      .din  = I2S_GPIO_UNUSED
    }
  };

  i2s_channel_init_std_mode(
    tx_handle,
    &std_cfg
  );

  i2s_channel_enable(tx_handle);
}


// ========================================================
// PLAY SIMPLE CUTE SOUND
// ========================================================

void playTone(
  float frequency,
  int duration,
  float volume = 0.15
)
{
  const int sampleRate = 22050;

  int samples =
    sampleRate * duration / 1000;

  int16_t* buffer =
    (int16_t*)malloc(samples * sizeof(int16_t));

  if (!buffer)
    return;

  for (int i = 0; i < samples; i++)
  {
    float t =
      (float)i / sampleRate;

    float wave =
      sin(2.0 * PI * frequency * t);

    buffer[i] =
      (int16_t)(
        wave *
        32767 *
        volume
      );
  }

  size_t bytesWritten;

  i2s_channel_write(
    tx_handle,
    buffer,
    samples * sizeof(int16_t),
    &bytesWritten,
    portMAX_DELAY
  );

  free(buffer);
}


// ========================================================
// MIMO VOICE
// ========================================================

void mimoHello()
{
  playTone(700, 100);
  playTone(900, 120);
  playTone(1200, 180);
}

void mimoHappy()
{
  playTone(900, 80);
  playTone(1200, 100);
  playTone(1500, 180);
}

void mimoTouchSound()
{
  playTone(1000, 70);
  playTone(1400, 100);
}

void mimoWeatherSound()
{
  playTone(700, 100);
  playTone(850, 100);
  playTone(1100, 180);
}


// ========================================================
// DRAW EYES
// ========================================================

void drawNormalEyes()
{
  oled.clearBuffer();

  // left eye
  oled.drawRBox(
    25,
    20,
    28,
    25,
    7
  );

  // right eye
  oled.drawRBox(
    75,
    20,
    28,
    25,
    7
  );

  oled.sendBuffer();
}


// ========================================================
// HAPPY EYES
// ========================================================

void drawHappyEyes()
{
  oled.clearBuffer();

  // left happy eye
  oled.drawDisc(
    39,
    32,
    15
  );

  // right happy eye
  oled.drawDisc(
    89,
    32,
    15
  );

  // black lower cuts to make smile-like eyes
  oled.setDrawColor(0);

  oled.drawBox(
    22,
    32,
    34,
    20
  );

  oled.drawBox(
    72,
    32,
    34,
    20
  );

  oled.setDrawColor(1);

  oled.sendBuffer();
}


// ========================================================
// LOVE EYES
// ========================================================

void drawLoveEyes()
{
  oled.clearBuffer();

  // left heart
  oled.drawDisc(32, 29, 8);
  oled.drawDisc(45, 29, 8);
  oled.drawTriangle(
    24, 33,
    53, 33,
    38, 48
  );

  // right heart
  oled.drawDisc(78, 29, 8);
  oled.drawDisc(91, 29, 8);
  oled.drawTriangle(
    70, 33,
    99, 33,
    85, 48
  );

  oled.sendBuffer();
}


// ========================================================
// SURPRISED
// ========================================================

void drawSurprised()
{
  oled.clearBuffer();

  oled.drawCircle(
    39,
    32,
    14
  );

  oled.drawCircle(
    89,
    32,
    14
  );

  oled.sendBuffer();
}


// ========================================================
// SLEEPY
// ========================================================

void drawSleepy()
{
  oled.clearBuffer();

  oled.drawLine(
    22, 34,
    54, 34
  );

  oled.drawLine(
    74, 34,
    106, 34
  );

  oled.sendBuffer();
}


// ========================================================
// ANGRY
// ========================================================

void drawAngry()
{
  oled.clearBuffer();

  oled.drawLine(
    22, 22,
    54, 30
  );

  oled.drawLine(
    74, 30,
    106, 22
  );

  oled.drawRBox(
    27,
    30,
    25,
    15,
    4
  );

  oled.drawRBox(
    76,
    30,
    25,
    15,
    4
  );

  oled.sendBuffer();
}


// ========================================================
// RANDOM EMOTION
// ========================================================

void randomEmotion()
{
  int e = random(0, 5);

  switch (e)
  {
    case 0:
      currentEmotion = NORMAL;
      break;

    case 1:
      currentEmotion = HAPPY;
      break;

    case 2:
      currentEmotion = LOVE;
      break;

    case 3:
      currentEmotion = SLEEPY;
      break;

    case 4:
      currentEmotion = SURPRISED;
      break;
  }
}


// ========================================================
// ANIMATION LOOP
// ========================================================

void animateMimo()
{
  if (
    millis() -
    lastAnimation <
    1200
  )
    return;

  lastAnimation = millis();

  animationFrame++;

  if (animationFrame % 5 == 0)
  {
    randomEmotion();
  }

  switch (currentEmotion)
  {
    case NORMAL:
      drawNormalEyes();
      break;

    case HAPPY:
      drawHappyEyes();
      break;

    case LOVE:
      drawLoveEyes();
      break;

    case SLEEPY:
      drawSleepy();
      break;

    case SURPRISED:
      drawSurprised();
      break;

    case ANGRY:
      drawAngry();
      break;
  }
}


// ========================================================
// SHOW TIME
// ========================================================

void showTime()
{
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo))
  {
    oled.clearBuffer();

    oled.setFont(
      u8g2_font_6x12_tf
    );

    oled.drawStr(
      20,
      30,
      "No time :("
    );

    oled.sendBuffer();

    return;
  }

  char timeString[20];

  strftime(
    timeString,
    sizeof(timeString),
    "%H:%M",
    &timeinfo
  );

  char dateString[30];

  strftime(
    dateString,
    sizeof(dateString),
    "%d/%m/%Y",
    &timeinfo
  );

  oled.clearBuffer();

  oled.setFont(
    u8g2_font_6x12_tf
  );

  oled.drawStr(
    48,
    12,
    "TIME"
  );

  oled.setFont(
    u8g2_font_logisoso24_tf
  );

  oled.drawStr(
    18,
    43,
    timeString
  );

  oled.setFont(
    u8g2_font_6x12_tf
  );

  oled.drawStr(
    39,
    59,
    dateString
  );

  oled.sendBuffer();
}


// ========================================================
// WEATHER
// ========================================================

void getWeather()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    weatherAvailable = false;
    return;
  }

  HTTPClient http;

  String url =
    "https://api.open-meteo.com/v1/forecast?"
    "latitude=" +
    String(LATITUDE, 4) +
    "&longitude=" +
    String(LONGITUDE, 4) +
    "&current=temperature_2m,weather_code"
    "&timezone=auto";

  http.begin(url);

  int httpCode =
    http.GET();

  if (httpCode == 200)
  {
    String payload =
      http.getString();

    JsonDocument doc;

    DeserializationError error =
      deserializeJson(
        doc,
        payload
      );

    if (!error)
    {
      temperature =
        doc["current"]
           ["temperature_2m"];

      int weatherCode =
        doc["current"]
           ["weather_code"];

      weatherText =
        weatherCodeToText(
          weatherCode
        );

      weatherAvailable = true;
    }
  }

  http.end();
}


// ========================================================
// WEATHER CODE CONVERSION
// ========================================================

String weatherCodeToText(
  int code
)
{
  if (code == 0)
    return "Sunny";

  if (code <= 3)
    return "Cloudy";

  if (code <= 48)
    return "Foggy";

  if (code <= 67)
    return "Rain";

  if (code <= 77)
    return "Snow";

  if (code <= 82)
    return "Showers";

  if (code <= 86)
    return "Snow";

  return "Storm";
}


// ========================================================
// SHOW WEATHER
// ========================================================

void showWeather()
{
  if (!weatherAvailable)
  {
    oled.clearBuffer();

    oled.setFont(
      u8g2_font_6x12_tf
    );

    oled.drawStr(
      20,
      25,
      "Weather unavailable"
    );

    oled.drawStr(
      28,
      45,
      "Check WiFi :("
    );

    oled.sendBuffer();

    return;
  }

  oled.clearBuffer();

  oled.setFont(
    u8g2_font_6x12_tf
  );

  oled.drawStr(
    42,
    12,
    "WEATHER"
  );

  oled.setFont(
    u8g2_font_logisoso24_tf
  );

  String temp =
    String(
      temperature,
      1
    ) +
    "C";

  oled.drawStr(
    25,
    43,
    temp.c_str()
  );

  oled.setFont(
    u8g2_font_6x12_tf
  );

  oled.drawStr(
    42,
    59,
    weatherText.c_str()
  );

  oled.sendBuffer();
}


// ========================================================
// WIFI
// ========================================================

void connectWiFi()
{
  oled.clearBuffer();

  oled.setFont(
    u8g2_font_6x12_tf
  );

  oled.drawStr(
    25,
    30,
    "Connecting WiFi..."
  );

  oled.sendBuffer();

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 30
  )
  {
    delay(500);
    attempts++;
  }

  oled.clearBuffer();

  if (WiFi.status() == WL_CONNECTED)
  {
    oled.drawStr(
      32,
      30,
      "WiFi OK!"
    );

    oled.sendBuffer();

    delay(800);
  }
  else
  {
    oled.drawStr(
      25,
      30,
      "WiFi failed :("
    );

    oled.sendBuffer();

    delay(800);
  }
}


// ========================================================
// NTP TIME
// ========================================================

void setupTime()
{
  // Pakistan UTC+5
  configTime(
    5 * 3600,
    0,
    "pool.ntp.org",
    "time.nist.gov"
  );
}


// ========================================================
// TOUCH
// ========================================================

void touchPressed()
{
  if (
    millis() -
    lastTouch <
    TOUCH_DELAY
  )
    return;

  lastTouch = millis();

  mimoTouchSound();

  if (mode == MODE_ANIMATION)
  {
    mode = MODE_TIME;

    screenStart =
      millis();

    showTime();
  }

  else if (mode == MODE_TIME)
  {
    mode = MODE_WEATHER;

    screenStart =
      millis();

    getWeather();

    mimoWeatherSound();

    showWeather();
  }

  else
  {
    mode = MODE_ANIMATION;

    screenStart =
      millis();

    mimoHappy();

    drawHappyEyes();
  }
}


// ========================================================
// SETUP
// ========================================================

void setup()
{
  Serial.begin(115200);

  randomSeed(
    analogRead(0)
  );

  // Touch
  pinMode(
    TOUCH_PIN,
    INPUT
  );

  // OLED
  Wire.begin(
    OLED_SDA_PIN,
    OLED_SCL_PIN
  );

  oled.begin();

  oled.clearBuffer();

  oled.setFont(
    u8g2_font_6x12_tf
  );

  oled.drawStr(
    45,
    30,
    "MIMO"
  );

  oled.drawStr(
    32,
    48,
    "Starting..."
  );

  oled.sendBuffer();

  // Audio
  setupI2S();

  mimoHello();

  // WiFi
  connectWiFi();

  // Time
  setupTime();

  // Weather
  getWeather();

  // Start animation
  mode =
    MODE_ANIMATION;

  screenStart =
    millis();

  drawHappyEyes();

  delay(1000);
}


// ========================================================
// MAIN LOOP
// ========================================================

void loop()
{
  // ------------------------------------------------------
  // TOUCH
  // ------------------------------------------------------

  if (
    digitalRead(TOUCH_PIN)
    == HIGH
  )
  {
    touchPressed();

    // Wait until finger is removed
    while (
      digitalRead(TOUCH_PIN)
      == HIGH
    )
    {
      delay(10);
    }
  }


  // ------------------------------------------------------
  // AUTOMATIC RETURN TO ANIMATION
  // ------------------------------------------------------

  if (
    mode != MODE_ANIMATION &&
    millis() -
    screenStart >
    INFO_TIMEOUT
  )
  {
    mode =
      MODE_ANIMATION;

    drawNormalEyes();
  }


  // ------------------------------------------------------
  // ANIMATION
  // ------------------------------------------------------

  if (
    mode ==
    MODE_ANIMATION
  )
  {
    animateMimo();
  }


  // ------------------------------------------------------
  // UPDATE TIME SCREEN
  // ------------------------------------------------------

  if (
    mode ==
    MODE_TIME
  )
  {
    static unsigned long lastClock =
      0;

    if (
      millis() -
      lastClock >
      1000
    )
    {
      lastClock =
        millis();

      showTime();
    }
  }


  delay(10);
}
