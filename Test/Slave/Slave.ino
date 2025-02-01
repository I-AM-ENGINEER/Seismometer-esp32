#include <WiFi.h>
#include <esp_now.h>

#define MASTER_MAC {0xC8, 0xF0, 0x9E, 0xF8, 0xAF, 0x08} // Replace with your master ESP32 MAC address
#define SYNC_PIN  23

// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t masterAddress[] = MASTER_MAC;

typedef struct __attribute__((packed)) {
  uint8_t i;
} sync_data_t;

// Create a struct_message called myData
sync_data_t myData;
esp_now_peer_info_t peerInfo;

// callback function that will be executed when data is received
void IRAM_ATTR OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  //memcpy(&myData, incomingData, sizeof(myData));
  esp_err_t result = esp_now_send(masterAddress, (uint8_t *) &myData, sizeof(myData));
  //digitalWrite(SYNC_PIN, HIGH);
  //delayMicroseconds(100);
  //digitalWrite(SYNC_PIN, LOW);
  //Serial.print("timestamp");
  //Serial.println(myData.timestamp);
  //Serial.println();
}

void IRAM_ATTR OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  digitalWrite(SYNC_PIN, HIGH);
  delayMicroseconds(100);
  digitalWrite(SYNC_PIN, LOW);
}
 
void setup() {
  // Initialize Serial Monitor
  pinMode(SYNC_PIN, OUTPUT);
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}
 
void loop() {

}
