#include "secrets.h"   // Wi-Fi·Firebase 설정 (secrets.example.h 복사)
#include <Arduino.h>        // 아두이노 라이브러리
#include <WiFi.h>           // ESP32 와이파이 라이브러리
#include <FirebaseESP32.h>  // Firebase 연동 라이브러리 (ESP32 용)
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <HX711.h>          // 로드셀 라이브러리
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

LiquidCrystal_PCF8574 lcd(0x27);  // LCD 주소 설정 (주소는 0x27 또는 0x3F일 수 있음)

// 버튼 
#define Rstart_PIN 18
#define Start_PIN 5

// 무게 센서 핀 설정
#define DT_PIN 12          // HX711의 DT 핀 (ESP32의 GPIO 12)
#define SCK_PIN 13         // HX711의 SCK 핀 (ESP32의 GPIO 13)
HX711 scale;               // HX711 객체 생성

// 광센서 핀 설정
#define TCR_PIN 4            // ESP32의 GPIO 4로 설정
volatile bool dropletDetected = false; // 물방울 감지 상태를 저장하는 변수
int count = 0;
void IRAM_ATTR handleDroplet() {
  dropletDetected = true; // 인터럽트 발생 시 감지 상태를 true로 설정
}

// WiFi 설정
const char* ssid = WIFI_SSID;           // Wi-Fi 이름
const char* password = WIFI_PASSWORD;  // Wi-Fi 비밀번호

// Firebase 설정
// API_KEY: secrets.h            // Firebase API Key
// DATABASE_URL: secrets.h   // Firebase Database URL
// USER_EMAIL: secrets.h                            // Firebase 사용자 이메일
// USER_PASSWORD: secrets.h                                  // Firebase 사용자 비밀번호

// Firebase 객체 선언
FirebaseConfig firebaseConfig;  // Firebase 설정 정보를 담는 객체
FirebaseAuth firebaseAuth;      // Firebase 인증 정보를 담는 객체
FirebaseData firebaseData;      // Firebase 데이터 전송/수신을 위한 객체

// 변수 처리
bool startpart = true; // 첫 번째 파트
bool firstpart = false; // 첫 번째 파트
bool secondpart = false; // 두 번째 파트
bool thirdpart = false; // 세 번째 파트

float setweight = 100.0; // 수액통, 센서 무게 ㅇㅋ
float pweight = 0; // 10초 전의 무게
float aweigh = 0; //10초 후의 무게
float weight = 0; // 현재 무게
float savespeed = 0;
float startweight = 0;



void setup() {
  // LCD 초기화
  lcd.begin(16, 2);
  lcd.setBacklight(255);   // LCD 백라이트 켜기 (0~255 범위 설정)
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  pinMode(Start_PIN, INPUT_PULLUP);  // INPUT_PULLUP 모드로 변경
  pinMode(Rstart_PIN, INPUT_PULLUP);  // INPUT_PULLUP 모드로 변경

  Serial.begin(115200);  // 시리얼 통신 초기화
  delay(3000);
  Serial.println("시작");

  // 로드셀 설정
  scale.begin(DT_PIN, SCK_PIN);  // HX711 초기화
  scale.set_scale(309);          // 보정값 설정
  scale.tare();                  // 현재 무게를 0으로 설정

  // 광센서 핀모드 설정
  pinMode(TCR_PIN, INPUT);         // 광센서 핀을 입력으로 설정

  // WiFi 연결 시도
  Serial.print("와이파이 연결중: ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n와이파이 연결 성공!");

  // Firebase 설정 및 인증
  firebaseConfig.api_key = API_KEY;
  firebaseConfig.database_url = DATABASE_URL;
  firebaseAuth.user.email = USER_EMAIL;
  firebaseAuth.user.password = USER_PASSWORD;

  Firebase.begin(&firebaseConfig, &firebaseAuth); // Firebase 시작

  if (!Firebase.ready()) {
    Serial.println("Firebase 초기화 실패!");
  } else {
    Serial.println("Firebase 초기화 성공!");
  }

  // 인터럽트 설정: TCRT5000_PIN 핀에서 FALLING 상태(LOW로 떨어질 때)에서 handleDroplet() 함수를 호출
  attachInterrupt(digitalPinToInterrupt(TCR_PIN), handleDroplet, FALLING);

  // lcd 화면 출력
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready, done");
  lcd.setCursor(0, 1);
  lcd.print("Press Btn!");

  Serial.println("버튼을 눌러주세요");
}

void loop() {
  if (digitalRead(Rstart_PIN) == LOW) {
    Serial.println("버튼이 눌렸습니다. 보드를 재시작합니다...");
    delay(1000);
    ESP.restart();
  }

  if (startpart == true && digitalRead(Start_PIN) == LOW) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wait");
    lcd.setCursor(0, 1);
    lcd.print("Measuring...");

    weight = scale.get_units(10) - setweight;
    firstpart = true;
    startpart = false; 
    startweight = weight;

    if (Firebase.setFloat(firebaseData, "/802/StartWeight", weight)) {
      Serial.println("무게 데이터 전송 성공: " + String(weight) + "g");
    } else {
      Serial.print("무게 데이터 전송 실패: ");
      Serial.println(firebaseData.errorReason());
    }
    delay(2000);
  }

  while (firstpart) {
    if (digitalRead(Rstart_PIN) == LOW) {
        Serial.println("보드를 재시작합니다...");
        delay(1000);
        ESP.restart();
    }

    Serial.println("정상적 첫 파트 변경 완료");

    // 10초 전 무게 측정
    float startWeight = scale.get_units(10) - setweight;
    Serial.println("10초 전 무게: " + String(startWeight));

    int dropCount = 0; // 물방울 횟수를 세는 변수
    bool lastDetection = false; // 이전 물방울 감지 상태 변수

    unsigned long startTime = millis();
    while (millis() - startTime < 10000) {
        bool currentDetection = digitalRead(TCR_PIN) == HIGH;

        if (currentDetection && !lastDetection) { // 새로운 물방울 감지가 발생한 경우
            dropCount++;
            
            // 물방울이 완전히 떨어질 때까지 대기
            while (digitalRead(TCR_PIN) == HIGH) {
                // 센서가 LOW 상태로 돌아올 때까지 대기
            }
        }
        
        lastDetection = currentDetection; // 현재 감지 상태를 기록
    }

    // 10초 후 무게 측정
    float endWeight = scale.get_units(10) - setweight;
    Serial.println("10초 후 무게: " + String(endWeight));

    // 물방울 횟수 출력
    Serial.println("10초 동안 떨어진 물방울 수: " + String(dropCount));

    // 무게 변화량 계산
    float weightChange = startWeight - endWeight; // 무게 변화량

    // 평균 물방울당 무게 계산
    float weightPerDrop = (dropCount > 0) ? weightChange / dropCount : 0;
    Serial.println("평균 물방울당 무게: " + String(weightPerDrop) + " g");

    // 속도 계산 (g/s 단위로 계산)
    float speed = weightChange / 10; // 속도 (g/s)
    Serial.println("속도: " + String(speed) + " g/s");

    // 남은 시간 계산 (초 단위)
    float remainingTime = (speed != 0) ? endWeight / speed : -1;
    Serial.println("남은 시간: " + String(remainingTime) + " 초");

    if (Firebase.setFloat(firebaseData, "/802/remainingTime", remainingTime)) {
        Serial.println("남은 시간 데이터 전송 성공: " + String(remainingTime) + " 초");
    } else {
        Serial.print("남은 시간 데이터 전송 실패: ");
        Serial.println(firebaseData.errorReason());
    }
    delay(2000);

    if (Firebase.setFloat(firebaseData, "/802/speed", speed)) {
        Serial.println("속도 데이터 전송 성공: " + String(speed) + " g/s");
    } else {
        Serial.print("속도 데이터 전송 실패: ");
        Serial.println(firebaseData.errorReason());
    }
    delay(2000);

    dropCount = 0;

    firstpart = false;
    secondpart = true;
    savespeed = speed;
    
    float proWeight = scale.get_units(10) - setweight;
    int progress = 100 - ((proWeight /startweight)*100);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Progress");
    lcd.setCursor(0, 1);
    lcd.print(progress);
    lcd.print("%");

    
  }


  while (secondpart) {
    if (digitalRead(Rstart_PIN) == LOW) {
        Serial.println("보드를 재시작합니다...");
        delay(1000);
        ESP.restart();
    }

    // 10초 전 무게 측정
    float startWeight = scale.get_units(10) - setweight;
    Serial.println("10초 전 무게: " + String(startWeight));

    int dropCount = 0; // 물방울 횟수를 세는 변수
    bool lastDetection = false; // 이전 물방울 감지 상태 변수

    unsigned long startTime = millis();
    while (millis() - startTime < 10000) {
        bool currentDetection = digitalRead(TCR_PIN) == HIGH;

        if (currentDetection && !lastDetection) { // 새로운 물방울 감지가 발생한 경우
            dropCount++;
            
            // 물방울이 완전히 떨어질 때까지 대기
            while (digitalRead(TCR_PIN) == HIGH) {
                // 센서가 LOW 상태로 돌아올 때까지 대기
            }
        }
        
        lastDetection = currentDetection; // 현재 감지 상태를 기록
    }

    // 10초 후 무게 측정
    float endWeight = scale.get_units(10) - setweight;
    Serial.println("10초 후 무게: " + String(endWeight));

    // 물방울 횟수 출력
    Serial.println("10초 동안 떨어진 물방울 수: " + String(dropCount));

    // 무게 변화량 계산
    float weightChange = startWeight - endWeight; // 무게 변화량

    // 음수 값 처리
    if (weightChange < 0) {
        Serial.println("이상한 움직임 감지 다시 측정");
        delay(1000); // 잠시 대기 후 다시 측정
        continue; // 루프 시작으로 되돌아감
    }


    // 평균 물방울당 무게 계산
    float weightPerDrop = (dropCount > 0) ? weightChange / dropCount : 0;
    Serial.println("평균 물방울당 무게: " + String(weightPerDrop) + " g");

    // 속도 계산 (g/s 단위로 계산)
    float speed = weightChange / 10; // 속도 (g/s)
    Serial.println("속도: " + String(speed) + " g/s");

    // 남은 시간 계산 (초 단위)
    float remainingTime = (speed != 0) ? endWeight / speed : -1;
    Serial.println("남은 시간: " + String(remainingTime) + " 초");
    
    float pspeed = savespeed + 0.05;
    float mspeed = savespeed - 0.05;


    if(speed > pspeed || speed < mspeed )
    {
      savespeed = speed;
      if (Firebase.setFloat(firebaseData, "/802/remainingTime", remainingTime)) {
      Serial.println("남은 시간 데이터 전송 성공: " + String(remainingTime) + " 초");
    } else {
      Serial.print("남은 시간 데이터 전송 실패: ");
      Serial.println(firebaseData.errorReason());
    }
    delay(2000);

    if (Firebase.setFloat(firebaseData, "/802/speed", speed)) {
      Serial.println("속도 데이터 전송 성공: " + String(speed) + " g/s");
    } else {
      Serial.print("속도 데이터 전송 실패: ");
      Serial.println(firebaseData.errorReason());
    }
    delay(2000);
    }

    // dropCount 초기화
    dropCount = 0;

    float proWeight = scale.get_units(10) - setweight;
    int progress = 100 - ((proWeight /startweight)*100);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Progress");
    lcd.setCursor(0, 1);
    lcd.print(progress);
    lcd.print("%");

    Serial.println("진행도 :"+ (String)progress);
  }
}
