#include <Servo.h>

// ================= MOTOR DRIVER =================
#define ENA 5
#define IN1 7
#define IN2 8
#define IN3 9
#define IN4 10
#define ENB 6

// ================= ULTRASONIC =================
#define TRIG 2
#define ECHO 4

// ================= SERVOS =================
Servo baseServo;
Servo armServo;
Servo gripServo;

#define BASE_SERVO_PIN 3
#define ARM_SERVO_PIN 11
#define GRIP_SERVO_PIN 12

// ================= VARIABLES =================
long duration;
int distance;

// ===================================================
// ================= MOTOR FUNCTIONS =================
// ===================================================

void forward() {

  analogWrite(ENA, 180);
  analogWrite(ENB, 180);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void backward() {

  analogWrite(ENA, 180);
  analogWrite(ENB, 180);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void rightTurn() {

  analogWrite(ENA, 180);
  analogWrite(ENB, 180);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void leftTurn() {

  analogWrite(ENA, 180);
  analogWrite(ENB, 180);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void stopMotors() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ===================================================
// ================= ULTRASONIC ======================
// ===================================================

int getDistance() {

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  duration = pulseIn(ECHO, HIGH, 30000);

  // If no echo
  if (duration == 0) {
    return 100;
  }

  distance = duration * 0.034 / 2;

  return distance;
}

// ===================================================
// ================= SERIAL SERVO CONTROL ============
// ===================================================

void handleSerial() {

  if (Serial.available()) {

    String input = Serial.readStringUntil('\n');

    input.trim();

    // ================= BASE SERVO =================
    // Example: B90

    if (input.startsWith("B")) {

      int angle = input.substring(1).toInt();

      angle = constrain(angle, 0, 180);

      baseServo.write(angle);

      Serial.print("Base Servo: ");
      Serial.println(angle);
    }

    // ================= ARM SERVO =================
    // Example: A70

    else if (input.startsWith("A")) {

      int angle = input.substring(1).toInt();

      angle = constrain(angle, 0, 120);

      armServo.write(angle);

      Serial.print("Arm Servo: ");
      Serial.println(angle);
    }

    // ================= GRIPPER SERVO =================
    // Example: G45

    else if (input.startsWith("G")) {

      int angle = input.substring(1).toInt();

      angle = constrain(angle, 0, 100);

      gripServo.write(angle);

      Serial.print("Gripper Servo: ");
      Serial.println(angle);
    }
  }
}

// ===================================================
// ================= SETUP ===========================
// ===================================================

void setup() {

  Serial.begin(9600);

  // Motor Pins
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Ultrasonic
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Attach Servos
  baseServo.attach(BASE_SERVO_PIN);
  armServo.attach(ARM_SERVO_PIN);
  gripServo.attach(GRIP_SERVO_PIN);

  // Initial Servo Positions
  baseServo.write(90);
  armServo.write(60);
  gripServo.write(45);

  Serial.println("===== RescueBot Nano Ready =====");
  Serial.println("Servo Commands:");
  Serial.println("B0-180 = Base Servo");
  Serial.println("A0-120 = Arm Servo");
  Serial.println("G0-90  = Gripper Servo");

  delay(1000);
}

// ===================================================
// ================= LOOP ============================
// ===================================================

void loop() {

  // ================= DISTANCE CHECK =================

  distance = getDistance();

  // Ignore bad readings
  if (distance == 0) {
    distance = 100;
  }

  Serial.print("Distance: ");
  Serial.println(distance);

  // ================= OBSTACLE AVOIDANCE =================

  if (distance > 10) {

    forward();

  } else {

    stopMotors();
    delay(200);

    backward();
    delay(400);

    stopMotors();
    delay(200);

    rightTurn();
    delay(500);

    stopMotors();
    delay(200);
  }

  // ================= SERIAL CONTROL =================

  handleSerial();

  delay(50);
}
