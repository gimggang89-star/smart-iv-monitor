#include "secrets.h"   // Wi-Fi·Firebase 설정 (secrets.example.h 복사)
#include <WiFi.h>
#include <PubSubClient.h>

// ====== WiFi / MQTT 설정 ======
const char* ssid = WIFI_SSID;          // 와이파이 이름
const char* password = WIFI_PASSWORD;     // 와이파이 비번

// 🔧 Node-RED에서 쓰는 HiveMQ 브로커랑 동일하게 맞추기
const char* mqtt_server = "broker.hivemq.com";  // MQTT 브로커 주소
const int   mqtt_port   = 1883;                 // 기본 1883

WiFiClient espClient;
PubSubClient client(espClient);

// ====== 가짜 센서 기준값 ======
const float BASE_TEMP = 30.0;   // 기준 온도
const float BASE_HUMI = 40.0;   // 기준 습도

long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // MQTT 재접속 루프
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // 🔧 clientId를 약간 랜덤하게 만들어서 중복 방지
    String clientId = "ESP32-IoT-Dashboard-";
    clientId += String(random(0xFFFF), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // 필요하면 여기서 subscribe도 가능
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// ±1~5 랜덤 생성 (0 안나오게)
int randomDelta() {
  int d = random(1, 6);          // 1~5
  if (random(0, 2) == 0) d = -d; // 부호 랜덤
  return d;
}

void setup() {
  Serial.begin(115200);
  randomSeed(micros());          // 랜덤 시드

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 1000) {  // 1초마다 전송
    lastMsg = now;

    // 기준값(30, 40)에서 ±1~5 랜덤
    float t = BASE_TEMP + randomDelta();
    float h = BASE_HUMI + randomDelta();

    // JSON 만들기
    char payload[100];
    snprintf(payload, sizeof(payload),
             "{\"temp\": %.2f, \"humi\": %.2f}", t, h);

    Serial.print("Publish message: ");
    Serial.println(payload);

    // 🔧 Node-RED mqtt in 토픽이랑 동일해야 함
    client.publish("iot/room1/sensor", payload);
  }
}
