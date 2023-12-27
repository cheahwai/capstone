//the duration for ultrasonic sensor is commend as it need recieve signal to preceed, if not will MAX DELAY

#include <EEPROM.h>
#define EEPROM_SIZE       64

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
uint8_t update_en = 0;

#endif

#ifdef WIFI_EN

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char *ssid = "";
const char *password = "";
const char* googleAppsScriptURL = "https://script.google.com/macros/s/AKfycbzsbIjGoYmz-6omRmBLgRw18gZpx0hVqXdTKmaJhBPX3GhRZJmLlRUgleFoWIShLuTH/exec";

HTTPClient http;
String sheetName = "Sheet1";
String cell = "A1";
int lastCol = 1;
uint8_t update_en = 0;

String getColumnLetter(int col);
void updateGoogleSheet(uint32_t value);

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

#define MOTOR_PWM_PIN   21
#define MOTOR_DIR1_PIN  19
#define MOTOR_DIR2_PIN  18
#define MOTOR_PWM_Ch    0
#define FRONT_LIMIT_PIN 22
#define BACK_LIMIT_PIN  23
#define stop_motor      {ledcWrite(MOTOR_PWM_Ch, 0); digitalWrite(MOTOR_DIR1_PIN, 0); digitalWrite(MOTOR_DIR2_PIN, 0);}
#define forward_motor   {ledcWrite(MOTOR_PWM_Ch, 100); digitalWrite(MOTOR_DIR1_PIN, 1); digitalWrite(MOTOR_DIR2_PIN, 0);}
#define return_motor    {ledcWrite(MOTOR_PWM_Ch, 100); digitalWrite(MOTOR_DIR1_PIN, 0); digitalWrite(MOTOR_DIR2_PIN, 1);}
#define front_limit     digitalRead(FRONT_LIMIT_PIN)
#define back_limit      digitalRead(BACK_LIMIT_PIN)

#define VIBRATOR_PIN    15

#define LED_R_PIN       5
#define LED_G_PIN       17
#define LED_B_PIN       16
#define LED_R_Ch        1
#define LED_G_Ch        2
#define LED_B_Ch        3

//////////////user threshold/////////////////////

float seat_threshold = 6000.0;        //threshold value when sit
int min_dist = 4.5;                   //minimum distance in cm
float seat_width = 30.0;              //in cm
float dist_compare_threshold = 50.0;  //in percentage
float seat_compare_threshold = 50.0;  //in percentage
uint64_t SIT_TIME = 45 * 60 * 1000;   //45 minutes
uint64_t STAND_TIME = 3 * 60 * 1000;  //3 minutes

/////////////////////////////////////////////////

uint8_t start_flag = 1;
uint8_t alert = 0;

float H_dist = 0, L_dist = 0, Diff_dist = 0;
float Back_dist = 0;

uint8_t led1 = 0;
uint8_t ledR_PWM = 0, ledG_PWM = 0, ledB_PWM = 0;

uint16_t pressure1_data = 0, pressure2_data = 0;
float left_pressure = 0, right_pressure = 0;
float Diff_pressure = 0.0;

uint8_t flash_PB_flag = 0;

uint32_t led_count = 0;
uint32_t BT_count = 0;
uint32_t serial_count = 0;
uint32_t leave_count = 0;
uint32_t sitting_count = 0;
uint32_t flash_count= 0;
uint32_t sit_time = 0;
uint32_t leave_time = 0;

//debug use
uint8_t motor_move = 0;       
////////////////////////////////////////////

void pressure (void);
void distance (void);
void back_support (void);
void check (void);

////////////////////////////////////////////

void setup() {

	EEPROM.begin(EEPROM_SIZE);
	seat_width = EEPROM.read(0);

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
	ledcSetup(0, 100, 8);         //100Hz, CCR up to 256
	pinMode(MOTOR_DIR1_PIN, OUTPUT);
	pinMode(MOTOR_DIR2_PIN, OUTPUT);
	pinMode(FRONT_LIMIT_PIN, INPUT_PULLUP);
	pinMode(BACK_LIMIT_PIN, INPUT_PULLUP);

	pinMode(VIBRATOR_PIN, OUTPUT);

	ledcAttachPin(LED_R_PIN, LED_R_Ch);
	ledcAttachPin(LED_G_PIN, LED_G_Ch);
	ledcAttachPin(LED_B_PIN, LED_B_Ch);
	ledcSetup(LED_R_Ch, 1000, 8);          //1000Hz, CCR up to 256
	ledcSetup(LED_G_Ch, 1000, 8);
	ledcSetup(LED_B_Ch, 1000, 8);
	pinMode(LED_PIN, OUTPUT);

	Serial.begin(115200);

#ifdef BLUETOOTH_EN
	SerialBT.begin(BT_ID); //Bluetooth device name
#endif

#ifdef WIFI_EN
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.println("Connecting to WiFi...");
	}
	Serial.println("Connected to WiFi");

	// EEPROM.get(2, sit_time);

	// // Initialize the Time library
	// configTime(0, 0, "pool.ntp.org");
	// Serial.println("Waiting for time sync");
	// while (time(nullptr) < 1510644967) {
	//   delay(10);
	// }
	//
	//  // Get the current date and time
	//  time_t now = time(nullptr);
	//  struct tm *timeInfo = localtime(&now);
	//
	//  String url = "https://sheets.googleapis.com/v4/spreadsheets/" + String(sheetId) + "/values/" + sheetName;
	//
	//  // Prepare HTTP GET request to read the last value in the Google Sheet
	//  HTTPClient http;
	//  http.begin(url);
	//  int httpResponseCode = http.GET();
	//
	//  // Compare with the last sit_time
	//  if (httpResponseCode > 0) {
	//    // Parse the JSON response
	//    DynamicJsonDocument jsonDoc(1024);
	//    deserializeJson(jsonDoc, http.getString());
	//
	//    // Extract the last value from the Google Sheet
	//    String lastValue = jsonDoc["values"][0][0];
	//
	//    // Compare with the last stored EEPROM value
	//    if (lastValue.toInt() != sit_time) {
	//      // Values are different, update the Google Sheet
	//      updateGoogleSheet(sit_time);
	//    } else {
	//      Serial.println("sit_time in EEPROM is the same as the last value in Google Sheet. No update needed.");
	//    }
	//  } else {
	//    Serial.print("HTTP Request failed. Error code: ");
	//    Serial.println(httpResponseCode);
	//  }
	//  http.end();

#endif

	Serial.println("\nsetup done, start running in ");
  for(int i = 5; i > 0; i--){
    Serial.print(i);
    Serial.print("s ");
	  delay(1000);
  }
}

void loop() {

	pressure();
	distance();
	back_support();

	sit_time = (millis() - sitting_count) / 1000;
	leave_time = (millis() - leave_count) / 1000;

	if(millis() - led_count >= 20){
		led1 = !led1;
		digitalWrite(LED_PIN, led1);
		led_count = millis();
		check();
	}

	if(millis() - serial_count >= 3000){

		Serial.print("\n\nCPU time: ");
		Serial.print(millis());
		Serial.print(" , PB: (front) ");
		Serial.print(front_limit);
		Serial.print(" (back) ");
		Serial.print(back_limit);
		Serial.print(" (motor) ");
		Serial.print(motor_move);
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
		Serial.print(sit_time);
		Serial.print(", (stand) ");
		Serial.println(leave_time);
		serial_count = millis();
	}

	//  if(FLASH_WRITE_PB == 0 && flash_PB_flag == 0){
	//    delay(20);
	//    flash_PB_flag = 1;
	//    seat_width = L_dist;
	//    EEPROM.write(0, seat_width);
	//    EEPROM.commit();
	//  }else if(FLASH_WRITE_PB && flash_PB_flag){
	//    flash_PB_flag = 0;
	//  }

	if(millis() - flash_count >= 60 * 1000 && start_flag){
		flash_count = millis();
		EEPROM.put(2, sit_time);
		EEPROM.commit();

#ifdef WIFI_EN
		if (WiFi.status() != WL_CONNECTED) {
			Serial.println("Reconnecting to WiFi...");
			WiFi.disconnect();
			WiFi.reconnect();
		}
		updateGoogleSheet(start_flag);
#endif
	}
}

void pressure(void){
	pressure1_data = analogRead(PRESSURE1_PIN);
	pressure2_data = analogRead(PRESSURE2_PIN);

	left_pressure = pressure1_data;
	right_pressure = pressure2_data;

	start_flag = ((left_pressure + right_pressure) < seat_threshold);
  Diff_pressure = abs(left_pressure - right_pressure) / max(left_pressure, right_pressure) * 100.0;
}

float get_dist (int TRIG_PIN, int ECHO_PIN){
  digitalWrite(TRIG_PIN, 0);
	delay(2);
	digitalWrite(TRIG_PIN, 1);
	delay(10);
	digitalWrite(TRIG_PIN, 0);
  return (float)(pulseIn(ECHO_PIN, 1) * SOUND_SPEED / 2);
}

void back_support (void){
	Back_dist = get_dist(TRIG_B_PIN, ECHO_B_PIN);
	bool limitSwitch1 = (back_limit == 0);   // 
	bool limitSwitch2 = (front_limit == 0);  // Active LOW

	if (Back_dist > min_dist && Back_dist < seat_width && !limitSwitch1 && start_flag) {
		forward_motor
		motor_move = 1;
	} else if (Back_dist >= seat_width && !limitSwitch2) {
		return_motor
		motor_move = 2;
	} else {
		stop_motor
		motor_move = 3;
	}
}

void distance (void){
	if(start_flag == 0){
		H_dist = 0.0;
		L_dist = 0.0;
		Diff_dist = 0.0;
		return;
	}

	H_dist = get_dist(TRIG_H_PIN, ECHO_H_PIN);
	L_dist = get_dist(TRIG_L_PIN, ECHO_L_PIN);

  Diff_dist = (H_dist <= min_dist || L_dist <= min_dist) ? 3.0 :
            abs(H_dist - L_dist) / max(H_dist, L_dist) * 100.0;
}

void output_device(int vibrator_en, int RLED_en, int GLED_en, int BLED_en){
  digitalWrite(VIBRATOR_PIN, vibrator_en);
  ledR_PWM = RLED_en? ledR_PWM + 5 : 0;
  ledG_PWM = GLED_en && RLED_en? ledG_PWM + 2 :
             GLED_en? ledG_PWM + 2 : 0;
  ledB_PWM = BLED_en? ledB_PWM + 5 : 0;
  if(ledR_PWM >= 250) ledR_PWM = 5;
  if(ledG_PWM >= 250) ledG_PWM = 5;
  if(ledB_PWM >= 250) ledB_PWM = 5;
  ledcWrite(LED_R_Ch, ledR_PWM);
  ledcWrite(LED_G_Ch, ledG_PWM);
  ledcWrite(LED_B_Ch, ledB_PWM);
}

void check (void){
	if(start_flag == 0){
		sitting_count = millis();
    if(millis() - leave_count >= STAND_TIME){
			alert = 5;
#ifdef BLUETOOTH_EN
    }else if(update_en && millis() - leave_count >= 3 * 60 * 1000){
			update_en = 0;
			if (SerialBT.available()) {
				Serial.write(SerialBT.read());
			}
			sprintf(BTstr, "Sit time = %u s\n", sit_time);
			SerialBT.write((uint8_t *)&BTstr, strlen(BTstr));
#endif
		}else
			alert = 0;

	}else{
		leave_count = millis();

#ifdef BLUETOOTH_EN
		if(millis() - sitting_count >= 3 * 60 * 1000 && update_en == 0){
			update_en = 1;
		}
#endif

		if(millis() - sitting_count >= SIT_TIME)
			alert = 4;
		else if(L_dist > seat_width)
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
    output_device(0, 0, 1, 0);
		break;
	case 1:   //red
    output_device(1, 1, 0, 0);
		break;
	case 2:   //yellow
		output_device(1, 1, 1, 0);
		break;
	case 3:   //blue
		output_device(1, 0, 0, 1);
		break;
	case 4:   //purple
    output_device(1, 1, 0, 1);
		break;
	case 5:   //off
    output_device(0, 0, 0, 0);
		break;
	}   
}

#ifdef WIFI_EN
void updateGoogleSheet(uint32_t value) {

	http.begin(googleAppsScriptURL);
	http.addHeader("Content-Type", "application/x-www-form-urlencoded");

	String postData = "sittingTime=" + String(value);

	int httpResponseCode = http.POST(postData);

	if (httpResponseCode > 0) {
		String response = http.getString();
		Serial.println(response);
	}

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

#endif
