#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <TimeLib.h>
#include <OrviboS20.h>
#include <OrviboS20WiFiPair.h>

#include "Log.h"
#include "pins.h"
#include "server.h"
#include "RGBLed.h"

#include "settings.h" // Create from settings.template

#define LED_PAIRING_STATE Color::Blue, 1000, 1000
#define LED_PAIRING_ERROR Color::Red, 100, 200, 3
#define LED_PAIRING_SUCCESS Color::Green, 100, 200, 3

static WiFiUDP ntpUDP;
static OrviboS20Device s20;
static RGBLed led(RED_LED_PIN, GREEN_LED_PIN, BLUE_LED_PIN);

NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_CLOCK_OFFSET, 60000);

static void setupWifi()
{
  WiFi.disconnect();
  WiFi.softAPdisconnect();

  // There seems to be an issue with the ESP DHCP server and a re-connecting S20
  // If you reboot the ESP with a S20 device connected without this delay the IP assignment
  // will not work and also the wifi_softap_get_station_info() will get stuck.
  Serial.println("Long delay...");
  delay(30000);

  WiFi.mode(WIFI_AP_STA);

  WiFi.onStationModeConnected([](const WiFiEventStationModeConnected &event) {
    Serial.printf("WiFi connected, RSSI: %d dBm", WiFi.RSSI());
  });

  WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &event) {
    Serial.printf("WiFi got IP, RSSI: %d dBm", WiFi.RSSI());
  });

  // Setup WiFi station
  Serial.printf("Connecting WiFi to \"%s\"", WIFI_STA_SSID);
  WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSKEY);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);

  // Setup WiFi AP
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSKEY);
}

static void setupS20()
{
  // Set callbacks for S20 device
  s20.onConnect([](OrviboS20Device &device) {
    Log.info("S20 connected!");
  });
  s20.onDisconnect([](OrviboS20Device &device) {
    Log.info("S20 disconnected!");
  });
  s20.onStateChange([](OrviboS20Device &device, bool new_state) {
    Log.info("S20 state changed to: %d\n", new_state);
  });

  // Set OrviboS20 communication callbacks
  OrviboS20.onFoundDevice([](uint8_t mac[]) {
    Log.info("<OrviboS20> Detected new Orvibo device: %02x:%02x:%02x:%02x:%02x:%02x\n",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  });

  // Start S20 communication (this class handles the communication for all OrviboS20Device instances)
  OrviboS20.begin();

  // Set callbacks for OrviboS20WiFiPair
  OrviboS20WiFiPair.onFoundDevice([](const uint8_t mac[]) {
    // This is called when OrviboS20WiFiPair finds a device with SSID "WiWo-S20"
    // OrviboS20WiFiPair will then try to connect to the device
    Log.info("<OrviboS20WiFiPair> Found S20 device in pairing mode");
  });
  OrviboS20WiFiPair.onSendingCommand([](const uint8_t mac[], const char cmd[]) {
    // This is called for each command sent to the S20 device to be paired
    Log.info("<OrviboS20WiFiPair> Sending command: %s", cmd);
  });
  OrviboS20WiFiPair.onStopped([](OrviboStopReason reason) {
    // This is called when the pairing process is stopped
    switch (reason)
    {
    case REASON_TIMEOUT:
      Log.warn("<OrviboS20WiFiPair> Pairing timeout");
      led.stopBlink();
      break;
    case REASON_COMMAND_FAILED:
      Log.error("<OrviboS20WiFiPair> Command failed");
      led.startBlink(LED_PAIRING_ERROR);
      break;
    case REASON_STOPPED_BY_USER:
      led.stopBlink();
      break;
    case REASON_PAIRING_SUCCESSFUL:
      led.startBlink(LED_PAIRING_SUCCESS);
      break;
    }
  });
  OrviboS20WiFiPair.onSuccess([](const uint8_t mac[]) {
    // The pairing is now complete and the S20 device should now connect to our "WIWO" SSID
    Log.info("<OrviboS20WiFiPair> Pairing successful!");
  });

  pinMode(BUTTON_PIN, INPUT);
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    // Start S20 pairing process and make it connect to our AP
    led.startBlink(LED_PAIRING_STATE);
    OrviboS20WiFiPair.begin(WIFI_AP_SSID, WIFI_AP_PASSKEY);
  }
}

static void setupOta()
{
  ArduinoOTA.setHostname(HOSTNAME);
#ifdef OTA_PASSWORD
  ArduinoOTA.setPassword(OTA_PASSWORD);
#endif

  ArduinoOTA.onStart([]() {
    Log.info("OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Log.info("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int last_percent = 0;
    int percent = (progress / (total / 100));
    int diff = percent - last_percent;
    diff = diff < 0 ? -diff : diff;
    if (diff >= 5)
    {
      last_percent = percent;
      Log.info("Progress: %u%%\r", (progress / (total / 100)));
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Log.error("Error[%u]:", error);
    switch (error)
    {
    case OTA_AUTH_ERROR:
      Log.error("Auth Failed");
      break;
    case OTA_BEGIN_ERROR:
      Log.error("Begin Failed");
      break;
    case OTA_CONNECT_ERROR:
      Log.error("Connect Failed");
      break;
    case OTA_RECEIVE_ERROR:
      Log.error("Receive Failed");
      break;
    case OTA_END_ERROR:
      Log.error("End Failed");
      break;
    }
  });
  ArduinoOTA.begin();
}

static void handleNtp()
{
  static long last_time = 0;

  if (millis() - last_time < 10000)
  {
    return;
  }
  last_time = millis();

  if (year() < 2000)
  {
    timeClient.update();
    Log.info("Current time: %s", timeClient.getFormattedTime().c_str());
    setTime(timeClient.getEpochTime());
  }
}

static void handleS20()
{
  // Handle S20 communication
  OrviboS20.handle();
  // Handle WiFi pairing communication
  OrviboS20WiFiPair.handle();
}

void setup()
{
  Serial.begin(115200);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  digitalWrite(ONBOARD_LED_PIN, LOW);

  Log.begin();

  // Connect to WiFi network
  setupWifi();

  if (!MDNS.begin(HOSTNAME))
  {
    Log.error("Error setting up MDNS responder!");
  }
  MDNS.addService("http", "tcp", 80);

  Log.info("Free stack: %d", ESP.getFreeContStack());

  server_init();

  setupOta();
  setupS20();
}

void loop()
{
  MDNS.update();
  ArduinoOTA.handle();
  handleNtp();
  handleS20();
  server_handle();
  led.handle();
}
