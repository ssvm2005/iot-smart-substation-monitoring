#define BLYNK_TEMPLATE_ID "TMPL3LK0Hb07I"
#define BLYNK_TEMPLATE_NAME "Smart Substation"
#define BLYNK_AUTH_TOKEN "XMYqS-eUOoOIYhNDJ_CwELZPR7DF94CP"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>

// WIFI
char ssid[] = "<enter your credentials>";
char pass[] = "<enter your password>";

// PIN CONFIG 
#define TRIG_PIN     33
#define ECHO_PIN     32
#define IR_PIN       35
#define CURRENT_PIN  34
#define RELAY_PIN    27
#define SERVO_PIN    13

Servo tapServo;
BlynkTimer timer;

// VARIABLES 
long duration;
float distance;
float oilPercent;
int irValue;

bool manualRelay = 0;
int servoPosition = 90;

// CALIBRATION 
float offset = 0;
float sensitivity = 0.185;

// THRESHOLDS 
#define CURRENT_TRIP 15.0
#define IR_THRESHOLD 120   // 🔥 NEW

// IR AVERAGING FUNCTION 
int getAverageIR() {
  int sum = 0;
  int samples = 10;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(IR_PIN);
    delay(5);
  }

  return sum / samples;
}

// CALIBRATION FUNCTION 
void calibrateOffset() {
  int sum = 0;

  for (int i = 0; i < 100; i++) {
    sum += analogRead(CURRENT_PIN);
    delay(5);
  }

  float voltage = (sum / 100.0) * (3.3 / 4095.0);
  offset = voltage;

  Serial.print("Calibrated Offset: ");
  Serial.println(offset);
}

//  ULTRASONIC 
float readOilLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) return oilPercent;

  distance = duration * 0.034 / 2.0;

  oilPercent = ((20.0 - distance) / 20.0) * 100.0;
  return constrain(oilPercent, 0, 100);
}

//  BLYNK INPUT 
BLYNK_WRITE(V3) {
  manualRelay = param.asInt();
}

BLYNK_WRITE(V4) {
  servoPosition = param.asInt();
}

// MAIN FUNCTION 
void sendData() {

  oilPercent = readOilLevel();
  irValue = getAverageIR();

  // CURRENT SENSOR --------
  int sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(CURRENT_PIN);
    delay(2);
  }

  float voltage = (sum / 20.0) * (3.3 / 4095.0);

  if (voltage < 0.1) voltage = offset;

  float current = (voltage - offset) / sensitivity;

  if (abs(current) < 0.1) current = 0;

  // -------- OIL QUALITY --------
  int oilQualityStatus;

  if (irValue > IR_THRESHOLD) {
    oilQualityStatus = 1; // DIRTY
  } else {
    oilQualityStatus = 0; // CLEAN
  }

  // -------- RELAY CONTROL --------
  bool relayON;

  if (current > CURRENT_TRIP) {
    relayON = false;
    manualRelay = 0;
    Blynk.virtualWrite(V3, 0);
  } else {
    relayON = manualRelay;
  }

  digitalWrite(RELAY_PIN, relayON ? LOW : HIGH);

  // -------- SERVO --------
  tapServo.write(servoPosition);

  // -------- BLYNK OUTPUT --------
  Blynk.virtualWrite(V0, oilPercent);
  Blynk.virtualWrite(V1, oilQualityStatus);
  Blynk.virtualWrite(V2, current);

  // -------- DEBUG --------
  Serial.println("------ DATA ------");
  Serial.print("IR Avg: "); Serial.println(irValue);
  Serial.print("Oil Quality: "); Serial.println(oilQualityStatus ? "DIRTY" : "CLEAN");
  Serial.print("Voltage: "); Serial.println(voltage);
  Serial.print("Current (A): "); Serial.println(current);
  Serial.print("Oil %: "); Serial.println(oilPercent);
  Serial.print("Distance: "); Serial.println(distance);
  Serial.print("Relay State: "); Serial.println(relayON);
  Serial.print("GPIO OUT: ");
  Serial.println(relayON ? "LOW (ON)" : "HIGH (OFF)");
  Serial.println("------------------\n");
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(CURRENT_PIN, INPUT);

  digitalWrite(RELAY_PIN, HIGH);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  tapServo.attach(SERVO_PIN);
  tapServo.write(servoPosition);

  delay(2000);

  calibrateOffset();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(2000L, sendData);
}

// ---------------- LOOP ----------------
void loop() {
  Blynk.run();
  timer.run();
}
