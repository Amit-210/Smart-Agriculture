#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <DHT.h>

#define ESPNOW_CHANNEL 1

#define DHTPIN 15
#define DHTTYPE DHT11
#define UV_PIN 34

#define I2C_SDA 21
#define I2C_SCL 22

#define LM75A_ADDRESS 0x48

// Board B / Relay ESP MAC
uint8_t peerAddress[] = {0x68, 0xFE, 0x71, 0x8A, 0x79, 0x18};

DHT dht(DHTPIN, DHTTYPE);

typedef struct message {
  int msgType;   // 1 = sensor data, 2 = relay status

  float humidity;
  float temperature;
  int uvValue;

  bool fanState;
  bool humidifierState;
  bool autoMode;
} message;

message sendData;
message receiveData;

unsigned long lastSendTime = 0;

float readLM75A() {
  Wire.beginTransmission(LM75A_ADDRESS);
  Wire.write(0x00); // Temperature register

  if (Wire.endTransmission(false) != 0) {
    return NAN;
  }

  Wire.requestFrom(LM75A_ADDRESS, 2);

  if (Wire.available() < 2) {
    return NAN;
  }

  byte msb = Wire.read();
  byte lsb = Wire.read();

  int16_t raw = ((int16_t)msb << 8) | lsb;

  // LM75A 11-bit temperature data
  raw = raw >> 5;

  // Negative temperature sign extension
  if (raw & 0x0400) {
    raw |= 0xF800;
  }

  float temperature = raw * 0.125;
  return temperature;
}

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("A to B Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

void onDataReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  memcpy(&receiveData, data, sizeof(receiveData));

  if (receiveData.msgType == 2) {
    Serial.println("----- B Theke Status Received -----");

    Serial.print("Mode: ");
    Serial.println(receiveData.autoMode ? "AUTO" : "MANUAL");

    Serial.print("Fan: ");
    Serial.println(receiveData.fanState ? "ON" : "OFF");

    Serial.print("Humidifier: ");
    Serial.println(receiveData.humidifierState ? "ON" : "OFF");

    Serial.println("-----------------------------------");
  }
}

void setup() {
  Serial.begin(9600);

  dht.begin();

  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(UV_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceive);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Peer Add Failed");
    return;
  }

  Serial.println("Board A Sensor ESP Ready with LM75A");
  Serial.print("My MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  if (millis() - lastSendTime >= 2000) {
    lastSendTime = millis();

    float humidity = dht.readHumidity();
    float temperature = readLM75A();
    int uvValue = analogRead(UV_PIN);

    if (isnan(humidity)) humidity = -1;
    if (isnan(temperature)) temperature = -1;

    sendData.msgType = 1;
    sendData.humidity = humidity;
    sendData.temperature = temperature;
    sendData.uvValue = uvValue;
    sendData.fanState = false;
    sendData.humidifierState = false;
    sendData.autoMode = true;

    Serial.println("----- A Sending Sensor Data -----");

    Serial.print("UV Value: ");
    Serial.println(uvValue);

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("LM75A Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.println("-------------------------------");

    esp_now_send(peerAddress, (uint8_t *)&sendData, sizeof(sendData));
  }
}