#include <Servo.h>

// ===== Pin Map =====
const int ENA = 5;
const int ENB = 6;
const int IN1 = 7;
const int IN2 = 8;
const int IN3 = 9;
const int IN4 = 10;

const int SERVO_PIN = 11;
const int TRIG_PIN = 2;
const int ECHO_PIN = 3;
const int PUMP_PIN = A1;

const int FLAME_L = 12;
const int FLAME_C = 4;
const int FLAME_R = 13;

#define WATER_SENSOR A4
#define LED_FIRE A2
#define LED_WATER A3
#define BUZZER A0

// ===== Tham số =====
int waterThreshold = 300;

int normalSpeed = 60;
int avoidSpeed  = 90;
int fireSpeed   = 60;

int servoCenter = 90;
int servoLeft   = 150;
int servoRight  = 30;

int obstacleStopDist = 25;
int fireSprayDist    = 40;

bool useSerialDebug = true;

// ===== Biến =====
Servo nozzle;
bool spraying = false;

// ===== Prototype =====
long readDistanceCM();
void moveForward();
void moveBackward();
void moveForwardFire();
void turnLeft();
void turnRight();
void stopCar();
void aimNozzleCenter();
void aimNozzleLeft();
void aimNozzleRight();
void pumpOn();
void pumpOff();

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(PUMP_PIN, OUTPUT);
  pumpOff();

  pinMode(FLAME_L, INPUT);
  pinMode(FLAME_C, INPUT);
  pinMode(FLAME_R, INPUT);

  pinMode(LED_FIRE, OUTPUT);
  pinMode(LED_WATER, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  nozzle.attach(SERVO_PIN);
  aimNozzleCenter();

  if (useSerialDebug) {
    Serial.begin(9600);
    delay(300);
    Serial.println(F("=== XE CHỮA CHÁY BẮT ĐẦU ==="));
  }

  stopCar();
}

void loop() {
  // ==== Đọc mực nước ====
  int waterLevel = analogRead(WATER_SENSOR);
  bool waterEmpty = (waterLevel < waterThreshold);
  digitalWrite(LED_WATER, waterEmpty ? HIGH : LOW);

  // ==== Đọc cảm biến khác ====
  long dist = readDistanceCM();

  // ==== Lọc nhiễu cảm biến lửa ====
  int fireCount = 0;
  for (int i = 0; i < 5; i++) {
    if (digitalRead(FLAME_L) == LOW ||
        digitalRead(FLAME_C) == LOW ||
        digitalRead(FLAME_R) == LOW)
      fireCount++;
    delay(2);
  }
  bool fireDetected = (fireCount >= 3);  // 3/5 lần có lửa → chấp nhận

  // ==== Trường hợp HC-SR04 trả về 0 khi vật quá gần ====
  bool tooClose = (dist == 0 && fireDetected);

  // ==== Serial hiển thị ====
  Serial.print("Khoảng cách (cm): "); Serial.print(dist);
  Serial.print(" | Mực nước: "); Serial.print(waterLevel);
  Serial.print(" | Hết nước: "); Serial.print(waterEmpty ? "Có" : "Không");
  Serial.print(" | Phát hiện lửa: "); Serial.print(fireDetected ? "Có" : "Không");
  Serial.print(" | Bơm: "); Serial.print(spraying ? "Bật" : "Tắt");

  // ==== Trạng thái xe ====
  String carState = "Dừng";
  if (!fireDetected && dist > obstacleStopDist)
    carState = "Đi thẳng";
  else if (!fireDetected && dist > 0 && dist <= obstacleStopDist)
    carState = "Tránh vật cản";
  else if (fireDetected && !waterEmpty && (tooClose || dist > fireSprayDist))
    carState = "Tiến về lửa";
  Serial.print(" | Trạng thái xe: "); Serial.println(carState);

  // ==== Báo lửa & còi ====
  if (fireDetected && !waterEmpty) {
    digitalWrite(LED_FIRE, HIGH);
    tone(BUZZER, 1000);
  } else {
    digitalWrite(LED_FIRE, LOW);
    noTone(BUZZER);
  }

  // ==== Hết nước ====
  if (waterEmpty) {
    stopCar();
    pumpOff();
    spraying = false;
    aimNozzleCenter();
    delay(50);
    return;
  }

  // ==== Nếu đang phun mà mất lửa → tắt bơm ngay ====
  if (!fireDetected && spraying) {
    spraying = false;
    pumpOff();
  }

  // ==== Servo tự trả về giữa khi hết lửa ====
  if (!fireDetected)
    aimNozzleCenter();

  // ==== Né vật cản khi không có lửa ====
  if (!fireDetected && dist > 0 && dist <= obstacleStopDist) {
    stopCar();
    delay(100);
    moveBackward();
    delay(400);
    stopCar();
    delay(100);
    turnLeft();
    delay(500);
    stopCar();
    delay(100);
    moveForward();
    delay(50);
    return;
  }

  // ==== Chạy thẳng khi không có lửa và không vướng vật cản ====
  if (!fireDetected && dist > obstacleStopDist) {
    moveForward();
    delay(20);
    return;
  }

  // ==== Có lửa nhưng có vật cản gần → ưu tiên né ====
  if (fireDetected && dist > 0 && dist <= obstacleStopDist) {
    stopCar();
    delay(100);
    moveBackward();
    delay(300);
    stopCar();
    delay(100);
    turnLeft();
    delay(400);
    stopCar();
    delay(100);
    return;
  }

  // ==== Nếu đang phun → không cho xe chạy để tránh rung ====
  if (spraying) {
    stopCar();
    delay(10);
    return;
  }

  // ==== Xử lý chữa cháy ====
  if (fireDetected && !waterEmpty) {

    // Xác định hướng lửa
    int flameL = digitalRead(FLAME_L);
    int flameC = digitalRead(FLAME_C);
    int flameR = digitalRead(FLAME_R);

    // Lửa hai bên
    if (flameL == LOW && flameR == LOW)
      aimNozzleCenter();
    else if (flameL == LOW && flameC == HIGH && flameR == HIGH)
      aimNozzleLeft();
    else if (flameR == LOW && flameC == HIGH && flameL == HIGH)
      aimNozzleRight();
    else
      aimNozzleCenter();

    // Khoảng cách
    if (tooClose || dist <= fireSprayDist) {
      stopCar();
      if (!spraying) {
        spraying = true;
        pumpOn();
      }
ccc;''      moveForwardFire();
      delay(100);
      stopCar();
    }

    delay(10);
    return;
  }

  spraying = false;
}

// ===== Hàm đo khoảng cách HC-SR04 =====
long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 25000UL);
  if (duration == 0) return 0;
  return duration / 58;
}

// ===== Điều khiển động cơ =====
void moveForward() {
  analogWrite(ENA, normalSpeed);
  analogWrite(ENB, normalSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void moveBackward() {
  analogWrite(ENA, avoidSpeed);
  analogWrite(ENB, avoidSpeed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void moveForwardFire() {
  analogWrite(ENA, fireSpeed);
  analogWrite(ENB, fireSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  analogWrite(ENA, avoidSpeed);
  analogWrite(ENB, avoidSpeed); 
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnRight() {
  analogWrite(ENA, avoidSpeed);
  analogWrite(ENB, avoidSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void stopCar() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ===== Servo =====
void aimNozzleCenter() { nozzle.write(servoCenter); }
void aimNozzleLeft()   { nozzle.write(servoLeft); }
void aimNozzleRight()  { nozzle.write(servoRight); }

// ===== Máy bơm =====
void pumpOn()  { digitalWrite(PUMP_PIN, HIGH); }
void pumpOff() { digitalWrite(PUMP_PIN, LOW); }