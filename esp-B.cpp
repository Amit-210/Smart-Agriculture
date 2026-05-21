#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>

#define ESPNOW_CHANNEL 1

#define FAN_RELAY 23
#define HUMIDIFIER_RELAY 22

// Most relay module LOW trigger hoy
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// Board A / Sensor ESP MAC
uint8_t peerAddress[] = {0x00, 0x70, 0x07, 0x25, 0xBE, 0xEC};

const char* ssid = "Mushroom_System";
const char* password = "12345678";

WebServer server(80);

typedef struct message {
  int msgType;   // 1 = sensor data, 2 = relay status

  float humidity;
  float temperature;
  int uvValue;

  bool fanState;
  bool humidifierState;
  bool autoMode;
} message;

message receiveData;
message sendData;

float humidity = -1;
float temperature = -1;
int uvValue = 0;

bool fanState = false;
bool humidifierState = false;
bool autoMode = true;

unsigned long lastStatusSend = 0;

// UV threshold. UV sensor value dekhe adjust korte parba.
int uvHighLimit = 3500;

void applyRelay() {
  digitalWrite(FAN_RELAY, fanState ? RELAY_ON : RELAY_OFF);
  digitalWrite(HUMIDIFIER_RELAY, humidifierState ? RELAY_ON : RELAY_OFF);
}

void autoControl() {
  if (temperature < 0 || humidity < 0) {
    fanState = false;
    humidifierState = false;
    applyRelay();
    return;
  }

  // Temp > 35 C hole Fan ON, Humidifier OFF
  if (temperature > 35) {
    fanState = true;
    humidifierState = false;
  }

  // Temp < 20 C hole Humidifier ON, Fan OFF
  else if (temperature < 20) {
    fanState = false;
    humidifierState = true;
  }

  // Temp 20-35 C hole humidity er upor depend korbe
  else {
    if (humidity < 80) {
      fanState = false;
      humidifierState = true;
    }

    else if (humidity >= 80 && humidity <= 90) {
      fanState = false;
      humidifierState = false;
    }

    else if (humidity > 90) {
      fanState = false;
      humidifierState = false;
    }
  }

  // UV/Light beshi hole fan ON
  if (uvValue > uvHighLimit) {
    fanState = true;

    // Humidifier optional
    if (humidity < 80 && temperature <= 35) {
      humidifierState = true;
    } else {
      humidifierState = false;
    }
  }

  applyRelay();
}

void sendRelayStatus() {
  sendData.msgType = 2;
  sendData.humidity = humidity;
  sendData.temperature = temperature;
  sendData.uvValue = uvValue;
  sendData.fanState = fanState;
  sendData.humidifierState = humidifierState;
  sendData.autoMode = autoMode;

  esp_now_send(peerAddress, (uint8_t *)&sendData, sizeof(sendData));
}

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("B to A Status Send: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

void onDataReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  memcpy(&receiveData, data, sizeof(receiveData));

  if (receiveData.msgType == 1) {
    humidity = receiveData.humidity;
    temperature = receiveData.temperature;
    uvValue = receiveData.uvValue;

    Serial.println("----- A Theke Sensor Data Received -----");

    Serial.print("UV Value: ");
    Serial.println(uvValue);

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    if (autoMode) {
      autoControl();
    }

    Serial.print("Mode: ");
    Serial.println(autoMode ? "AUTO" : "MANUAL");

    Serial.print("Fan: ");
    Serial.println(fanState ? "ON" : "OFF");

    Serial.print("Humidifier: ");
    Serial.println(humidifierState ? "ON" : "OFF");

    Serial.println("---------------------------------------");

    sendRelayStatus();
  }
}

String htmlPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Smart Mushroom Farming</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <style>
    body {
      font-family: Arial;
      background: #eef5ef;
      margin: 0;
      padding: 20px;
      text-align: center;
    }

    h2 {
      color: #176b3a;
    }

    .card {
      background: white;
      padding: 18px;
      margin: 12px auto;
      border-radius: 12px;
      max-width: 430px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.15);
    }

    .value {
      font-size: 24px;
      font-weight: bold;
      color: #222;
    }

    button {
      padding: 12px 18px;
      margin: 6px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      color: white;
      cursor: pointer;
    }

    .auto { background: #1f8f4d; }
    .manual { background: #555; }
    .on { background: #0b79d0; }
    .off { background: #d93636; }

    .logic {
      font-size: 13px;
      text-align: left;
      line-height: 1.6;
    }
  </style>
</head>

<body>

<h2>Smart Mushroom Farming Monitoring System</h2>

<div class="card">
  <p>Mode</p>
  <div class="value" id="mode">Loading...</div>
  <button class="auto" onclick="setMode(1)">AUTO</button>
  <button class="manual" onclick="setMode(0)">MANUAL</button>
</div>

<div class="card">
  <p>Temperature</p>
  <div class="value" id="temp">-- C</div>
</div>

<div class="card">
  <p>Humidity</p>
  <div class="value" id="humidity">-- %</div>
</div>

<div class="card">
  <p>UV / Light Value</p>
  <div class="value" id="uv">--</div>
</div>

<div class="card">
  <p>Fan</p>
  <div class="value" id="fan">--</div>
  <button class="on" onclick="setFan(1)">Fan ON</button>
  <button class="off" onclick="setFan(0)">Fan OFF</button>
</div>

<div class="card">
  <p>Humidifier</p>
  <div class="value" id="humidifier">--</div>
  <button class="on" onclick="setHumidifier(1)">Humidifier ON</button>
  <button class="off" onclick="setHumidifier(0)">Humidifier OFF</button>
</div>

<div class="card logic">
  <b>Auto Logic:</b><br>
  Temp &lt; 20 C: Humidifier ON, Fan OFF<br>
  Temp 20-35 C + Humidity 80-90%: Both OFF<br>
  Temp &gt; 35 C: Humidifier OFF, Fan ON<br>
  Humidity &lt; 80%: Humidifier ON<br>
  Humidity &gt; 90%: Humidifier OFF<br>
  UV/Light High: Fan ON
</div>

<script>
function updateData() {
  fetch('/data')
    .then(response => response.json())
    .then(data => {
      document.getElementById('mode').innerHTML = data.autoMode ? "AUTO" : "MANUAL";
      document.getElementById('temp').innerHTML = data.temperature + " C";
      document.getElementById('humidity').innerHTML = data.humidity + " %";
      document.getElementById('uv').innerHTML = data.uv;
      document.getElementById('fan').innerHTML = data.fan ? "ON" : "OFF";
      document.getElementById('humidifier').innerHTML = data.humidifier ? "ON" : "OFF";
    });
}

function setMode(value) {
  fetch('/mode?auto=' + value).then(() => updateData());
}

function setFan(value) {
  fetch('/fan?state=' + value).then(() => updateData());
}

function setHumidifier(value) {
  fetch('/humidifier?state=' + value).then(() => updateData());
}

setInterval(updateData, 1000);
updateData();
</script>

</body>
</html>
)rawliteral";

  return page;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleData() {
  String json = "{";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"uv\":" + String(uvValue) + ",";
  json += "\"fan\":" + String(fanState ? "true" : "false") + ",";
  json += "\"humidifier\":" + String(humidifierState ? "true" : "false") + ",";
  json += "\"autoMode\":" + String(autoMode ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

void handleMode() {
  if (server.hasArg("auto")) {
    autoMode = server.arg("auto").toInt();

    if (autoMode) {
      autoControl();
    }

    sendRelayStatus();
  }

  server.send(200, "text/plain", "OK");
}

void handleFan() {
  if (!autoMode && server.hasArg("state")) {
    fanState = server.arg("state").toInt();
    applyRelay();
    sendRelayStatus();
  }

  server.send(200, "text/plain", "OK");
}

void handleHumidifier() {
  if (!autoMode && server.hasArg("state")) {
    humidifierState = server.arg("state").toInt();
    applyRelay();
    sendRelayStatus();
  }

  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(9600);

  pinMode(FAN_RELAY, OUTPUT);
  pinMode(HUMIDIFIER_RELAY, OUTPUT);

  digitalWrite(FAN_RELAY, RELAY_OFF);
  digitalWrite(HUMIDIFIER_RELAY, RELAY_OFF);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password, ESPNOW_CHANNEL);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.println("WiFi Access Point Started");
  Serial.print("WiFi Name: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("Web Page IP: ");
  Serial.println(WiFi.softAPIP());

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

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/mode", handleMode);
  server.on("/fan", handleFan);
  server.on("/humidifier", handleHumidifier);

  server.begin();

  Serial.println("Board B Relay + Web ESP Ready");
  Serial.print("My MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  server.handleClient();

  if (millis() - lastStatusSend >= 5000) {
    lastStatusSend = millis();
    sendRelayStatus();
  }
}