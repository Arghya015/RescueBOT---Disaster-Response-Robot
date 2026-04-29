#include <Wire.h>
#include <Servo.h>

// ================= MPU =================
const int MPU = 0x68;
int16_t ax, ay, az;
int prevAx = 0, prevAy = 0;
int motionThreshold = 8000;
int motionCount = 0;

// ================= MOTOR =================
#define ENA 5
#define IN1 6
#define IN2 7
#define ENB 11
#define IN3 12
#define IN4 13

// ================= SENSORS =================
#define FLAME1 2
#define FLAME2 4
#define MQ2 A0

// ================= ULTRASONIC =================
#define TRIG A2
#define ECHO A1

// ================= ALERT =================
#define BUZZER 8
#define LED 9

// ================= SERVO =================
Servo camServo;
int servoPin = 10;

int angle = 10;
int direction = 1;
unsigned long lastMove = 0;
int stepDelay = 30;

// ================= SERIAL TIMER =================
unsigned long lastSerialTime = 0;

// ================= THRESHOLDS =================
int gasThreshold = 130;
int obstacleDistance = 20;

bool alertActive = false;

// ================= SETUP =================
void setup() {
  Serial.begin(9600);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(FLAME1, INPUT);
  pinMode(FLAME2, INPUT);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);

  camServo.attach(servoPin);

  // MPU init
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  stopCar();
}

// ================= LOOP =================
void loop() {

  //  FIRE
  bool fireDetected = (digitalRead(FLAME1) == LOW || digitalRead(FLAME2) == LOW);

  //  GAS
  int gasValue = analogRead(MQ2);
  bool gasDetected = (gasValue > gasThreshold);

  //  MPU
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true);

  ax = Wire.read() << 8 | Wire.read();
  ay = Wire.read() << 8 | Wire.read();
  az = Wire.read() << 8 | Wire.read();

  int diffX = abs(ax - prevAx);
  int diffY = abs(ay - prevAy);

  if (diffX > motionThreshold || diffY > motionThreshold) {
    motionCount++;
  } else {
    motionCount = 0;
  }

  prevAx = ax;
  prevAy = ay;

  bool motionDetected = (motionCount > 1);

  // 📡 DISTANCE
  int distance = getDistance();

  // 🚨 ALERT STATE
  bool alertNow = (fireDetected || gasDetected || motionDetected);

  // 🎥 SERVO CONTROL
  sweepServo(alertNow);

  // ================= SERIAL =================
  if (millis() - lastSerialTime >= 2000) {
    lastSerialTime = millis();

    if (fireDetected) {
      Serial.print("🔥 FIRE | ");
    } else {
      Serial.print("No Fire | ");
    }

    Serial.print("Gas: "); Serial.print(gasValue);
    Serial.print(" | Dist: "); Serial.print(distance);

    Serial.print(" | AX: "); Serial.print(ax);
    Serial.print(" | AY: "); Serial.print(ay);

    Serial.print(" | dX: "); Serial.print(diffX);
    Serial.print(" | dY: "); Serial.print(diffY);

    if (motionDetected) {
      Serial.print(" | MOTION!");
    } else {
      Serial.print(" | Stable");
    }

    Serial.println();
  }

  // ================= LOGIC =================

  if (alertNow) {
    stopCar();

    if (!alertActive) {
      beepTwice();
      alertActive = true;
    }
  }

  else if (distance > 0 && distance < obstacleDistance) {
    stopCar();
    delay(200);
    turnRight();
    delay(400);
  }

  else {
    moveForward();
    alertActive = false;
  }

  delay(50);
}

// ================= ULTRASONIC =================
int getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);
  if (duration == 0) return 999;

  return duration * 0.034 / 2;
}

// ================= SERVO =================
void sweepServo(bool stopServo) {

  if (stopServo) return;  

  if (millis() - lastMove >= stepDelay) {
    lastMove = millis();

    angle += direction;

    if (angle >= 170) direction = -1;
    if (angle <= 10) direction = 1;

    camServo.write(angle);
  }
}

// ================= MOVEMENT =================
void moveForward() {
  analogWrite(ENA, 90);
  analogWrite(ENB, 90);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  analogWrite(ENA, 90);
  analogWrite(ENB, 90);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void stopCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

// ================= ALERT =================
void beepTwice() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED, HIGH);
    delay(200);

    digitalWrite(BUZZER, LOW);
    digitalWrite(LED, LOW);
    delay(200);
  }
}
