#include <credentials.h>
#include <EspMQTTClient.h>
#include <MqttKalmanPublish.h>
#include <vector>

#ifdef I2SMATRIX
#include "matrix-i2s.h"
#elif NEOMATRIX
#include "matrix-neomatrix.h"
#else
#include "matrix-testing.h"
#endif

const bool MQTT_RETAINED = false;

EspMQTTClient client(
    WIFI_SSID,
    WIFI_PASSWORD,
    MQTT_SERVER,
    MQTT_USERNAME,
    MQTT_PASSWORD,
    CLIENT_NAME,
    1883);

#define BASE_TOPIC CLIENT_NAME "/"
#define BASE_TOPIC_SET BASE_TOPIC "set/"
#define BASE_TOPIC_STATUS BASE_TOPIC "status/"

#ifdef ESP8266
  #define LED_BUILTIN_ON LOW
  #define LED_BUILTIN_OFF HIGH
#else // for ESP32
  #define LED_BUILTIN_ON HIGH
  #define LED_BUILTIN_OFF LOW
#endif

MQTTKalmanPublish mkRssi(client, BASE_TOPIC_STATUS "rssi", MQTT_RETAINED, 12 * 5 /* every 5 min */, 10);

bool on = false;
uint8_t mqttBri = 0;

void testMatrix() {
  Serial.println("Fill screen: RED");
  matrix_fill(255, 0, 0);
  matrix_update();
  delay(250);

  Serial.println("Fill screen: GREEN");
  matrix_fill(0, 255, 0);
  matrix_update();
  delay(250);

  Serial.println("Fill screen: BLUE");
  matrix_fill(0, 0, 255);
  matrix_update();
  delay(250);
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println();

  matrix_setup(mqttBri << BRIGHTNESS_SCALE);

  client.enableDebuggingMessages();
  client.enableHTTPWebUpdater();
  client.enableOTA();
  client.enableLastWillMessage(BASE_TOPIC "connected", "0", MQTT_RETAINED);

  // well, hope we are OK, let's draw some colors first :)
  // testMatrix();

  matrix_fill(0, 0, 0);
  matrix_update();

  Serial.println("Setup done...");
}

void onConnectionEstablished()
{
  client.subscribe(BASE_TOPIC_SET "bri", [](const String &payload) {
    auto value = strtol(payload.c_str(), 0, 10);
    mqttBri = max(1l, min(255l >> BRIGHTNESS_SCALE, value));
    matrix_brightness((mqttBri << BRIGHTNESS_SCALE) * on);
    client.publish(BASE_TOPIC_STATUS "bri", String(mqttBri), MQTT_RETAINED);
  });

  client.subscribe(BASE_TOPIC_SET "on", [](const String &payload) {
    on = payload == "1" || payload == "true";
    matrix_brightness((mqttBri << BRIGHTNESS_SCALE) * on);
    client.publish(BASE_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
  });

  client.publish(BASE_TOPIC "git-remote", GIT_REMOTE, MQTT_RETAINED);
  client.publish(BASE_TOPIC "git-version", GIT_VERSION, MQTT_RETAINED);
  client.publish(BASE_TOPIC "connected", "2", MQTT_RETAINED);
}

// see https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
// hue 0.0 - 360.0
// sat 0.0 - 1.0
// bri 0.0 - 1.0
void set_hsv(uint16_t x, uint16_t y, float h, float s, float v)
{
  float r, g, b;

  if (s <= 0.0f) // < is bogus, just shuts up warnings
  {
    r = v;
    g = v;
    b = v;
  }
  else
  {
    float hh = h;
    if (hh >= 360.0f)
      hh = 0.0f;
    hh /= 60.0f;
    long i = (long)hh;
    float ff = hh - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - (s * ff));
    float t = v * (1.0f - (s * (1.0f - ff)));

    switch (i)
    {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;

    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    case 5:
    default:
      r = v;
      g = p;
      b = q;
      break;
    }
  }

  matrix_pixel(x, y, r * 255, g * 255, b * 255);
}

float hues[TOTAL_WIDTH * TOTAL_HEIGHT] = {};
float saturations[TOTAL_WIDTH * TOTAL_HEIGHT] = {};
float brightnesses[TOTAL_WIDTH * TOTAL_HEIGHT] = {};

struct Firework
{
  uint16_t center_x;
  uint16_t center_y;
  uint16_t hue;
  uint16_t distance;
};

const size_t FIREWORK_AMOUNT = 6;
struct Firework fireworks[FIREWORK_AMOUNT] = {};

uint16_t calc_point(uint16_t x, uint16_t y)
{
  return (x * TOTAL_HEIGHT) + y;
}

bool similar_firework_exists(uint16_t x, uint16_t y)
{
  for (size_t i = 0; i < FIREWORK_AMOUNT; i++)
  {
    if (x == fireworks[i].center_x || y == fireworks[i].center_y)
    {
      return true;
    }
  }

  return false;
}

void create_firework()
{
  static unsigned long nextFireworkMillis = 0;
  if (millis() >= nextFireworkMillis)
  {
    uint16_t center_x = millis() % TOTAL_WIDTH;
    uint16_t center_y = micros() % TOTAL_HEIGHT;
    uint16_t hue = millis() % 360;

    while (similar_firework_exists(center_x, center_y))
    {
      center_x += millis() % 7;
      center_x %= TOTAL_WIDTH;
      center_y += micros() % 7;
      center_y %= TOTAL_HEIGHT;
    }

    static size_t nextFireworkIndex = 0;
    if (fireworks[nextFireworkIndex].distance > max(TOTAL_WIDTH, TOTAL_HEIGHT))
    {
      fireworks[nextFireworkIndex] = Firework{
        center_x,
        center_y,
        hue,
        distance : 0
      };

      nextFireworkMillis = millis() + 50 + (micros() % 600);
      nextFireworkIndex += 1;
      nextFireworkIndex %= FIREWORK_AMOUNT;
    }
  }
}

void advance_firework(struct Firework &firework)
{
  float sat = min(1.0f, firework.distance * 0.08f);
  float bri = 1.0f - min(1.0f, firework.distance * 0.03f);

  auto small_x = firework.center_x - firework.distance;
  auto small_y = firework.center_y - firework.distance;
  auto big_x = firework.center_x + firework.distance;
  auto big_y = firework.center_y + firework.distance;

  auto index1 = calc_point(small_x, firework.center_y);
  if (small_x >= 0 && brightnesses[index1] < bri)
  {
    hues[index1] = firework.hue;
    saturations[index1] = sat;
    brightnesses[index1] = bri;
  }

  auto index2 = calc_point(firework.center_x, small_y);
  if (small_y >= 0 && brightnesses[index2] < bri)
  {
    hues[index2] = firework.hue;
    saturations[index2] = sat;
    brightnesses[index2] = bri;
  }

  auto index3 = calc_point(big_x, firework.center_y);
  if (big_x < TOTAL_WIDTH && brightnesses[index3] < bri)
  {
    hues[index3] = firework.hue;
    saturations[index3] = sat;
    brightnesses[index3] = bri;
  }

  auto index4 = calc_point(firework.center_x, big_y);
  if (big_y < TOTAL_HEIGHT && brightnesses[index4] < bri)
  {
    hues[index4] = firework.hue;
    saturations[index4] = sat;
    brightnesses[index4] = bri;
  }

  firework.distance++;
}

void animation_fireworks()
{
  create_firework();

  for (size_t i = 0; i < FIREWORK_AMOUNT; i++)
  {
    advance_firework(fireworks[i]);
  }

  for (uint16_t x = 0; x < TOTAL_WIDTH; x++)
  {
    for (uint16_t y = 0; y < TOTAL_HEIGHT; y++)
    {
      auto index = calc_point(x, y);
      set_hsv(x, y, hues[index], saturations[index], brightnesses[index]);

      if (saturations[index] < 1.0f)
      {
        saturations[index] = min(1.0f, saturations[index] + 0.04f);
      }
      if (brightnesses[index] > 0.0f)
      {
        brightnesses[index] = max(0.0f, brightnesses[index] - 0.05f);
      }
    }
  }
}

void loop()
{
  client.loop();
  digitalWrite(LED_BUILTIN, client.isConnected() ? LED_BUILTIN_OFF : LED_BUILTIN_ON);

  auto now = millis();

  static unsigned long nextMeasure = 0;
  if (now >= nextMeasure && client.isWifiConnected())
  {
    nextMeasure = now + 5000;
    auto rssi = WiFi.RSSI();
    auto avgRssi = mkRssi.addMeasurement(rssi);
    Serial.printf("RSSI          in dBm: %8d    Average: %10.2f\n", rssi, avgRssi);
  }

  animation_fireworks();

  matrix_update();
  delay(10);
}
