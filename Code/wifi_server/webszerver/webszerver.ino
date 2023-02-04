#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_now.h>

const char *SSID = "MATTS";
const char *PWD = "1234";
WebServer server(80);
StaticJsonDocument<250> jsonDocument;
char buffer[250];


//Hit detection required fields _____________________________________________________________________

int currentlyActiveSensor = -99;
int currentCombination[10] = {};
int combinationIndex = 0;


//ESP-NOW Required fields ____________________________________________________________________________

typedef struct struct_message {
  int id;
  unsigned  long milis;
  String mac;
} struct_message;
struct_message myData = {};

uint8_t clientAddress[] = {0x10, 0x97, 0xBD, 0xE2, 0xD6, 0xB8};
uint8_t masterAddress[] = {0xAC, 0x67, 0xB2, 0x53, 0x88, 0xCD};
char * masterCharAddress = "AC67B25388CD";
char* linkedDevices[10];
int numberOfLinkedDevices = 0;

//ESP-NOW fields until this point____________________________________________________________________

// ESP-NOW message Handler
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  // Copies the sender mac address to a string

  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  memcpy(&myData, incomingData, sizeof(myData));
  if (currentlyActiveSensor != myData.id) {
    Serial.printf("Wrong board has been hit");
    return;
  }
  Serial.printf("Board ID %u: %u bytes\n", myData.id, len);
  Serial.printf("Hit took: %u milisec\n", myData.milis, len);

  currentlyActiveSensor = -99;
}

//ESP-NOW message sender
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

uint8_t charToMacHelperAddress[6] = {};
uint8_t* charToMac(char* mac_addr) {
  for (int i = 0; mac_addr[i] != 0; i++) {
    char bytee[3];
    strncpy(bytee, &mac_addr[i * 2], 2); // copy 2 characters
    bytee[2] = 0;
    charToMacHelperAddress[i] = strtol(bytee, NULL, 16);
  }
  return &charToMacHelperAddress;
}

//Removing Peer --- esp_now_del_peer (macaddr)

esp_err_t addPeer(char* mac_addr) {

  uint8_t clientAddress[]  = &charToMac(mac_addr);
  /* Checking if it works
    uint8_t clientAddress[6];
    for(int i = 0; mac_addr[i]!= 0; i++){
    char bytee[3];
    strncpy(bytee, &mac_addr[i*2], 2);  // copy 2 characters
    bytee[2] = 0;
    clientAddress[i] = strtol(bytee, NULL, 16);
    }
  */


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
  server.on("/specific", HTTP_POST, specificTraining);
  server.on( "/random", HTTP_POST, randomTraining);
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
  serializeJsonPretty(jsonDocument, buffer);
}


void getDeviceList() {
  Serial.println("Get Device list has been called");
  create_json("command", 1);
  char macStr[18];
  for (int i = 0; i < numberOfLinkedDevices; i++) {
    char num_char[10 + sizeof(char)];
    std::sprintf(num_char, "%d", i);
    add_json_object(num_char, "mac_addr", linkedDevices[i]);
  }
  server.send(200, "application/json", buffer);
}

void randomTraining() {
  Serial.println("Random training has been strated");
  if (server.hasArg("plain") == false) {
    Serial.println("Missing argument list");
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  const char* obj = jsonDocument["deviceNum"];
  Serial.println(obj);
  int intValue = std::stoi(obj);  //This value holds how many combination we want to do randomly;
  
  
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
  esp_err_t result = addPeer(linkedDevices[numberOfLinkedDevices - 1]);
  server.send(200, "application/json", esp_err_to_name(result));
}

void specificTraining() {
  Serial.println("Random training mode initiated");
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  const char* obj = jsonDocument["deviceNum"];
  Serial.println(obj);
  int intValue = std::stoi(obj);
  if (intValue > 10 || intValue < 0 || linkedDevices[intValue] == null) {
    Serial.println("Incorrect input for deviceNum");
  }

  Serial.print("Added mac to message\t");
  Serial.println(myData.mac);
  myData.id = intValue;
  currentlyActiveSensor = intValue;

  uint8_t clientAddressLocal[6];
  for (int i = 0; linkedDevices[intValue][i] != 0; i++) {
    char bytee[3];
    strncpy(bytee, &linkedDevices[intValue][i * 2], 2); // copy 2 characters
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
  create_json("Result", 0);
  server.send(200, "application/json", buffer);
}



void setup() {
  Serial.begin(115200);
  Serial.println("Creating Access Point Wi-Fi");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SSID, PWD);

  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  setup_routing();
  server.begin();


  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW \n Restarting the system...");
    ESP.restart();
    return;
  }
  myData.mac = masterCharAddress;

  //Register the incoming and outgoing esp-now handlers
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  Serial.println("Setup is done");
}

void loop() {
  if(
  server.handleClient();
}
