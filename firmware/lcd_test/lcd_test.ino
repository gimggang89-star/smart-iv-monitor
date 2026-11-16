#include "secrets.h"   // Wi-Fi·Firebase 설정 (secrets.example.h 복사)
#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <HX711.h>
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

LiquidCrystal_PCF8574 lcd(0x27);

#define Rstart_PIN 18
#define Start_PIN 5

#define DT_PIN 12          // HX711의 DT 핀 (ESP32의 GPIO 12)
#define SCK_PIN 13         // HX711의 SCK 핀 (ESP32의 GPIO 13)
HX711 scale;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// API_KEY: secrets.h
// DATABASE_URL: secrets.h
// USER_EMAIL: secrets.h
// USER_PASSWORD: secrets.h

FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;
FirebaseData firebaseData;

float speed = 0;
float savespeed = 0.11;
float remainingTime = 0;
int displayTime = 0;
float startWeight = 0;  // 전역 변수로 선언하여 값을 유지

unsigned long lastWeightCheck = 0;
unsigned long lastLCDUpdate = 0;
const unsigned long weightInterval = 10000; // 10초마다 무게 측정
const unsigned long lcdInterval = 1000;     // 1초마다 LCD 업데이트

void setup() {
  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  pinMode(Start_PIN, INPUT_PULLUP);
  pinMode(Rstart_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(3000);
  Serial.println("시작");

  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(309);
  scale.tare();

  Serial.print("와이파이 연결중: ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n와이파이 연결 성공!");

  firebaseConfig.api_key = API_KEY;
  firebaseConfig.database_url = DATABASE_URL;
  firebaseAuth.user.email = USER_EMAIL;
  firebaseAuth.user.password = USER_PASSWORD;

  Firebase.begin(&firebaseConfig, &firebaseAuth);

  if (!Firebase.ready()) {
    Serial.println("Firebase 초기화 실패!");
  } else {
    Serial.println("Firebase 초기화 성공!");
  }

  Serial.println("버튼을 눌러주세요");
}

void loop() {
  unsigned long currentMillis = millis();

  // 재시작 버튼이 눌렸을 때
  if (digitalRead(Rstart_PIN) == LOW) {
    Serial.println("버튼이 눌렸습니다. 보드를 재시작합니다...");
    delay(1000);
    ESP.restart();
  }

  // 10초마다 무게 측정 및 속도, 남은 시간 계산
  if (currentMillis - lastWeightCheck >= weightInterval) {
    lastWeightCheck = currentMillis;

    // 시작과 끝 무게 측정
    startWeight = scale.get_units(10);
    delay(10000); // 10초 대기
    float endWeight = scale.get_units(10);

    float weightChange = startWeight - endWeight;
    speed = weightChange / 10;
    remainingTime = (speed != 0) ? endWeight / speed : -1;
    displayTime = remainingTime;

    Serial.println("속도: " + String(speed) + " g/s");
    Serial.println("남은 시간: " + String(remainingTime) + " 초");

    float pspeed = savespeed + 0.2;
    float mspeed = savespeed - 0.2;

    // 속도 변화가 감지되면 Firebase 전송 및 LCD 업데이트
    if (speed > pspeed || speed < mspeed) {
      savespeed = speed;

      // Firebase 전송
      if (Firebase.setFloat(firebaseData, "/sensor/remainingTime", remainingTime)) {
        Serial.println("남은 시간 데이터 전송 성공: " + String(remainingTime) + " 초");
      } else {
        Serial.print("남은 시간 데이터 전송 실패: ");
        Serial.println(firebaseData.errorReason());
      }
      delay(2000);

      if (Firebase.setFloat(firebaseData, "/sensor/speed", speed)) {
        Serial.println("속도 데이터 전송 성공: " + String(speed) + " g/s");
      } else {
        Serial.print("속도 데이터 전송 실패: ");
        Serial.println(firebaseData.errorReason());
      }
      delay(2000);

      // displayTime을 업데이트된 remainingTime으로 다시 설정
      displayTime = remainingTime;
    }
  }

  // 매 1초마다 LCD 업데이트 (remainingTime 감소)
  if (currentMillis - lastLCDUpdate >= lcdInterval) {
    lastLCDUpdate = currentMillis;

    if (displayTime > 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Remaining Time:");
      lcd.setCursor(0, 1);
      lcd.print(displayTime);
      lcd.print(" sec");

      displayTime--; // 남은 시간을 1초씩 줄임
    }
  }
}
