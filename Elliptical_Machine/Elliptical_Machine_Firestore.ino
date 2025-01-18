/*___________________________________Connections______________________________________ */
/*
This ESP32-based project is an exercise monitoring system that integrates RFID (MFRC522), MPU6050 (Accelerometer & Gyroscope), 
an RGB LED, and a push button to track elliptical machine workouts. It logs data to Firebase Firestore for cloud storage 
and analysis.
*/
/*
RFID RC522    ESP32
SDA  -------> 15         
SCK  -------> 18
MOSI -------> 23
MISO -------> 19
IRQ  -------> NC
GND  -------> GND
RST  -------> 27
VCC  -------> 3.3V
Pushbutton -> 22
RGB LED          ESP32
Red     -------> 13
Anode   -------> 3.3V
Green   -------> 12
Blue    -------> 14
MPU6050       ESP32
VCC --------> 3.3V         
GND --------> GND 
SCL --------> 26
SDA --------> 25
XDA --------> NC
XCL --------> NC
AD0 --------> GND
INT --------> NC
*/
/* ______________________________________Libraries______________________________________ */
//Wifi
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
//Firebase
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" //Provide the token generation process info.
//RFID
#include <SPI.h>
#include <MFRC522.h>
//Timestamp
#include <Time.h> 
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
//MPU6050
#include <Wire.h> //For communication between MPU6050 and ESP32 (I2C)
#include <Math.h>
/* ______________________________________Macros______________________________________ */
//Wifi
#define WIFI_SSID "" //1. Wifi details are necessary
#define WIFI_PASSWORD ""
//Firebase
#define API_KEY "" //2. Define the API Key 
#define FIREBASE_PROJECT_ID "" //3. Define the project ID 
#define USER_EMAIL "" //4. Define the user Email and password that alreadey registerd or added in your project 
#define USER_PASSWORD ""
//RFID
#define SS_PIN 15
#define RST_PIN 27
//Button
#define buttonPin 22
//For cloud functions
const char* gymName = "addiction";
const char* exName = "elliptical1";
//For RGB LEDs
#define RED 13
#define GREEN 12
#define BLUE 14
//For MPU6050 - threshod for number of rotations
#define Upper_Threshold 0.85
//MPU6050 I2C Pins
#define SCL 26
#define SDA 25
//Define sensitivity scale factors for accelerometer and gyro based on the GYRO_CONFIG and ACCEL_CONFIG register values
#define aScaleFactor 16384
#define gScaleFactor 131
/* ______________________________________Declarations and Variables______________________________________ */
//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
//Create Struct for uploading
typedef struct uploadMessage { // Structure example to receive data. Must match the sender structure
  String loginTimeS;
  String logoutTimeS;
  String dateS;
  String dayS;
  int ongoingS;
  double durationS;
  int countS;
} uploadMessage;
uploadMessage sessionData; // Create a struct_message called myData
//RFID
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
byte uid[4]; // Init array that will store UID
String RFIDCode = "";
//Timestamp
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); 
long int loginTime = 0;
long int logoutTime = 0;
//double duration;
String timeStamp, dateStamp, dayStamp;
//Button
int buttonState; //Logout will be initiated if button state is HIGH
//MPU6050: Since pin AD0 is Grounded, address of the device is b1101000 (0x68) [Check Sec 9.2 of Datasheet]
const uint8_t MPU6050SlaveAddress = 0x68;
//MPU6050: Few configuration register addresses
const uint8_t MPU6050_REGISTER_SMPLRT_DIV   =  0x19;
const uint8_t MPU6050_REGISTER_USER_CTRL    =  0x6A;
const uint8_t MPU6050_REGISTER_PWR_MGMT_1   =  0x6B;
const uint8_t MPU6050_REGISTER_PWR_MGMT_2   =  0x6C;
const uint8_t MPU6050_REGISTER_CONFIG       =  0x1A;
const uint8_t MPU6050_REGISTER_GYRO_CONFIG  =  0x1B;
const uint8_t MPU6050_REGISTER_ACCEL_CONFIG =  0x1C;
const uint8_t MPU6050_REGISTER_FIFO_EN      =  0x23;
const uint8_t MPU6050_REGISTER_INT_ENABLE   =  0x38;
const uint8_t MPU6050_REGISTER_SIGNAL_PATH_RESET  = 0x68;
const uint8_t MPU6050_REGISTER_ACCEL_XOUT_H =  0x3B; //Register 59 (14 Registers (59 to 72) contain accel, temp and gyro data)
//Accelerometer and Gyroscope Variabels
int16_t AX_raw, AY_raw,AZ_raw, GX_raw, GY_raw, GZ_raw, Temp_raw;
double Ax, Ay, Az, T, Gx, Gy, Gz;
//Counter Variables for Accelerometer Rotations
int state; //State Variable
int c_state = 0; //current state
int p_state = 0; //previous state
int count = 0; //counting all edges
//int realcount; //Count for just low to high edges
/* ______________________________________Setup______________________________________ */
void setup() {
  Serial.begin(115200); 
  //Wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //Print dots as long as it is not connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to "); Serial.println(WIFI_SSID);
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  //RFID
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  //Firestore
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);   
  config.api_key = API_KEY; //Assign the api key (required)   
  auth.user.email = USER_EMAIL; //Assign the user sign in credentials 
  auth.user.password = USER_PASSWORD;   
  config.token_status_callback = tokenStatusCallback; //(see addons/TokenHelper.h) Assign the callback function for the long running token generation task 
  Firebase.begin(&config, &auth);    
  Firebase.reconnectWiFi(true);
  //Setup MPU6050
  Wire.begin(SDA, SCL);
  MPU6050_Init(); //Setup the MPU6050 registers
  //RGB LED
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(buttonPin, INPUT);  
  digitalWrite(RED,HIGH); //Note: Common Anode LEDs work on active LOW signals
  digitalWrite(GREEN,HIGH);
  digitalWrite(BLUE,HIGH);
  RGB_color(1, 0, 1); //Green - unoccupied at the start
}
/* ______________________________________Loop______________________________________ */
void loop() {
  //Check for an RFID Card
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()){
    for (byte i = 0; i < 4; i++) {
      uid[i] = rfid.uid.uidByte[i];
    }
  //Get RFID card
  for (byte j = 0; j < rfid.uid.size; j++) 
   {
      RFIDCode.concat(String(rfid.uid.uidByte[j] < 0x10 ? "0" : ""));
      RFIDCode.concat(String(rfid.uid.uidByte[j], HEX));
   }
  //Print RFID Tag
  RFIDCode.toUpperCase();
  RFIDCode=String(RFIDCode);
  Serial.print("RFID Code: "); Serial.println(RFIDCode);
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  //Update NTP for Login time
  timeClient.update();
  loginTime = timeClient.getEpochTime() + (19800); //Set offset wrt your Timezone - 5 hours 30 minutes = 19800s (India is GMT+5:30)
  getDetailedTimeStamp(loginTime);
  //Update struct meesage for firestore
  sessionData.loginTimeS = timeStamp;
  sessionData.dateS = dateStamp;
  sessionData.dayS = dayStamp;
  sessionData.ongoingS = true; //Indicates that RFID has been logged in
  Serial.print("Login time: "); Serial.println(sessionData.loginTimeS);
  rfid.PCD_AntennaOff();  //Turn Antenna off
  Serial.println("Antenna Off "); //Successfully logged in
  RGB_color(0, 1, 1); //RED - occupied
  } //rfid present
  if(sessionData.ongoingS){
    while(1){
      buttonState = digitalRead(buttonPin);
      if(buttonState==HIGH){
        endSession();
        break;
      }
      else if(buttonState==LOW){
        readSensors();
      }
    }//while
  }//ongoing session ie status flag
}
/* ______________________________________Functions______________________________________ */
void endSession(){
  timeClient.update();
  logoutTime = timeClient.getEpochTime() + (19800); //5 hours 30 minutes = 19800s (India is GMT+5:30)
  getDetailedTimeStamp(logoutTime);
  sessionData.logoutTimeS = timeStamp;
  sessionData.durationS = ((hour(logoutTime) - hour(loginTime))*3600 + (minute(logoutTime) - minute(loginTime))*60 + (second(logoutTime) - second(loginTime)));
  //Create Firestore document
  if (Firebase.ready()){
    String content;
    FirebaseJson js;
    String documentPath = "gyms/" + String(gymName) + "/users/" + RFIDCode + "/activity/" + dateStamp + "/today/" + String(exName);
    Serial.println(documentPath);
    js.set("fields/Login/stringValue", sessionData.loginTimeS);
    js.set("fields/Logout/stringValue", sessionData.logoutTimeS);
    js.set("fields/date/stringValue", sessionData.dateS);
    js.set("fields/day/stringValue", sessionData.dayS);
    js.set("fields/duration/integerValue", sessionData.durationS);
    js.set("fields/reps/integerValue", sessionData.countS); //Parameter to be measured - reps
    js.toString(content);
    //Create Doc
    Serial.print("Button pressed! Create a document... ");
    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "" /* databaseId can be (default) or empty */, documentPath.c_str(), content.c_str())){
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
      //Turn Antenna ON
      rfid.PCD_AntennaOn();
      RFIDCode = "";
      Serial.println("Antenna is on");
      RGB_color(1, 0, 1); //Green - unoccupied
      sessionData.ongoingS = false;
    }
    else {
      Serial.println(fbdo.errorReason());
    }
  }
}
//Read MPU6050 Data
void readSensors(){
  //Read sensors here
  Read_RawValue(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_XOUT_H);
  //Divide each with their sensitivity scale factor
  Ax = (double)AX_raw / aScaleFactor;
  Ay = (double)AY_raw / aScaleFactor;
  Az = (double)AZ_raw / aScaleFactor;
  T = (double)Temp_raw / 340 + 36.53; //temperature formula from datasheet
  Gx = (double)GX_raw / gScaleFactor;
  Gy = (double)GY_raw / gScaleFactor;
  Gz = (double)GZ_raw / gScaleFactor; 
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
  sessionData.countS = count / 2;
  Serial.print(" State: "); Serial.print(state);
  Serial.print(" Count: "); Serial.println(sessionData.countS);
  delay(500);
}
//Function to convert epoch time into a readable format
void getDetailedTimeStamp(int logs) {
  timeStamp = ((hour(logs)<10)?("0"+String(hour(logs))):(String(hour(logs)))) + ":" + ((minute(logs)<10)?("0"+String(minute(logs))):(String(minute(logs)))) + ":" + ((second(logs)<10)?("0"+String(second(logs))):(String(second(logs))));
  dateStamp = ((day(logs)<10)?("0"+String(day(logs))):(String(day(logs))))+ "-" + ((month(logs)<10)?("0"+String(month(logs))):(String(month(logs)))) + "-" + String(year(logs));
  switch(weekday(logs))
  {
    case 1:  dayStamp = "Sunday"; break;
    case 2:  dayStamp = "Monday"; break;
    case 3:  dayStamp = "Tuesday"; break;
    case 4:  dayStamp = "Wednesday"; break;
    case 5:  dayStamp = "Thursday"; break;
    case 6:  dayStamp = "Friday"; break;
    default: dayStamp = "Saturday";
  }
}
//Function to get RGB colours
void RGB_color(bool r, bool g, bool b){
  digitalWrite(RED, r);
  digitalWrite(GREEN, g);
  digitalWrite(BLUE, b);
  delay(1000);
}
//Configure and setup MPU6050 Registers
void MPU6050_Init() {
  delay(150);
  //Step 1: Set sample rate divider to get the desired sampling rate of 1kHz based on the formula given in datasheet
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_SMPLRT_DIV, 0x07);
  //Step 2: Set PLL with X axis gyroscope as the clock reference for improved stability.
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_PWR_MGMT_1, 0x01);
  //Step 3: This functionality is not required. Disable it.
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_PWR_MGMT_2, 0x00);
  //Step 4: Disable external Frame Synchronization and disable DLPF so that Gyroscope Output Rate = 8kHz
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_CONFIG, 0x00);
  //Step 5: Set gyroscope full range to +- 250 dps, so that the gyroscope sensitivity scale factor is 131
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_GYRO_CONFIG, 0x00);
  //Step 6: Set accelerometer full range to +- 2g, so that the accelerometer sensitivity scale factor is 16384
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_CONFIG, 0x00);
  //Step 7: This functionality is not required. Disable it.
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_FIFO_EN, 0x00);
  //Step 8: Enable the Data Ready interrupt
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_INT_ENABLE, 0x01);
  //Step 9: Do not reset signal paths
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_SIGNAL_PATH_RESET, 0x00);
  //Step 10: The functionalities provided by the bits of this register are not requires now. Disable them
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_USER_CTRL, 0x00);
}
//14 Registers (59 to 72) contain accel, temp and gyro data. We need to access it
void Read_RawValue(uint8_t deviceAddress, uint8_t regAddress) {
  Wire.beginTransmission(deviceAddress); //Get the slave's attention, tell it we're sending a command byte. Slave = MPU6050
  Wire.write(regAddress); //The command byte sets pointer to register whose address is given
  Wire.endTransmission();
  Wire.requestFrom(deviceAddress, (uint8_t)14); //Used by the master to request bytes from a slave device
  AX_raw = (((int16_t)Wire.read() << 8) | Wire.read());
  AY_raw = (((int16_t)Wire.read() << 8) | Wire.read());
  AZ_raw = (((int16_t)Wire.read() << 8) | Wire.read());
  Temp_raw = (((int16_t)Wire.read() << 8) | Wire.read());
  GX_raw = (((int16_t)Wire.read() << 8) | Wire.read());
  GY_raw = (((int16_t)Wire.read() << 8) | Wire.read());
  GZ_raw = (((int16_t)Wire.read() << 8) | Wire.read());
}
//A function that lets us write data to the slave's registers easily
void I2C_Write(uint8_t deviceAddress, uint8_t regAddress, uint8_t data) {
  Wire.beginTransmission(deviceAddress); //Get the slave's attention, tell it we're sending a command byte. Slave = MPU6050
  Wire.write(regAddress); //The command byte sets pointer to register whose address is given
  Wire.write(data); //Now that the pointer is ‘pointing’ at the specific register you wanted, this command will replace the byte stored in that register with the given data.
  Wire.endTransmission(); //This tells the slave that you’re done giving it instructions for now
}
