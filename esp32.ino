//the duration for ultrasonic sensor is commend as it need recieve signal to preceed, if not will MAX DELAY

#include <EEPROM.h>
#define EEPROM_SIZE     4

//#define BLUETOOTH_EN    1
#define WIFI_EN         1

#ifdef BLUETOOTH_EN

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
String BT_ID = "C2G01_ESP32";
String message = "";
char incomingChar;
String serialStr = "";
char BTstr[100] = {};

#endif

#ifdef WIFI_EN

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//const char *ssid = "C2G01_ESP32";
//const char *password = NULL;
const char *ssid = "Galaxy A13 A863";
const char *password = "vere5058";
const char *sheetId = "YourGoogleSheetID";
const char *apiKey = "YourGoogleAPIKey";

String sheetName = "Sheet1";
String cell = "A1";
int lastCol = 1;

#endif

#define LED_PIN         2

#define PRESSURE1_PIN   34
#define PRESSURE2_PIN   35

#define TRIG_H_PIN      32
#define ECHO_H_PIN      33
#define TRIG_L_PIN      25
#define ECHO_L_PIN      26
#define TRIG_B_PIN      27
#define ECHO_B_PIN      14
#define SOUND_SPEED     0.034

#define MOTOR_PWM_PIN   23
#define MOTOR_DIR_PIN   22
#define MOTOR_PWM_Ch    0
#define FRONT_PB_PIN    17
#define BACK_PB_PIN     16
#define stop_motor      {ledcWrite(MOTOR_PWM_Ch, 0); digitalWrite(MOTOR_DIR_PIN, 0);}
#define move_motor      {ledcWrite(MOTOR_PWM_Ch, 6000); digitalWrite(MOTOR_DIR_PIN, 1);}
#define return_motor    {ledcWrite(MOTOR_PWM_Ch, -6000); digitalWrite(MOTOR_DIR_PIN, 0);}
#define front_PB        (digitalRead(FRONT_PB_PIN) == 0)
#define back_PB         (digitalRead(BACK_PB_PIN) == 0)

#define VIBRATOR_PIN    21

#define LED_R_PIN       19
#define LED_G_PIN       18
#define LED_B_PIN       5
#define LED_R_Ch        1
#define LED_G_Ch        2
#define LED_B_Ch        3

#define FLASH_PIN       15
#define FLASH_WRITE_PB  digitalRead(FLASH_PIN)

//////////////user threshold/////////////////////

float seat_threshold = 6000.0;           //value when sit
int BACK_THRESHOLD = 3.5;               //minimum distance in cm
float seat_width = 30.0;              //in cm
float dist_compare_threshold = 50.0;  //in percentage
float seat_compare_threshold = 50.0;  //in percentage
uint64_t SIT_TIME = 45 * 60 * 1000;   //45 minutes
uint64_t STAND_TIME = 3 * 60 * 1000; //3 minutes

/////////////////////////////////////////////////

uint8_t start_flag = 1;
uint8_t alert = 0;
uint8_t update_en = 0;

float H_dist = 0, L_dist = 0, Diff_dist = 0;
float Back_dist = 0;

uint8_t led1 = 0;
uint8_t ledR_PWM = 0, ledG_PWM = 0, ledB_PWM = 0;

uint8_t devices = 0;
uint16_t pressure1_data = 0, pressure2_data = 0;
float left_pressure = 0, right_pressure = 0;
float Diff_pressure = 0.0;

uint8_t flash_PB_flag = 0;

uint32_t led_count = 0;
uint32_t BT_count = 0;
uint32_t serial_count = 0;
uint32_t leave_count = 0;
uint32_t sitting_count = 0;
uint32_t flash_count = 0;
uint32_t sit_time = 0;
uint32_t leave_time = 0;

////////////////////////////////////////////

void pressure (void);
void distance (void);
void back_support (void);
void check (void);

////////////////////////////////////////////

String getColumnLetter(int col);
void updateGoogleSheet(uint32_t value);

////////////////////////////////////////////

void setup() {
  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);
  pinMode(FLASH_WRITE_PB, INPUT_PULLUP);
  seat_width = EEPROM.read(0);
  
  // put your setup code here, to run once:
//  analogReadResolution(12);       //4096, 3.2V
//  analogSetClockDiv(1);
//  setCpuFrequencyMhz(80);
  
  pinMode(TRIG_H_PIN, OUTPUT);
  pinMode(ECHO_H_PIN, INPUT);
  pinMode(TRIG_L_PIN, OUTPUT);
  pinMode(ECHO_L_PIN, INPUT);
  pinMode(TRIG_B_PIN, OUTPUT);
  pinMode(ECHO_B_PIN, INPUT);

  ledcAttachPin(MOTOR_PWM_PIN, MOTOR_PWM_Ch);
  ledcSetup(0, 1000, 15);         //1000Hz, up to 20000 (32767, 0x7FFF)
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(FRONT_PB_PIN, INPUT_PULLUP);
  pinMode(BACK_PB_PIN, INPUT_PULLUP);

  pinMode(VIBRATOR_PIN, OUTPUT);
  
  ledcAttachPin(LED_R_PIN, LED_R_Ch);
  ledcAttachPin(LED_G_PIN, LED_G_Ch);
  ledcAttachPin(LED_B_PIN, LED_B_Ch);
  ledcSetup(1, 1000, 8);          //1000Hz, up to 256
  ledcSetup(2, 1000, 8);
  ledcSetup(3, 1000, 8);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.begin(115200);
  
#ifdef BLUETOOTH_EN
  SerialBT.begin(BT_ID); //Bluetooth device name
#endif

#ifdef WIFI_EN
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  EEPROM.get(2, sit_time);

  // Initialize the Time library
  configTime(0, 0, "pool.ntp.org");
  Serial.println("Waiting for time sync");
  while (time(nullptr) < 1510644967) {
    delay(10);
  }
  
  // Get the current date and time
  time_t now = time(nullptr);
  struct tm *timeInfo = localtime(&now);

  String url = "https://sheets.googleapis.com/v4/spreadsheets/" + String(sheetId) + "/values/" + sheetName + "?key=" + String(apiKey);

  // Prepare HTTP GET request to read the last value in the Google Sheet
  HTTPClient http;
  http.begin(url);
  int httpResponseCode = http.GET();

  // Compare with the last sit_time
  if (httpResponseCode > 0) {
    // Parse the JSON response
    DynamicJsonDocument jsonDoc(1024);
    deserializeJson(jsonDoc, http.getString());

    // Extract the last value from the Google Sheet
    String lastValue = jsonDoc["values"][0][0];

    // Compare with the last stored EEPROM value
    if (lastValue.toInt() != sit_time) {
      // Values are different, update the Google Sheet
      updateGoogleSheet(sit_time);
    } else {
      Serial.println("sit_time in EEPROM is the same as the last value in Google Sheet. No update needed.");
    }
  } else {
    Serial.print("HTTP Request failed. Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  
#endif
}

void loop() {
  // put your main code here, to run repeatedly:

  sit_time = (millis() - sitting_count) / 1000;
  leave_time = (millis() - leave_count) / 1000;
  
  if(millis() - led_count >= 20){
    led1 = !led1;
    digitalWrite(LED_PIN, led1);
    led_count = millis();
    check();
  }

  pressure();
  distance();
//  back_support();
  
  if(millis() - serial_count >= 300){
    Serial.print("\n\nCPU time: ");
    Serial.print(millis());
    Serial.print(" , PB: (front) ");
    Serial.print(front_PB);
    Serial.print(" (back) ");
    Serial.print(back_PB);
    Serial.print(" , flag: ");
    Serial.print(start_flag);
    Serial.print(", ");
    Serial.println(alert);
    
    Serial.print("Back_dist: ");
    Serial.print(Back_dist);
    Serial.print("  Upper_dist: ");
    Serial.print(H_dist);
    Serial.print("  Lower_dist: ");
    Serial.print(L_dist);
    Serial.print("  Diff_dist (%): ");
    Serial.println(Diff_dist);
    
    Serial.print("Pressure: ");
    Serial.print(left_pressure);
    Serial.print(", ");
    Serial.print(right_pressure);
    Serial.print(", Pressure_dist (%): ");
    Serial.println(Diff_pressure);

    Serial.print("Time (s): (sit) ");
    Serial.print(sit_time); //sit time
    Serial.print(", (stand) ");
    Serial.println(leave_time); //leave time
    serial_count = millis();
  }

  if(FLASH_WRITE_PB == 0 && flash_PB_flag == 0){
    delay(20);
    flash_PB_flag = 1;
    seat_width = L_dist;
    EEPROM.write(0, seat_width);
    EEPROM.commit();
  }else if(FLASH_WRITE_PB && flash_PB_flag){
    flash_PB_flag = 0;
  }

  if(millis() - flash_count >= 60 * 1000){
    flash_count = millis();
    EEPROM.put(2, sit_time);
    EEPROM.commit();

#ifdef WIFI_EN
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
#endif
  }
}

void pressure(void){
  pressure1_data = analogRead(PRESSURE1_PIN);
  pressure2_data = analogRead(PRESSURE2_PIN);

  left_pressure = pressure1_data;
  right_pressure = pressure2_data;

  if((left_pressure + right_pressure) > seat_threshold)
    start_flag = 0;
  else
    start_flag = 1;

  if(left_pressure > right_pressure){
    Diff_pressure = (left_pressure - right_pressure) / left_pressure * 100.0;
  }else{
    Diff_pressure = (right_pressure - left_pressure) / right_pressure * 100.0;
  }
}

void back_support (void){
  static uint8_t move_support = 1;
  if(start_flag == 0){
    move_support = 1;
    return;
  }
  digitalWrite(TRIG_B_PIN, 0);
  delay(2);
  digitalWrite(TRIG_B_PIN, 1);
  delay(10);
  digitalWrite(TRIG_B_PIN, 0);
  long durationB = pulseIn(ECHO_B_PIN, 1);
//  long durationB;

  Back_dist = durationB * SOUND_SPEED / 2;
  if(move_support){
    move_motor
    if(Back_dist <= BACK_THRESHOLD || back_PB){
      stop_motor
      move_support = 0;
    }
  }
}

void distance (void){
  if(start_flag == 0){
    H_dist = 0.0;
    L_dist = 0.0;
    Diff_dist = 0.0;
    return;
  }
  
  digitalWrite(TRIG_H_PIN, 0);
  delay(2);
  digitalWrite(TRIG_H_PIN, 1);
  delay(10);
  digitalWrite(TRIG_H_PIN, 0);
  long duration1 = pulseIn(ECHO_H_PIN, 1);
  
  digitalWrite(TRIG_L_PIN, 0);
  delay(2);
  digitalWrite(TRIG_L_PIN, 1);
  delay(10);
  digitalWrite(TRIG_L_PIN, 0);
  long duration2 = pulseIn(ECHO_L_PIN, 1);
  
//  long duration1, duration2;
  H_dist = duration1 * SOUND_SPEED / 2;
  L_dist = duration2 * SOUND_SPEED / 2;

  if(H_dist <= 3.0 || L_dist <= 3.0)     //if not detected or user's back touched to back
    Diff_dist = 0.0;
  else if(H_dist > L_dist)
    Diff_dist = (H_dist - L_dist) / H_dist * 100.0;
  else
    Diff_dist = (L_dist - H_dist) / L_dist * 100.0;
}

void check (void){
  if(start_flag == 0){
    sitting_count = millis();
    if(update_en && millis() - leave_count >= 3 * 60 * 1000){

      update_en = 0;

#ifdef BLUETOOTH_EN
    if (SerialBT.available()) {
      Serial.write(SerialBT.read());
    }
    sprintf(BTstr, "Sit time = %u s\n", sit_time);
    SerialBT.write((uint8_t *)&BTstr, strlen(BTstr));
#endif

#ifdef WIFI_EN
  updateGoogleSheet(sit_time);
#endif

    }else if(millis() - leave_count >= STAND_TIME){
      if(front_PB)
        stop_motor
      else
        return_motor
      alert = 5;
    }else
      alert = 0;

  }else{
    leave_count = millis();
    if(millis() - sitting_count >= 3 * 60 * 1000 && update_en == 0){
      update_en = 1;
    }
    
    if(millis() - sitting_count >= SIT_TIME){
      alert = 4;
    }else if(L_dist > seat_width)
      alert = 2;
    else if(Diff_dist > dist_compare_threshold)
      alert = 1;
    else if(Diff_pressure > seat_compare_threshold)
      alert = 3;
    else
      alert = 0;
  }

  switch(alert){
    case 0:   //green
      digitalWrite(VIBRATOR_PIN, 0);
      ledR_PWM = 0;
      ledG_PWM += 5;
      ledB_PWM = 0;
      if(ledG_PWM >= 250)
        ledG_PWM = 5;
      ledcWrite(LED_R_Ch, ledR_PWM);
      ledcWrite(LED_G_Ch, ledG_PWM);
      ledcWrite(LED_B_Ch, ledB_PWM);
      break;
    case 1:   //red
      digitalWrite(VIBRATOR_PIN, 1);
      ledR_PWM += 5;
      ledG_PWM = 0;
      ledB_PWM = 0;
      if(ledR_PWM >= 250)
        ledR_PWM = 5;
      ledcWrite(LED_R_Ch, ledR_PWM);
      ledcWrite(LED_G_Ch, ledG_PWM);
      ledcWrite(LED_B_Ch, ledB_PWM);
      break;
    case 2:   //yellow
      digitalWrite(VIBRATOR_PIN, 1);
      ledR_PWM += 5;
      ledG_PWM += 2;
      ledB_PWM = 0;
      if(ledR_PWM >= 250){
        ledR_PWM = 5;
        ledG_PWM = 5;
      }
      ledcWrite(LED_R_Ch, ledR_PWM);
      ledcWrite(LED_G_Ch, ledG_PWM);
      ledcWrite(LED_B_Ch, ledB_PWM);
      break;
    case 3:   //blue
      digitalWrite(VIBRATOR_PIN, 1);
      ledR_PWM = 0;
      ledG_PWM = 0;
      ledB_PWM += 5;
      if(ledB_PWM >= 250)
        ledB_PWM = 5;
      ledcWrite(LED_R_Ch, ledR_PWM);
      ledcWrite(LED_G_Ch, ledG_PWM);
      ledcWrite(LED_B_Ch, ledB_PWM);
      break;
    case 4:   //purple
      digitalWrite(VIBRATOR_PIN, 1);
      ledR_PWM += 5;
      ledG_PWM = 0;
      ledB_PWM += 5;
      if(ledR_PWM >= 250){
        ledR_PWM = 5;
        ledB_PWM = 5;
      }
      ledcWrite(LED_R_Ch, ledR_PWM);
      ledcWrite(LED_G_Ch, ledG_PWM);
      ledcWrite(LED_B_Ch, ledB_PWM);
      break;
    case 5:   //off
      digitalWrite(VIBRATOR_PIN, 0);
      ledR_PWM = 0;
      ledG_PWM = 0;
      ledB_PWM = 0;
      ledcWrite(LED_R_Ch, ledR_PWM);
      ledcWrite(LED_G_Ch, ledG_PWM);
      ledcWrite(LED_B_Ch, ledB_PWM);
      break;
  }   
}

void updateGoogleSheet(uint32_t value) {
  // Retrieve the last-used cell information
  int lastRow = 1;  // You may adjust this based on your specific requirements
  int lastCol = 1;  // You may adjust this based on your specific requirements

  // Build the URL for the Google Sheets API to add the new value
  String addUrl = "https://sheets.googleapis.com/v4/spreadsheets/" + String(sheetId) + "/values/" + sheetName + "!" + getColumnLetter(lastCol) + String(lastRow) + "?key=" + String(apiKey);

  // Prepare HTTP PUT request
  HTTPClient http;
  http.begin(addUrl);
  http.addHeader("Content-Type", "application/json");

  // Build the JSON payload
  DynamicJsonDocument addJsonDoc(256);
  addJsonDoc["values"] = value;

  // Convert JSON to string
  String addJsonPayload;
  serializeJson(addJsonDoc, addJsonPayload);

  // Send PUT request to add the new value
  int addHttpResponseCode = http.PUT(addJsonPayload);

  // Check for errors
  if (addHttpResponseCode > 0) {
    Serial.println("Added new value to Google Sheet.");
  } else {
    Serial.print("Failed to add new value to Google Sheet. Error code: ");
    Serial.println(addHttpResponseCode);
  }

  // Close connection
  http.end();
}

String getColumnLetter(int col) {
  String columnLetter = "";
  while (col > 0) {
    int remainder = col % 26;
    if (remainder == 0) {
      columnLetter = 'Z' + columnLetter;
      col = col / 26 - 1;
    } else {
      columnLetter = char('A' + remainder - 1) + columnLetter;
      col = col / 26;
    }
  }
  return columnLetter;
}
