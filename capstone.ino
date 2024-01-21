#include <EEPROM.h>
#define EEPROM_SIZE       (2 * (sizeof(uint64_t) + sizeof(float) * 2))

#define WIFI_EN         1
//#define BLUETOOTH_EN      1

#ifdef BLUETOOTH_EN

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
String BT_ID = "C2G01_ESP32";
char BTstr[100] = {};

#endif

#ifdef WIFI_EN

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>

const char *ssid = "Your ID";
const char *password = "password";
const char* googleAppsScriptURL = "https://script.google.com/macros/s/AKfycbzsbIjGoYmz-6omRmBLgRw18gZpx0hVqXdTKmaJhBPX3GhRZJmLlRUgleFoWIShLuTH/exec";

HTTPClient http;
WebServer server(80);
int address = 0;
float value = 0.0;

void updateGoogleSheet(uint32_t value);

#endif  

#define LED_PIN         2

#define PRESSURE1_PIN   34
#define PRESSURE2_PIN   35
#define PRESSURE3_PIN   32
#define PRESSURE4_PIN   33

#define TRIG_PIN        27
#define ECHO_PIN        14
#define SOUND_SPEED     0.0343

#define VIBRATOR_PIN    21

#define AUDIO_PIN       25

#define LED_R_PIN       5
#define LED_G_PIN       17
#define LED_B_PIN       16
#define LED_R_Ch        0
#define LED_G_Ch        1
#define LED_B_Ch        2

//////////// Function Prototype /////////////////

void pressure (void);
void distance (void);
void back_support(void);
void check (void);

/////////////////////////////////////////////////

///////////// user threshold ////////////////////

float seat_threshold = 15500.0;        //threshold value when sit
uint16_t back_threshold = 3850.0;     //threshold for back touching
int min_dist = 3.5;                   //minimum distance in cm
float seat_width = 42.0;              //in cm
float dist_compare_threshold = 12.0;  //in cm
float seat_compare_threshold = 23.0;  //in percentage
uint64_t SIT_TIME = 30 * 60 * 1000;   //30 minutes
uint64_t STAND_TIME = 3 * 60 * 1000;  //3 minutes
uint64_t VIBRATE_TIME = 5 * 1000;     //5 seconds

/////////////////////////////////////////////////

uint8_t start_flag = 1;
uint8_t alert = 5;

float dist = 0;
bool echoFallen = false;

uint8_t led1 = 0;
uint8_t ledR_PWM = 0, ledG_PWM = 0, ledB_PWM = 0;

uint16_t pressure1_data = 0, pressure2_data = 0;
uint16_t pressure3_data = 0, pressure4_data = 0;
float left_pressure = 0, right_pressure = 0;
float Diff_pressure = 0.0;
uint8_t back_touched = 0;

uint8_t flash_PB_flag = 0;

uint32_t led_count = 0;
uint32_t serial_count = 0;
uint32_t flash_count= 0;
uint32_t echoStart = 0, echoEnd = 0;
uint32_t leave_count = 0;
uint32_t sitting_count = 0;
uint32_t back_count = 0;
uint32_t side_count = 0;
uint32_t front_count = 0;
uint32_t sit_time = 0;
uint32_t leave_time = 0;

///////////// Timer Interrupt ///////////////////

hw_timer_t *print_timer = NULL, *blink_timer = NULL;

void IRAM_ATTR print_timer_callback(){
    Serial.println();
    
    Serial.print(SIT_TIME / 1000);
    Serial.print(" s, ");
    Serial.print(seat_compare_threshold);
    Serial.println("%");
    
    Serial.print(start_flag);
    Serial.print(", ");
    Serial.println(alert);

    Serial.println(dist);

    Serial.print(left_pressure);
    Serial.print("   ");
    Serial.print(right_pressure);
    Serial.print("   ");
    Serial.println(Diff_pressure);

    Serial.print(pressure3_data);
    Serial.print("   ");
    Serial.print(pressure4_data);
    Serial.print("   ");
    Serial.println(back_touched);

    Serial.print(sit_time);
    Serial.print("   ");
    Serial.print(leave_time);
    Serial.print("   ");
    Serial.print((millis() - side_count)/1000);
    Serial.print("   ");
    Serial.print((millis() - front_count)/1000);
    Serial.print("   ");
    Serial.println((millis() - back_count)/1000);

#ifdef BLUETOOTH_EN
    sprintf(BTstr, "flash: %u %f\n", SIT_TIME / 60 / 1000, seat_compare_threshold);
    SerialBT.write((uint8_t *)&BTstr, strlen(BTstr));
    sprintf(BTstr, "flag: %u %d\n", start_flag, alert);
    SerialBT.write((uint8_t *)&BTstr, strlen(BTstr));
    sprintf(BTstr, "dist: %.2f \n", dist);
    SerialBT.write((uint8_t *)&BTstr, strlen(BTstr));
    sprintf(BTstr, "pres: %.2f %.2f %.2f%%\n", left_pressure, right_pressure, Diff_pressure);
    SerialBT.write((uint8_t *)&BTstr, strlen(BTstr));
    sprintf(BTstr, "back: %d %d %d\n", pressure3_data, pressure4_data, back_touched);
    SerialBT.write((uint8_t *)&BTstr, strlen(BTstr));
    sprintf(BTstr, "time: %u %u %u %u %u\n\n", sit_time, leave_time, (millis() - side_count)/1000, (millis() - front_count)/1000, (millis() - back_count)/1000);
    SerialBT.write((uint8_t *)&BTstr, strlen(BTstr));
#endif

}

void IRAM_ATTR blink_timer_callback(){
    led1 = !led1;
    digitalWrite(LED_PIN, led1);
}

#ifdef WIFI_EN

hw_timer_t *googlesheet_timer = NULL;

void IRAM_ATTR googlesheet_timer_callback(){
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
    updateGoogleSheet(start_flag);
}

#endif

/////////////////////////////////////////////////

////////////// GPIO Interrupt ///////////////////

void ultrasonicEcho(){
  switch(digitalRead(ECHO_PIN)){
    case HIGH:
      echoStart = micros();
      break;
    case LOW:
      echoEnd = micros();
      //set falling edge flag
      echoFallen = true;
  }
}

/////////////////////////////////////////////////

////////////////// Sound ////////////////////////

#include "hunched_forward_16000.h"
#include "sitting_overtime_16000.h"
#include "slouching_to_side_16000.h"
#include "slumped_backward_16000.h"
#include "XT_DAC_Audio.h"

XT_Wav_Class Sound_Side(slouching_to_side_16000);   
XT_Wav_Class Sound_Time(sitting_overtime_16000);
XT_Wav_Class Sound_Back(slumped_backward_16000);
XT_Wav_Class Sound_Front(hunched_forward_16000);
           
XT_DAC_Audio_Class DacAudio(AUDIO_PIN,0);         //Use GPIO 25, one of the 2 DAC pins and timer 0

/////////////////////////////////////////////////

void setup() {

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), ultrasonicEcho, CHANGE);
  
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
  Serial.println(WiFi.localIP());

  server.on("/someEndpoint", HTTP_GET, []() {
    address = server.arg("address").toInt();
    value = server.arg("value").toFloat();

    if(address == 1){
        SIT_TIME = value * 1000;
        EEPROM.put(0, SIT_TIME);
        EEPROM.commit();
        sitting_count = millis();
        server.send(200, "text/plain", "Signal received by ESP32");
    }else if(address == 2){
        seat_compare_threshold = Diff_pressure + 5.0;
        EEPROM.put(sizeof(uint64_t), seat_compare_threshold);
        EEPROM.commit();
        server.send(200, "text/plain", "Signal received by ESP32");
    }else if(address == 3){
        server.send(200, "text/plain", String(alert));
    }
  });

  server.begin();
 
#endif

  EEPROM.begin(64);
//  EEPROM.put(0, SIT_TIME);
//  EEPROM.put(sizeof(uint64_t), seat_compare_threshold);
//  EEPROM.put(sizeof(uint64_t) + sizeof(float), seat_width);
//  EEPROM.commit();
  EEPROM.get(0, SIT_TIME);
  EEPROM.get(sizeof(uint64_t), seat_compare_threshold);
  EEPROM.get(sizeof(uint64_t) + sizeof(float), seat_width);

  Serial.println("\nsetup done, start running in ");
  for(int i = 5; i > 0; i--){
    Serial.print(i);
    Serial.print("s ");
    delay(1000);
  }

  print_timer = timerBegin(1, 80, true);                          //timer 1, 80 prescaler, count up
  timerAttachInterrupt(print_timer, &print_timer_callback, true); //attach callback function
  timerAlarmWrite(print_timer, 500000, true);                     //500000 us, generated periodically
  timerAlarmEnable(print_timer);                                  //enable the interrupt

  blink_timer = timerBegin(2, 80, true);
  timerAttachInterrupt(blink_timer, &blink_timer_callback, true);
  timerAlarmWrite(blink_timer, 20000, true);
  timerAlarmEnable(blink_timer);

  back_count = millis();
  front_count = millis();
  side_count = millis();
}

void loop() {
#ifdef WIFI_EN
  server.handleClient();
#endif
  DacAudio.FillBuffer();
  pressure();
  back_support();
  distance();
  check();

  sit_time = (millis() - sitting_count) / 1000;
  leave_time = (millis() - leave_count) / 1000;

  if(millis() - flash_count >= 60 * 1000 && start_flag){
    flash_count = millis();

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
  pressure3_data = analogRead(PRESSURE3_PIN);
  pressure4_data = analogRead(PRESSURE4_PIN);

  left_pressure = pressure1_data + pressure4_data;
  right_pressure = pressure2_data + pressure3_data;

  start_flag = ((left_pressure + right_pressure) < seat_threshold);
  Diff_pressure = abs(left_pressure - right_pressure) / max(left_pressure, right_pressure) * 100.0;
  if(Diff_pressure < seat_compare_threshold && start_flag)  side_count = millis();
}

void back_support (void){

  back_touched = (start_flag && (pressure3_data < back_threshold || pressure4_data < back_threshold));
  if(back_touched && start_flag) back_count = millis();
}

float get_dist (int trig_pin, int echo_pin){
  digitalWrite(trig_pin, 0);
  delay(2);
  digitalWrite(trig_pin, 1);
  delay(10);
  digitalWrite(trig_pin, 0);
  if(echoFallen){
    echoFallen = false;
    return (SOUND_SPEED * ((echoEnd - echoStart) / 2));
  }else
    return min_dist;
}

void distance (void){
  if(start_flag == 0){
    dist = 0.0;
    return;
  }

  dist = get_dist(TRIG_PIN, ECHO_PIN);
  if(dist > seat_width)  dist = seat_width;
  if(dist <= dist_compare_threshold) front_count = millis();
}

void output_sound(void){
  if(Sound_Front.Playing==false && alert == 1)
    DacAudio.Play(&Sound_Front);   
  else if(Sound_Back.Playing==false && alert == 2)
    DacAudio.Play(&Sound_Back);
  else if(Sound_Side.Playing==false && alert == 3)
    DacAudio.Play(&Sound_Side);
  else if(Sound_Time.Playing==false && alert == 4)
    DacAudio.Play(&Sound_Time);
}

void output_device(int vibrator_en, int RLED_en, int GLED_en, int BLED_en){
  static uint32_t vibrator_count = 0;
  
  ledR_PWM = RLED_en? ledR_PWM + 5 : 0;
  ledG_PWM = GLED_en && RLED_en? ledG_PWM + 2 :
             GLED_en? ledG_PWM + 2 : 0;
  ledB_PWM = BLED_en? ledB_PWM + 5 : 0;
  if(RLED_en && GLED_en && ledR_PWM >= 250){
    ledR_PWM = 5;
    ledG_PWM = 5;
  }
  if(ledR_PWM >= 250) ledR_PWM = 5;
  if(ledG_PWM >= 250) ledG_PWM = 5;
  if(ledB_PWM >= 250) ledB_PWM = 5;
  ledcWrite(LED_R_Ch, ledR_PWM);
  ledcWrite(LED_G_Ch, ledG_PWM);
  ledcWrite(LED_B_Ch, ledB_PWM);
  
  if(vibrator_en && (millis() - vibrator_count) < VIBRATE_TIME){
      digitalWrite(VIBRATOR_PIN, 1);
      output_sound();
  }else if(vibrator_en == 0){
      digitalWrite(VIBRATOR_PIN, 0);
      vibrator_count = millis();
  }else{
      digitalWrite(VIBRATOR_PIN, 0);
  }
}

void check (void){
  if(start_flag == 0){
    sitting_count = millis();
    alert = (millis() - leave_count >= STAND_TIME) ? 5 : 0;
  }else{
    leave_count = millis();
    if(millis() - sitting_count >= SIT_TIME)  //time
      alert = 4;
    else if(dist > dist_compare_threshold && (millis() - front_count >= 1 * 1000)) //front
      alert = 1;
    else if(back_touched == 0 && (millis() - back_count >= 3 * 1000))   //back
      alert = 2;
    else if(Diff_pressure > seat_compare_threshold && (millis() - side_count >= 3 * 1000))   //side
      alert = 3;
    else
      alert = 0;
  }
  
  switch(alert){
  case 0:   output_device(0, 0, 1, 0);  break;  //green
  case 1:   output_device(1, 1, 0, 0);  break;  //red
  case 2:   output_device(1, 1, 1, 0);  break;  //yellow
  case 3:   output_device(1, 0, 0, 1);  break;  //blue
  case 4:   output_device(1, 1, 0, 1);  break;  //purple
  case 5:   output_device(0, 0, 0, 0);  break;  //off
  }   
}

#ifdef WIFI_EN

void updateGoogleSheet(uint32_t value) {

  http.begin(googleAppsScriptURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData = "sittingTime=" + String(value);

  http.POST(postData);
  http.end();
}

#endif
