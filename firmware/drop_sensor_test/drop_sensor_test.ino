#include <Arduino.h>
#include <HX711.h>

int count = 1;
int sum = 0;
#define TCRT5000_PIN 4 // TCRT5000의 DO 핀

#define DT_PIN 12          // HX711의 DT 핀 (ESP32의 GPIO 12)
#define SCK_PIN 13         // HX711의 SCK 핀 (ESP32의 GPIO 13)
HX711 scale;               // HX711 객체 생성

void setup() {
  Serial.begin(115200);          // 시리얼 통신 시작
  pinMode(TCRT5000_PIN, INPUT);  // TCRT5000 핀을 입력으로 설정

  Serial.println("TCRT5000 Sensor Example");
  
  // 로드셀 설정
  scale.begin(DT_PIN, SCK_PIN);  // HX711 초기화
  scale.set_scale(309);          // 보정값 설정
  scale.tare();                  // 현재 무게를 0으로 설정
}

void loop() {
  int lightValue = digitalRead(TCRT5000_PIN); // TCRT5000 값 읽기

  // 물방울이 떨어지는 순간 0 출력
  if (lightValue == LOW && count == 1) {
    Serial.println("0"); // 물방울 떨어짐 표시
    sum += 1;
    Serial.print("물방울 횟수: ");
    Serial.println(sum); // 물방울 횟수 출력
    count = 0;
  } else if (lightValue == HIGH && count == 0) {
    count = 1;
  }

  // 무게 측정 및 노이즈 제거
  float startWeight = scale.get_units(10);

  // -0.3보다 작은 값은 0으로 간주
  if (abs(startWeight) < 0.3) {
    startWeight = 0;
  }
  Serial.println(startWeight);

  // 1초마다 영점 조정 가능
  scale.tare();
  delay(1000);
}
