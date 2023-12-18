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


//------------HTU21 Sensor-----------

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

//------------WIFI & MQTT-----------

  setup_wifi();
  delay(1000); // Dauer der Onlineverbindung nach WIFI in ms
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
 // mqtt_convert();
  Serial.print("Setup 50% finished\n");

// MQTT aktivieren/deaktiverien
  if (!client.connected()) {
    reconnect();
  }
  mqtt_easy_send();
  delay(TIME_AWAKE * MS_TO_S_FACTOR); // Dauer der Onlineverbindung nach MQTT in ms


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


//------------?????-----------

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "esp32_wetterstation_arduino/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
    }
    else if(messageTemp == "off"){
      Serial.println("off");
    }
  }
}


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


//------------MQTT Konvertieren for Nodered-----------

void mqtt_convert() {
  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    Adafruit_HTU21DF htu = Adafruit_HTU21DF();
    float temperature = 0;
    float humidity = 0;
    
    // Temperature in Celsius
    temperature = htu.readTemperature();
    // Uncomment the next line to set temperature in Fahrenheit 
    // (and comment the previous temperature line)
    //temperature = 1.8 * bme.readTemperature() + 32; // Temperature in Fahrenheit
    
    // Convert the value to a char array
    char tempString[8];
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    client.publish("esp32_wetterstation_arduino/temperature", tempString);
    Serial.print(".....published temp");

    humidity = htu.readHumidity();
    
    // Convert the value to a char array
    char humString[8];
    dtostrf(humidity, 1, 2, humString);
    Serial.print("Humidity: ");
    Serial.println(humString);
    client.publish("esp32_wetterstation_arduino/humidity", humString);
    Serial.print(".....published humidity");

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

  
  String temp = String(temp_float);
  client.publish("esp32_wetterstation_arduino/temperature", temp.c_str());
  String hum = String(hum_float);
  client.publish("esp32_wetterstation_arduino/humidity", hum.c_str());


  // Temperature in Celsius
//   // Uncomment the next line to set temperature in Fahrenheit 
//   // (and comment the previous temperature line)
//   //temperature = 1.8 * bme.readTemperature() + 32; // Temperature in Fahrenheit
  
//   // Convert the value to a char array
//  char tempString[8];                                         //verursacht PANIC Erorr
// //  dtostrf(temperature, 1, 2, tempString);
//   Serial.print("Temperature: ");
//   Serial.println(tempString);
//  client.publish("esp32_wetterstation_arduino/temperature", temp);
//   Serial.print(".....published temp");

//   humidity = htu.readHumidity();
  
//   // Convert the value to a char array
//   char humString[8];
// //  dtostrf(humidity, 1, 2, humString);
//   Serial.print("Humidity: ");
//   Serial.println(humString);
//  client.publish("esp32_wetterstation_arduino/humidity", humString);
//   Serial.print(".....published humidity");
}
