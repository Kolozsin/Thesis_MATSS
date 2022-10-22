#include <esp_now.h>
#include <WiFi.h>

// Network credentials and Wifi setup
const char* ssid = "MATTS";
const char* password = "123456789";
WiFiServer server(80);
// Variable to store the HTTP request
String header;

uint8_t clientAddress[] = {0x10, 0x97, 0xBD, 0xE2, 0xD6, 0xB8};
uint8_t masterAddress[] = {0xAC, 0x67, 0xB2, 0x53, 0x88, 0xCD};

//Message Structure and stored message
typedef struct struct_message {
  int id;
  unsigned  long milis;
  uint8_t* mac[6];
} struct_message;
struct_message myData;



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

void addPeer() {
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, clientAddress, 6);
  peerInfo.encrypt = false;
  peerInfo.ifidx=WIFI_IF_AP;

  //Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  //WIFI setup
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  //This will be moved to an http request handler
  addPeer();


  //Register the incoming and outgoing esp-now handlers
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
}

void loop() {
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            //Handler of HTML
            if (header.indexOf("GET /addDevice") >= 0) {
              Serial.println("Send to esp-now hit detection");
              memcpy(&myData.mac,masterAddress,sizeof(masterAddress));
              esp_err_t result = esp_now_send(clientAddress, (uint8_t *) &myData, sizeof(myData));
              if (result == ESP_OK) {
                Serial.println("Sent with success");
              }
              else {
                Serial.println("Error sending the data");
              }
            }

            /*
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");

            // Display current state, and ON/OFF buttons for GPIO 26
            client.println("<p>GPIO 26 - State </p>");
            // If the output26State is off, it displays the ON button

            client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
            client.println("</body></html>");

            */

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
