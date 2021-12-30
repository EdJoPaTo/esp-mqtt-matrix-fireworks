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

#define CLIENT_NAME "espFireworks"
const bool MQTT_RETAINED = false;

EspMQTTClient client(
    WIFI_SSID,
    WIFI_PASSWORD,
    MQTT_SERVER,
    MQTT_USERNAME,
    MQTT_PASSWORD,
    CLIENT_NAME,
    1883 // MQTT Broker Port. Can be omitted
);

#define BASIC_TOPIC CLIENT_NAME "/"
#define BASIC_TOPIC_SET BASIC_TOPIC "set/"
#define BASIC_TOPIC_STATUS BASIC_TOPIC "status/"

#ifdef ESP8266
  #define LED_BUILTIN_ON LOW
  #define LED_BUILTIN_OFF HIGH
#else // for ESP32
  #define LED_BUILTIN_ON HIGH
  #define LED_BUILTIN_OFF LOW
#endif

MQTTKalmanPublish mkCommandsPerSecond(client, BASIC_TOPIC_STATUS "commands-per-second", false, 12 * 1 /* every 1 min */, 10);
MQTTKalmanPublish mkKilobytesPerSecond(client, BASIC_TOPIC_STATUS "kilobytes-per-second", false, 12 * 1 /* every 1 min */, 10);
MQTTKalmanPublish mkRssi(client, BASIC_TOPIC_STATUS "rssi", MQTT_RETAINED, 12 * 5 /* every 5 min */, 10);

boolean on = false;
uint8_t mqttBri = 0;

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println();

  matrix_setup(mqttBri << BRIGHTNESS_SCALE);

  client.enableDebuggingMessages();
  client.enableHTTPWebUpdater();
  client.enableOTA();
  client.enableLastWillMessage(BASIC_TOPIC "connected", "0", MQTT_RETAINED);

  // // well, hope we are OK, let's draw some colors first :)
  // Serial.println("Fill screen: RED");
  // matrix_fill(255, 0, 0);
  // matrix_update();
  // delay(250);

  // Serial.println("Fill screen: GREEN");
  // matrix_fill(0, 255, 0);
  // matrix_update();
  // delay(250);

  // Serial.println("Fill screen: BLUE");
  // matrix_fill(0, 0, 255);
  // matrix_update();
  // delay(250);

  // Serial.println("Fill screen: BLACK");
  matrix_fill(0, 0, 0);
  matrix_update();
  // delay(250);

  Serial.println("Setup done...");
}

void onConnectionEstablished()
{
  client.subscribe(BASIC_TOPIC_SET "bri", [](const String &payload) {
    int value = strtol(payload.c_str(), 0, 10);
    mqttBri = max(1, min(255 >> BRIGHTNESS_SCALE, value));
    matrix_brightness((mqttBri << BRIGHTNESS_SCALE) * on);
    client.publish(BASIC_TOPIC_STATUS "bri", String(mqttBri), MQTT_RETAINED);
  });

  client.subscribe(BASIC_TOPIC_SET "on", [](const String &payload) {
    boolean value = payload != "0";
    on = value;
    matrix_brightness((mqttBri << BRIGHTNESS_SCALE) * on);
    client.publish(BASIC_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
  });

  client.publish(BASIC_TOPIC "connected", "2", MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "bri", String(mqttBri), MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
}

// see https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
void set_hsv(uint16_t x, uint16_t y, uint16_t hue, uint8_t sat, uint8_t bri)
{
  double h = hue;
  double s = sat / 255.0;
  double v = bri / 255.0;

  double hh, p, q, t, ff;
  long i;
  double r, g, b;

  if (s <= 0.0) // < is bogus, just shuts up warnings
  {
    r = v;
    g = v;
    b = v;
  }
  else
  {
    hh = h;
    if (hh >= 360.0)
      hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = v * (1.0 - s);
    q = v * (1.0 - (s * ff));
    t = v * (1.0 - (s * (1.0 - ff)));

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

uint8_t hues[TOTAL_WIDTH * TOTAL_HEIGHT] = {};
uint8_t saturations[TOTAL_WIDTH * TOTAL_HEIGHT] = {};
uint8_t brightnesses[TOTAL_WIDTH * TOTAL_HEIGHT] = {};

struct Firework
{
  uint16_t center_x;
  uint16_t center_y;
  uint16_t hue;
  uint16_t distance;
};

const size_t FIREWORK_AMOUNT = 10;
Firework fireworks[FIREWORK_AMOUNT] = {};
unsigned long nextMeasure = 0;
unsigned long nextFireworkMillis = 0;
size_t nextFireworkIndex = 0;

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
  uint8_t sat = min(255, firework.distance * 20);
  uint8_t bri = 255 - min(255, firework.distance * 8);

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

      if (saturations[index] < 255)
      {
        saturations[index] = min(255, saturations[index] + 10);
      }
      if (brightnesses[index] > 0)
      {
        brightnesses[index] = max(0, brightnesses[index] - 12);
      }
    }
  }
}

void loop()
{
  client.loop();
  digitalWrite(LED_BUILTIN, client.isConnected() ? LED_BUILTIN_OFF : LED_BUILTIN_ON);

  auto now = millis();

  if (client.isConnected())
  {
    if (now >= nextMeasure)
    {
      nextMeasure = now + 5000;
      long rssi = WiFi.RSSI();
      float avgRssi = mkRssi.addMeasurement(rssi);
      Serial.print("RSSI        in dBm:     ");
      Serial.print(String(rssi).c_str());
      Serial.print("   Average: ");
      Serial.println(String(avgRssi).c_str());
    }
  }

  animation_fireworks();

  matrix_update();
  delay(10);
}
