#include <Wire.h>
#include "Adafruit_HTU21DF.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <iostream>
#include <string>

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define MS_TO_S_FACTOR 1000     /*Conversion factor for ms to seconds */
#define TIME_TO_SLEEP  10        /* Sleep Time in s  */
#define TIME_AWAKE 10           /* Wake Time in s */

// TODO:
// integration Batteriespannung

// https://randomnerdtutorials.com/esp32-mqtt-publish-subscribe-arduino-ide/
// https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/

//------------WIFI & MQTT-----------
const char *ssid = "XXXX";
const char *password = "XXXX";
const char *mqtt_server = "XXXX";
const char *mqtt_username = "XXXX";
const char *mqtt_password = "XXXX";


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
RTC_DATA_ATTR int bootCount = 0;

// Batteriespannung
const int pin = 34;
int analog = 0;
float bat_mV = 0;
float bat_mV_raw = 0;
float offset = 0.11;
int numReadings = 8;
float sum = 0;

// DS18B20 Temp
const int oneWireBus = 4;  // Pin 4
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);   


//------------Allgemein setup-----------

void setup(){
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");


//------------intern HTU21 Sensor-----------

  htu21_sensor();

//------------intern BAT Sensor-----------

  voltage_bat();
  delay(1500); // Batterieauslesung

//------------DS18B20 Temp-----------

  temp_boden();

//------------WIFI & MQTT-----------

  setup_wifi();
  delay(1000); // Dauer der Onlineverbindung nach WIFI in ms
  client.setServer(mqtt_server, 1883);
//  client.setCallback(callback);
  Serial.print("Setup 50% finished\n");

// MQTT aktivieren/deaktiverien
  if (!client.connected()) {
    reconnect();
  }
  mqtt_easy_send();
  delay(TIME_AWAKE * MS_TO_S_FACTOR); // Dauer der Onlineverbindung nach MQTT


//------------Deepsleep-----------
  Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void loop(){
  //This is not going to be called
}


//------------Deepsleep-----------

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}


//------------WIFI-----------

void setup_wifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


//------------Callback inaktiv-----------

// void callback(char* topic, byte* message, unsigned int length) {
//   Serial.print("Message arrived on topic: ");
//   Serial.print(topic);
//   Serial.print(". Message: ");
//   String messageTemp;
  
//   for (int i = 0; i < length; i++) {
//     Serial.print((char)message[i]);
//     messageTemp += (char)message[i];
//   }
//   Serial.println();

//   // Feel free to add more if statements to control more GPIOs with MQTT

//   // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
//   // Changes the output state according to the message
//   if (String(topic) == "esp32_wetterstation_arduino/output") {
//     Serial.print("Changing output to ");
//     if(messageTemp == "on"){
//       Serial.println("on");
//     }
//     else if(messageTemp == "off"){
//       Serial.println("off");
//     }
//   }
// }


//------------MQTT reconnect-----------

void reconnect() {
  Serial.print("attempting to send mqtt\n");
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) { //USERNAME PASSWORT korrekt
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 secondds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


//-------------MQTT easy send---------------

void mqtt_easy_send() {

  Adafruit_HTU21DF htu = Adafruit_HTU21DF();
  if (!htu.begin()) {
    Serial.println("Check circuit. HTU21D not found!");
  while (1);
    }
    float temp_float = htu.readTemperature();
    float hum_float = htu.readHumidity();
  
  // DS18B20 Temp
  sensors.requestTemperatures(); 
  float boden_temp = sensors.getTempCByIndex(0);

  calc_volt();

  String temp = String(temp_float);
  client.publish("esp32_wetterstation_arduino/temperature", temp.c_str());
  String hum = String(hum_float);
  client.publish("esp32_wetterstation_arduino/humidity", hum.c_str());
  String bat = String(bat_mV);
  client.publish("esp32_wetterstation_arduino/battery", bat.c_str());
  String bat_raw = String(bat_mV_raw);
  client.publish("esp32_wetterstation_arduino/battery_raw", bat_raw.c_str());
  String boden = String(boden_temp);
  client.publish("esp32_wetterstation_arduino/temperature_boden", boden.c_str());
}


//-------------HTU21 Sensor---------------

void htu21_sensor() {
  Adafruit_HTU21DF htu = Adafruit_HTU21DF();
  if (!htu.begin()) {
  Serial.println("Check circuit. HTU21D not found!");
  while (1);
  }
  float temp = htu.readTemperature();
  float hum = htu.readHumidity();
  Serial.print("Temperature(°C): "); 
  Serial.print(temp); 
  Serial.print("\t\t");
  Serial.print("Humidity(%): "); 
  Serial.println(hum);
}


//-------------Battery---------------

int calc_volt() {
  // ohne Mittelwertbildung
  analog = analogRead(pin);
  bat_mV_raw = analog * 0.00489 + offset;
  //Mittelwertbildung
  int total = 0;
  for (int i = 0; i < numReadings; i++) {
    total += analogRead(pin);
    delay(100);
  }
  sum = total * 0.00489; // Faktor laut Datenblatt
  bat_mV = sum / numReadings + offset;
  return bat_mV, bat_mV_raw;
}

void voltage_bat() {
  // ausgelagert in calc_volt
  calc_volt();

  Serial.print("Battery(V): ");
  Serial.println(bat_mV);
}

//-------------DS18B20 Temp---------------

void temp_boden() {
  sensors.requestTemperatures(); 
  float boden_temp = sensors.getTempCByIndex(0);
  Serial.print("Temperature Boden(°C): "); 
  Serial.print(boden_temp);
}
