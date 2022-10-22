#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_now.h>

const char *SSID = "MATTS";
const char *PWD = "1234";

WebServer server(80);

char* linkedDevices[10];
int numberOfLinkedDevices = 0;

StaticJsonDocument<250> jsonDocument;
char buffer[250];

//ESP-NOW Required fields
typedef struct struct_message {
  int id;
  unsigned  long milis;
  String mac;
} struct_message;
struct_message myData = {};

uint8_t clientAddress[] = {0x10, 0x97, 0xBD, 0xE2, 0xD6, 0xB8};
uint8_t masterAddress[] = {0xAC, 0x67, 0xB2, 0x53, 0x88, 0xCD};
char * masterCharAddress = "AC67B25388CD";
int currentlyActiveSensor = -99;



//ESP-NOW fields until this point

// ESP-NOW message Handler
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  // Copies the sender mac address to a string
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.printf("Board ID %u: %u bytes\n", myData.id, len);
  Serial.printf("Hit took: %u milisec\n", myData.milis, len);
  Serial.println();
}

//ESP-NOW message sender
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

esp_err_t addPeer(char* mac_addr) {
  uint8_t clientAddress[6];
  for(int i = 0; mac_addr[i]!= 0; i++){
    char bytee[3];
    strncpy(bytee, &mac_addr[i*2], 2);  // copy 2 characters
    bytee[2] = 0;   
    clientAddress[i] = strtol(bytee, NULL, 16);    
  }  
  //TESTING
  char macStr[18];
  Serial.print("MAC ADDED:\t");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           clientAddress[0], clientAddress[1], clientAddress[2], clientAddress[3], clientAddress[4], clientAddress[5]);
  Serial.println(macStr);
  //END TESTING
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, clientAddress, 6);
  peerInfo.encrypt = false;
  peerInfo.channel =  WiFi.channel();
  peerInfo.ifidx = WIFI_IF_STA;

  //Add peer
  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK) {
    Serial.print("Failed to add peer \t");
    Serial.println(result);
  }
  return result;
}


//This is the  Rest API table we use
void setup_routing() {     
  server.on("/listlinked", getDeviceList);   
  server.on("/addslave", HTTP_POST, addSlavePost);
  server.on("/random", HTTP_POST, randomTraining);              
  server.begin();    
}
 
void create_json(char *tag, float value) {  
  jsonDocument.clear();  
  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
  serializeJsonPretty(jsonDocument, buffer);
}
 
void add_json_object(char *nestedObject, char *tag, char *value) {
  JsonObject obj = jsonDocument.createNestedObject(nestedObject);
  obj["type"] = tag;
  obj["value"] = value;
  serializeJsonPretty(jsonDocument,buffer);
}

 
void getDeviceList() {
  Serial.println("Get Device list has been called");
  create_json("command",1);
  char macStr[18];
  for(int i = 0; i < numberOfLinkedDevices; i++){    
    char num_char[10 + sizeof(char)];
    std::sprintf(num_char, "%d", i);
    add_json_object(num_char,"mac_addr",linkedDevices[i]); 
  }

  server.send(200, "application/json", buffer);
}


void addSlavePost() {
  Serial.print("Add Slave has been called \t");
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  const char* obj = jsonDocument["mac_addr"];  
  Serial.println(obj);  
  linkedDevices[numberOfLinkedDevices++] = strdup(obj);
  esp_err_t result = addPeer(linkedDevices[numberOfLinkedDevices-1]);
  server.send(200, "application/json", esp_err_to_name(result));
}

void randomTraining(){
  Serial.println("Random training mode initiated");
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  const char* obj = jsonDocument["deviceNum"];
  Serial.println(obj);
  int intValue = std::stoi(obj);
  Serial.println(intValue);

  
  //memcpy(myData.mac,masterCharAddress,12);
  Serial.print("Added mac to message\t");
  Serial.println(myData.mac);
  myData.id = intValue;
  
  uint8_t clientAddressLocal[6];
  Serial.println("Memcpy was successful");
  for(int i = 0; linkedDevices[intValue][i]!= 0; i++){
    char bytee[3];
    strncpy(bytee, &linkedDevices[intValue][i*2], 2);  // copy 2 characters
    bytee[2] = 0;   
    clientAddressLocal[i] = strtol(bytee, NULL, 16);    
  }
  esp_err_t result = esp_now_send(clientAddressLocal, (uint8_t *) &myData, sizeof(myData));
    if (result == ESP_OK) {
      Serial.println("Sent with success");      
    }
    else {
      Serial.println(esp_err_to_name(result));
      Serial.println("Error sending the data");
    }
  create_json("Result",0);
  server.send(200, "application/json", buffer);
}

 

void setup() {     
  Serial.begin(115200); 
  Serial.println("Creating Access Point Wi-Fi");
  WiFi.mode(WIFI_AP_STA);  
  WiFi.softAP(SSID,PWD);
    
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  Serial.print("Wifi soft ap mac: \t");
  Serial.println(WiFi.softAPmacAddress());
  setup_routing();  
  server.begin();
  

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

   myData.mac = masterCharAddress;
   Serial.print("Added mac to message\t");
  
  //Register the incoming and outgoing esp-now handlers
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  Serial.println("Setup is done");
}    
       
void loop() {    
  server.handleClient();     
}
