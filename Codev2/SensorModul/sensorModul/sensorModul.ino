#define LED_1 4
#define LED_2 2
#define POT 32
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

enum States {
  OFF,
  ON,
  PAIRING,
  PAIRED,
  ACTIVATING,
  ACTIVE,
  GOTHIT,
  SENDING
};

long millisec = 0.0;
States state = States::OFF;

void IRAM_ATTR hitDetected() {
  long currentTime= millis();
  if(currentTime - millisec < 500)
    return;
  if(state == States::ACTIVE){
    state = States::GOTHIT;
    millisec = currentTime;
  }
  else{
    state = States::ACTIVE;
     millisec = currentTime;
  }
}






void setup() {
  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(POT), hitDetected, CHANGE);
  millisec = millis();
}

void loop() {
  Serial.print("READ: ");
  Serial.println(state);
}
