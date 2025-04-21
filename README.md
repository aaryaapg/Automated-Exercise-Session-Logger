# Hardware Specifications

**First Drafted:** 10/07/2021    
**Last Updated:** 21/04/2025  
**Purpose:** Describe the hardware specifications and architectural design of Fitolo devices for retrofitting on gym equipment.

---

## 1. Abstract

- Fitolo offers a range of smart devices that can be **retrofitted on gym equipment**.
- Each device includes:
  - A **User Board**: handles interaction via ID device, push button, and indicator.
  - A **Sensor Board**: collects exercise-specific data.
- The **User Board** connects to a **Gateway**, which links to a **Cloud backend**.
- A **prototype mobile app** is available for users to interact with cloud data.

---

## 2. Specifications

| Parameter        | Sensor Board                                        | User Board                                                | Gateway                                                  |
|------------------|-----------------------------------------------------|------------------------------------------------------------|-----------------------------------------------------------|
| **Function**     | Collects exercise metrics during workout            | Handles user interaction                                   | Manages data backup and transfer to the cloud             |
| **Components**   | ESP32, Sensors                                      | ESP32, RFID Scanner, Push Button, RGB LED                 | Memory, UPS, Controller                                   |
| **Communication**| Wireless to User Board (ESP-NOW / BLE / other)     | Wireless to Gateway (ESP-NOW / BLE / other)               | WiFi to Cloud Firestore                                   |
| **Placement**    | On gym equipment                                    | On gym equipment                                           | Central, accessible location                              |
| **Power Source** | Battery (3–6 months)                                | Battery (3–6 months)                                       | AC mains + internal power backup                          |
| **WiFi Support** | ❌                                                   | ❌                                                         | ✅                                                        |
| **Memory**       | ❌                                                   | EEPROM                                                     | SD Card                                                   |
| **Mobility**     | ✅ (movable)                                        | ❌ (static)                                                | ❌ (static)                                               |

> ℹ️ *ESP-NOW has been tested; protocol may change if needed.*

---

## 3. General Architecture
![alt text](<Tested H_W Architecture-Page-2.png>)
---
## 4. Devices

| Device              | Sensor Board Components                               | Common User Board Components                      |
|---------------------|--------------------------------------------------------|---------------------------------------------------|
| **Cycling Machine** | MPU6050 (Accelerometer/Gyroscope), Hall Sensors        | ESP32, RFID RC522, Push Button, LED Indicator     |
| **Elliptical**      | MPU6050 (Accelerometer/Gyroscope)                      | Same as above                                     |
| **Weight Stack**    | MPU6050, Rotary Encoder, Ultrasonic Sensor             | Same as above                                     |
| **Treadmill**       | Hall Sensors                                           | Same as above                                     |

---