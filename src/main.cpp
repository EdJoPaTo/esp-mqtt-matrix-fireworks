#include <credentials.h>
#include <EspMQTTClient.h>
#include <MqttKalmanPublish.h>
#include <vector>

#include "matrix.h"

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

// Percentage from 0.0f to 1.0f
float bri = 0;
bool on = false;

void testMatrix()
{
	Serial.println("Fill screen: RED");
	strip.ClearTo(RgbColor(20, 0, 0));
	strip.Show();
	delay(250);

	Serial.println("Fill screen: GREEN");
	strip.ClearTo(RgbColor(0, 20, 0));
	strip.Show();
	delay(250);

	Serial.println("Fill screen: BLUE");
	strip.ClearTo(RgbColor(0, 0, 20));
	strip.Show();
	delay(250);
}

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(115200);
	Serial.println();

	strip.Begin();

	client.enableDebuggingMessages();
	client.enableHTTPWebUpdater();
	client.enableOTA();
	client.enableLastWillMessage(BASE_TOPIC "connected", "0", MQTT_RETAINED);

	// well, hope we are OK, let's draw some colors first :)
	// testMatrix();

	strip.ClearTo(0);
	strip.Show();

	Serial.println("Setup done...");
}

void onConnectionEstablished()
{
	client.subscribe(BASE_TOPIC_SET "bri", [](const String &payload) {
		auto value = strtof(payload.c_str(), 0) / 100.0f;
		if (!isfinite(value)) return;
		bri = max(1 / 255.0f, min(1.0f, value));
		client.publish(BASE_TOPIC_STATUS "bri", String(bri * 100), MQTT_RETAINED);
	});

	client.subscribe(BASE_TOPIC_SET "on", [](const String &payload) {
		on = payload == "1" || payload == "true";
		client.publish(BASE_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
	});

	client.publish(BASE_TOPIC "git-remote", GIT_REMOTE, MQTT_RETAINED);
	client.publish(BASE_TOPIC "git-version", GIT_VERSION, MQTT_RETAINED);
	client.publish(BASE_TOPIC "connected", "2", MQTT_RETAINED);
}

// All values 0.0 - 1.0
float hues[TOTAL_PIXELS] = {};
float saturations[TOTAL_PIXELS] = {};
float brightnesses[TOTAL_PIXELS] = {};

struct Firework
{
	uint16_t center_x;
	uint16_t center_y;
	float hue;
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
	if (on && millis() >= nextFireworkMillis)
	{
		uint16_t center_x = millis() % TOTAL_WIDTH;
		uint16_t center_y = micros() % TOTAL_HEIGHT;
		float hue = (millis() % 360) / 360.0f;

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

	auto index1 = topo.Map(small_x, firework.center_y);
	if (small_x >= 0 && brightnesses[index1] < bri)
	{
		hues[index1] = firework.hue;
		saturations[index1] = sat;
		brightnesses[index1] = bri;
	}

	auto index2 = topo.Map(firework.center_x, small_y);
	if (small_y >= 0 && brightnesses[index2] < bri)
	{
		hues[index2] = firework.hue;
		saturations[index2] = sat;
		brightnesses[index2] = bri;
	}

	auto index3 = topo.Map(big_x, firework.center_y);
	if (big_x < TOTAL_WIDTH && brightnesses[index3] < bri)
	{
		hues[index3] = firework.hue;
		saturations[index3] = sat;
		brightnesses[index3] = bri;
	}

	auto index4 = topo.Map(firework.center_x, big_y);
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
			auto index = topo.Map(x, y);
			auto color = HsbColor(hues[index], saturations[index], brightnesses[index] * bri);
			strip.SetPixelColor(index, color);

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

	strip.Show();
	delay(10);
}
