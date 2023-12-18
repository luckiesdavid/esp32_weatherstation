esp32_weatherstation


Sensoren:

- HTU21
- DT11
- Batteriespannung
- 
Funktionen:

- Deepsleep
- MQTT for Homeassistant

- mqtt:
  - sensor:
      name: "ESP32 Wetterstation Arduino Temperatur"
      state_topic: "esp32_wetterstation_arduino/temperature"
      unit_of_measurement: "°C"
  - sensor:
      name: "ESP32 Wetterstation Arduino Humidity"
      state_topic: "esp32_wetterstation_arduino/humidity"
      unit_of_measurement: "%"

