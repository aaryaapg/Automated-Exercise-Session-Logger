/* ______________________________________Connections______________________________________ 
This code enables automatic tracking of elliptical workouts using RFID authentication, motion sensing, 
and cloud logging via Firebase. The session starts when an RFID card is scanned, tracks movement using MPU6050, 
and ends when a button is pressed, saving workout data remotely.

RFID RC522    NodeMCU
SDA  -------> D4         
SDK  -------> D5
MOSI -------> D7
MISO -------> D6
IRQ  -------> NC
GND  -------> GND
RST  -------> D3
VCC  -------> 3.3V

MPU6050       NodeMCU
VCC --------> 3.3V         
GND --------> GND 
SCL --------> D1
SDA --------> D2
XDA --------> NC
XCL --------> NC
AD0 --------> NC
INT --------> NC

Button --------> NodeMCU (D8)

*/

/* ______________________________________Libraries______________________________________ */
//For Firebase & WiFi Connectivity
#include <FirebaseESP8266.h>
#include <FirebaseESP8266HTTPClient.h>
#include <FirebaseFS.h>
#include <FirebaseJson.h>
#include <ESP8266WiFi.h> 
#include <deprecated.h>
//For RFID & Login-Logout Time
#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <require_cpp11.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Time.h> 
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
//For MPU6050
#include <Wire.h>
#include<math.h>

/* ______________________________________Macros______________________________________ */
//For RFID
#define SS_PIN D4  
#define RST_PIN D3 
//For Firebase & WiFi Connectivity
#define FIREBASE_HOST ""  
#define FIREBASE_AUTH ""  
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
//For MPU6050 - threshod for number of rotations
#define Upper_Threshold 0.85 
//For Button
#define buttonPin D8

/* ______________________________________Declarations and Variables: Login-Logout, Firebase, RFID______________________________________ */
//For Login-Logout Time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); 
String DATE, TIMESTAMP, WEEKDAY;
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
FirebaseData firebaseData;
FirebaseJson json;
int statuss = 0; //status is a keyword, hence the extra s
int out = 0;
long int login_time=0;
String rfidcode="";
long int logout_time=0;
double duration;
String path="/";   

/* ______________________________________Declarations and Variables: MPU6050______________________________________ */
// MPU6050 Slave Device Address
const uint8_t MPU6050SlaveAddress = 0x68;

// Select SDA and SCL pins for I2C communication 
const uint8_t scl = D1;
const uint8_t sda = D2;

// sensitivity scale factor respective to full scale setting provided in datasheet 
const uint16_t AccelScaleFactor = 16384;
const uint16_t GyroScaleFactor = 131;

// MPU6050 few configuration register addresses
const uint8_t MPU6050_REGISTER_SMPLRT_DIV   =  0x19;
const uint8_t MPU6050_REGISTER_USER_CTRL    =  0x6A;
const uint8_t MPU6050_REGISTER_PWR_MGMT_1   =  0x6B;
const uint8_t MPU6050_REGISTER_PWR_MGMT_2   =  0x6C;
const uint8_t MPU6050_REGISTER_CONFIG       =  0x1A;
const uint8_t MPU6050_REGISTER_GYRO_CONFIG  =  0x1B;
const uint8_t MPU6050_REGISTER_ACCEL_CONFIG =  0x1C;
const uint8_t MPU6050_REGISTER_FIFO_EN      =  0x23;
const uint8_t MPU6050_REGISTER_INT_ENABLE   =  0x38;
const uint8_t MPU6050_REGISTER_ACCEL_XOUT_H =  0x3B;
const uint8_t MPU6050_REGISTER_SIGNAL_PATH_RESET  = 0x68;

//Accelerometer and Gyroscope Variabels
int16_t AX_raw, AY_raw,AZ_raw, GX_raw, GY_raw, GZ_raw, Temp_raw;
double Ax, Ay, Az, T, Gx, Gy, Gz;

//Counter Variables for Accelerometer Rotations
int state; //State Variable
int c_state = 0; //current state
int p_state = 0; //previous state
int count = 0; //counting all edges
int realcount; //Count for just low to high edges

/* ______________________________________Setup______________________________________ */
void setup() {
  Serial.begin(9600);
  //Connecting to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  
  Serial.print("connecting");  
  while (WiFi.status() != WL_CONNECTED) {  
    Serial.print(".");   
    delay(500);  
  }  
  Serial.println();  
  Serial.print("Connected: ");  
  Serial.println(WiFi.localIP());  
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  timeClient.begin();
  Wire.begin(sda, scl);
  MPU6050_Init();
  pinMode(buttonPin, INPUT); // declare push button as input
}

/* ______________________________________Loop______________________________________ */
void loop() {
   timeClient.update();
   startSession();
   if(statuss==1){
    readSensors();
   }
   else {
    return;
   }
   timeClient.update();
}

/* ______________________________________Functions______________________________________ */

void startSession(){
  Serial.println(".");
  login_time=0;
  logout_time=0;
  rfidcode="";
  String content= "";
  byte letter;
  //Read Card
   if ( ! mfrc522.PICC_IsNewCardPresent()){ 
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()){
    Serial.println("No cards available");
    return;
    }
  //Print UID Tag
  Serial.println(" UID tag :");  
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  rfidcode=String(content.substring(1));
  path.concat("members/");
  path.concat(rfidcode);
  path.concat("/");
  path.concat("Activity");
  //Therefore path="/"+"members"+"/"+rfidcode+"/"+"Activity";
  timeClient.update();
  login_time = timeClient.getEpochTime() + (19800); //5 hours 30 minutes = 19800s (India is GMT+5:30)
  getTimestamp(login_time);
  Firebase.setString(firebaseData, path + "/elliptical/Login", TIMESTAMP);
  Firebase.setString(firebaseData, path + "/elliptical/Date", DATE + ", " + WEEKDAY);
  Firebase.setInt(firebaseData, path + "/elliptical/Count", 0);
  Firebase.setString(firebaseData, path + "/elliptical/Logout", "0");
  Firebase.setBool(firebaseData, path + "/elliptical/Ongoing_Session", true);
  delay(500);  
    mfrc522.PCD_AntennaOff();
    Serial.println(" Antenna Off "); //Successfully logged in
    statuss=1; //This indicates that an RFID card has been logged in. The sensors will be read as long as statuss=1.
  }

void readSensors() {
  int buttonState; //Logout will be initiated if button state is HIGH
  while(1) {
    buttonState = digitalRead(buttonPin);
    //delay(100);
    if(buttonState==HIGH) { //Button Pressed - Logout Procedure
      statuss=0;
      timeClient.update();
      logout_time = timeClient.getEpochTime() + (19800); //5 hours 30 minutes = 19800s (India is GMT+5:30)
      getTimestamp(logout_time);
      int Reps = realcount;
      Firebase.setInt(firebaseData, path + "/elliptical/Count", Reps);
      Firebase.setString(firebaseData, path + "/elliptical/Logout", TIMESTAMP);        
      //To get the duration (in Seconds)
      duration = ((hour(logout_time) - hour(login_time))*3600 + (minute(logout_time) - minute(login_time))*60 + (second(logout_time) - second(login_time)));
      Firebase.setDouble(firebaseData, path + "/elliptical/Duration", duration);
      Firebase.setBool(firebaseData, path + "/elliptical/Ongoing_Session", false);
      delay(500);  
      path="/";
      mfrc522.PCD_AntennaOn();
      break;
    }
    else if(buttonState==LOW) { 
      Serial.println("Reading Sensors");
      Read_RawValue(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_XOUT_H);
      //Calculations: divide each with their sensitivity scale factor
      Ax = (double)AX_raw/AccelScaleFactor;
      Ay = (double)AY_raw/AccelScaleFactor;
      Az = (double)AZ_raw/AccelScaleFactor;
      T = (double)Temp_raw/340+36.53; //temperature formula
      Gx = (double)GX_raw/GyroScaleFactor;
      Gy = (double)GY_raw/GyroScaleFactor;
      Gz = (double)GZ_raw/GyroScaleFactor;
      
      //Comparator
      if(Az > Upper_Threshold) {
        state = 1;
      }
      else {
        state = 0;
      }
      c_state = state;
      // compare the current state to its previous state
      if (c_state != p_state) {
          // if the state has changed, increment the counter
          count++;
          // Delay a little bit to avoid bouncing
      }
      else{
        count = count;
      }
      // save the current state as the last state, for next time through the loop
      p_state = c_state;
      realcount = count / 2;
      Serial.print(" State: "); Serial.print(state);
      Serial.print(" Count: "); Serial.println(realcount);
      delay(500); 
    }
  }
}

void I2C_Write(uint8_t deviceAddress, uint8_t regAddress, uint8_t data){
  Wire.beginTransmission(deviceAddress);
  Wire.write(regAddress);
  Wire.write(data);
  Wire.endTransmission();
}

// read all 14 register
void Read_RawValue(uint8_t deviceAddress, uint8_t regAddress){
  Wire.beginTransmission(deviceAddress);
  Wire.write(regAddress);
  Wire.endTransmission();
  Wire.requestFrom(deviceAddress, (uint8_t)14);
  AX_raw = (((int16_t)Wire.read()<<8) | Wire.read());
  AY_raw = (((int16_t)Wire.read()<<8) | Wire.read());
  AZ_raw = (((int16_t)Wire.read()<<8) | Wire.read());
  Temp_raw = (((int16_t)Wire.read()<<8) | Wire.read());
  GX_raw = (((int16_t)Wire.read()<<8) | Wire.read());
  GY_raw = (((int16_t)Wire.read()<<8) | Wire.read());
  GZ_raw = (((int16_t)Wire.read()<<8) | Wire.read());
}

//configure MPU6050
void MPU6050_Init(){
  delay(150);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_SMPLRT_DIV, 0x07);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_PWR_MGMT_1, 0x01);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_PWR_MGMT_2, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_CONFIG, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_GYRO_CONFIG, 0x00);//set +/-250 degree/second full scale
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_CONFIG, 0x00);// set +/- 2g full scale
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_FIFO_EN, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_INT_ENABLE, 0x01);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_SIGNAL_PATH_RESET, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_USER_CTRL, 0x00);
}

void getTimestamp(int logs) {
  TIMESTAMP = String(hour(logs)) + ":" + String(minute(logs)) + ":" + String(second(logs));
  DATE = String(day(logs))+ "-" + String(month(logs)) + "-" + String(year(logs));
  switch(weekday(logs))
  {
    case 1:  WEEKDAY = "Sunday"; break;
    case 2:  WEEKDAY = "Monday"; break;
    case 3:  WEEKDAY = "Tuesday"; break;
    case 4:  WEEKDAY = "Wednesday"; break;
    case 5:  WEEKDAY = "Thursday"; break;
    case 6:  WEEKDAY = "Friday"; break;
    default: WEEKDAY = "Saturday";
  }
}
