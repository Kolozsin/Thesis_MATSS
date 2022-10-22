// This is the Slave side of the code
#define LED_1 4
#define LED_2 2
#define POT 32
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

long milis = 0;
int startDetection = 0;
uint8_t broadcastAddress[6];
int id = 0;
int32_t channel = 0;


typedef struct struct_message {
  int id;
  unsigned long milis;
  String mac;
} struct_message;
struct_message myData = {};


//This is the main responsibility of the device.
long hitDetection() {
  milis = millis();
  digitalWrite(LED_1, HIGH);
  while ((analogRead(POT) < 2000)){}
  milis = millis() - milis;
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, HIGH);
  return milis;
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


// Will need a killswitch as well in the future (for example string fn in the message or something like that)
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  
  
  esp_now_peer_info_t peerInfo = {};
  channel = 0;

  memcpy(&myData,incomingData,sizeof(myData));

  id = myData.id; 
  
  uint8_t clientAddress[6];
  for(int i = 0; myData.mac[i]!= 0; i++){
    char bytee[3];
    strncpy(bytee, &myData.mac[i*2], 2);  // copy 2 characters
    bytee[2] = 0;   
    clientAddress[i] = strtol(bytee, NULL, 16);    
  }  
  
  memcpy(&peerInfo.peer_addr, clientAddress, sizeof(clientAddress));
  memcpy(&broadcastAddress, clientAddress, sizeof(clientAddress)); 

  char macStr[18];
  Serial.print("MAC ADDED:\t");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           clientAddress[0], clientAddress[1], clientAddress[2], clientAddress[3], clientAddress[4], clientAddress[5]);
  Serial.println(macStr);
  
  peerInfo.encrypt = false;
  peerInfo.channel = channel;
  peerInfo.ifidx = WIFI_IF_AP;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");  
  }
  startDetection = 1;  
}



void setup() {
  Serial.begin(115200);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  WiFi.mode(WIFI_AP_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW \n restart has started...");
    ESP.restart();
  }
  else {
    Serial.println("SUCCESS ESP-now");
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  if (startDetection == 1) {
    //This means that we have got a command from master
    hitDetection();
    myData.milis = milis;
    myData.id = id;
    startDetection = 0;
    Serial.println(myData.id);
    Serial.println(myData.milis);
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    if (result == ESP_OK) {
      Serial.println("Sent with success"); 
      delay(50);
      digitalWrite(LED_2, LOW);     
    }
    else {
      Serial.println(esp_err_to_name(result));
      Serial.println("Error sending the data");
    }
  }
}

int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i = 0; i < n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
        return WiFi.channel(i);
      }
    }
  }
  return 0;
}
