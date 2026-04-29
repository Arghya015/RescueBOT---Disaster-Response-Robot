# Disaster Management Robot (Arduino)

## Overview
This project is an autonomous disaster management robot built using Arduino.  
It detects fire, gas leakage, motion disturbances, and obstacles, and responds accordingly using alerts and navigation logic. A servo-mounted camera continuously scans the surroundings.

---

## Features

- Fire detection using dual flame sensors
- Gas detection using MQ2 sensor
- Motion detection using MPU6050 (X and Y axis monitoring)
- Obstacle detection and avoidance using ultrasonic sensor
- Continuous servo-based scanning mechanism
- Buzzer and LED alert system (synchronized)
- Autonomous forward movement with obstacle handling
- Serial monitoring with periodic updates (2 seconds interval)

---

## System Working

### Normal Operation
- Robot moves forward
- Servo continuously sweeps from left to right and right to left

### Alert Conditions (Fire / Gas / Motion)
- Robot stops immediately
- Servo stops at current position
- Buzzer and LED activate (two short beeps)

### Obstacle Detection
- Robot stops
- Turns right to avoid obstacle
- Resumes forward motion

### Serial Monitoring
- Data printed every 2 seconds:
  - Fire status
  - Gas value
  - Distance
  - Acceleration (AX, AY)
  - Change in axis (dX, dY)
  - Motion status

---

## Hardware Components

| Component              | Quantity |
|-----------------------|----------|
| Arduino Uno           | 1        |
| MPU6050 Sensor        | 1        |
| MQ2 Gas Sensor        | 1        |
| Flame Sensor          | 2        |
| Ultrasonic Sensor     | 1        |
| L298N Motor Driver    | 1        |
| DC Motors             | 2        |
| Servo Motor           | 1        |
| Buzzer                | 1        |
| LED                   | 1        |

---


## Software Requirements

- Arduino IDE
- Required Libraries:
  - Wire.h
  - Servo.h

---

## Setup Instructions

1. Connect all components as per the pin configuration
2. Install required libraries in Arduino IDE
3. Upload the code to Arduino Uno
4. Open Serial Monitor (baud rate: 9600)
5. Power the system

---

## Configuration Parameters

| Parameter            | Description                          |
|---------------------|--------------------------------------|
| gasThreshold        | Gas detection sensitivity            |
| motionThreshold     | Motion detection sensitivity         |
| obstacleDistance    | Minimum distance for obstacle action |
| stepDelay           | Servo speed control                  |

---

## Sample Serial Output
