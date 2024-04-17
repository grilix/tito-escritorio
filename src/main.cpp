#include <Arduino.h>

#define TITO_USE_ADAFRUIT_BME280

#include <tito/Timer.h>
#include <tito/Sensors.h>
#include <tito/TopicNode.h>
#include <tito/GPIO.h>

#include <tito/BME.h>

#include <tito/ReportableGroup.h>
#include <tito/CommandableGroup.h>
#include <tito/Network.h>

#include <tito/ESPSensors.h>

#include "network.h"
#define FAST_TIMER_TICK 500
#define SLOW_TIMER_TICK 20'000
#define MQTT_NAMESPACE "home/office-esp"

#define STATUS_LED 2
#define WORKBENCH_LED D5
#define DESKTOP_LED D6

ADC_MODE(ADC_VCC);

struct CommandableBooleanSensor {
  tito::BitWriter writer;
  tito::BooleanReporter reporter;
  tito::BooleanCommander commander;

  CommandableBooleanSensor(
    const tito::TopicNode* parent,
    const char* name,
    tito::BitWriter _writer
  ) :
    writer(_writer),
    reporter(&writer, parent, name),
    commander(&writer, reporter.getTopic())
  { }
};

struct App {
  App(
    const char* name,
    const tito::NetworkCredentials& netCredentials,
    const tito::MQTTCredentials& mqttCredentials
  ) :
    fastTimer(FAST_TIMER_TICK),
    slowTimer(SLOW_TIMER_TICK),
    beatTimer(500),
    fastBeatTimer(200),

    topic(name),

    bme(0x76, &topic, "bme"),
    bmeStatus(bme.getStateSensor()),
    bmeStatusReporter(&bmeStatus, bme.getTopic(), "chip"),

    temperatureSensor(bme.getTemperatureSensor()),
    humiditySensor(bme.getHumiditySensor()),
    pressureSensor(bme.getPressureSensor()),

    temperatureReporter(&temperatureSensor, bme.getTopic(), "temperature"),
    humidityReporter(&humiditySensor, bme.getTopic(), "humidity"),
    pressureReporter(&pressureSensor, bme.getTopic(), "pressure"),

    vccReporter(&vccSensor, &topic, "vcc"),
    lightsTopic(&topic, "lights"),

    statusLed(gpio.getBitWriter(STATUS_LED)),

    workbench(&lightsTopic, "workbench", gpio.getBitWriter(WORKBENCH_LED, true)),
    desktop(&lightsTopic, "desktop", gpio.getBitWriter(DESKTOP_LED, true)),

    commanders({
      &workbench.commander,
      &desktop.commander
    }),

    network(&topic, netCredentials, mqttCredentials, &commanders),

    reporters(&network, {
      tito::SensorNode(&bmeStatusReporter, &slowTimer),
      tito::SensorNode(&temperatureReporter, &slowTimer),
      tito::SensorNode(&humidityReporter, &slowTimer),
      tito::SensorNode(&pressureReporter, &slowTimer),

      tito::SensorNode(&vccReporter, &slowTimer),

      tito::SensorNode(&desktop.reporter, &fastTimer),
      tito::SensorNode(&workbench.reporter, &fastTimer),
    })
  { }

  tito::Timer fastTimer;
  tito::Timer slowTimer;
  tito::Timer beatTimer;
  tito::Timer fastBeatTimer;
  tito::GPIO<15> gpio;

  tito::TopicNode topic;

  tito::BME<Adafruit_BME280> bme;
  tito::MemoryValueSource<bool> bmeStatus;
  tito::BooleanReporter bmeStatusReporter;

  tito::MemoryValueSource<float> temperatureSensor;
  tito::MemoryValueSource<float> humiditySensor;
  tito::MemoryValueSource<float> pressureSensor;

  tito::FloatReporter temperatureReporter;
  tito::FloatReporter humidityReporter;
  tito::FloatReporter pressureReporter;

  tito::ESPVCCSensor vccSensor;
  tito::FloatReporter vccReporter;

  tito::TopicNode lightsTopic;
  tito::BitWriter statusLed;

  CommandableBooleanSensor workbench;
  CommandableBooleanSensor desktop;

  tito::CommandableGroup<2> commanders;

  tito::Network network;

  tito::ReportableGroup<7> reporters;

  bool initialized = false;

  void setup()
  {
    gpio.setup();

    bme.setup();

    network.connect();
    Serial.println("app setup done.");
  }
};

tito::NetworkCredentials netCredentials = { WIFI_SSID, WIFI_PASSWORD };
tito::MQTTCredentials mqttCredentials = { MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWORD };

static App app = App(MQTT_NAMESPACE, netCredentials, mqttCredentials);
unsigned long currentMillis = 0;

void setup()
{
  Serial.begin(115200);
  delay(300);

  app.setup();

  app.desktop.writer.setValue(false);
  app.workbench.writer.setValue(false);

  app.statusLed.setValue(true);
  Serial.println("setup done.");
}

void reportUptime(const unsigned long currentMillis)
{
  char valueBuffer[10];
  size_t result = snprintf(valueBuffer, 10, "%lu", (currentMillis / 1000));
  if (result >= 10) {
    result = (10 - 1);
  }

  tito::TopicNode topic = tito::TopicNode(&app.topic, "uptime");
  app.network.onValue(&topic, valueBuffer, result);
}

void slowTick(const unsigned long currentMillis)
{
  reportUptime(currentMillis);

  app.bme.refreshValue();
}

void disconnectedLoop(const unsigned long currentMillis)
{
  if (app.beatTimer.isTick(currentMillis)) {
    app.statusLed.toggleValue();
  }
}

void loop()
{
  currentMillis = millis();

  app.fastTimer.loop(currentMillis);
  app.slowTimer.loop(currentMillis);

  if (!app.network.isConnected()) {
    disconnectedLoop(currentMillis);
    return;
  }

  if (!app.bme.isStarted()) {
    if (app.fastBeatTimer.isTick(currentMillis)) {
      app.statusLed.toggleValue();
    }
    if (app.slowTimer.isTicking()) {
      app.bme.setup();
    }
  } else if (*app.statusLed.getValue()) {
    app.statusLed.setValue(false);
  }

  if (app.slowTimer.isTicking()) {
    yield();
    slowTick(currentMillis);
  }

  app.reporters.reportValues();
}
