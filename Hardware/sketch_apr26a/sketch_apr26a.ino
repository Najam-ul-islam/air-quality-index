#include <ArduinoJson.h>
#include <LiquidCrystal.h>
#include <DHT.h>
#include <MQ135.h>
#include <math.h>

// Sensor Pins
#define DHTPIN 2
#define DHTTYPE DHT11
#define MQ135_PIN A0
#define DSM501A_PM1 9  // PM2.5 (Vout1)
#define DSM501A_PM2 10 // PM10 (Vout2)

// LCD Configuration (RS, EN, D4, D5, D6, D7)
const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

DHT dht(DHTPIN, DHTTYPE);
MQ135 mq135(MQ135_PIN);

// Dust Sensor Variables
unsigned long sampleStartTime;
unsigned long lowpulseoccupancy_pm1 = 0;
unsigned long lowpulseoccupancy_pm2 = 0;
const unsigned long SAMPLE_PERIOD = 30000; // 30 seconds
float pm25_concentration = 0;
float pm10_concentration = 0;

// Timing Variables
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 2000;

// Calibration values
float Ro = 10.0; // Calibrate this in clean air!
const float CO_CURVE[3] = {10.0, -2.0, 1.0}; // CO calibration: a * (Rs/Ro)^b + c

// Cache last values in case DHT fails
float lastTemp = 25.0; // Initialized to room temp to avoid NaN on first read
float lastHum = 50.0;  // Initialized to a reasonable humidity

// Variables for non-blocking dust sensor pulse measurement
bool pm1_low_state = false;
unsigned long pm1_low_start = 0;

bool pm2_low_state = false;
unsigned long pm2_low_start = 0;

// Forward declarations
void readDustSensors();
float getCorrectedCO(float temp, float hum, float Ro);
float estimateNH3(float rs_ro_ratio);
int calculateAQI(float pm25);
void sendSensorData(float temp, float hum, float co, float nh3, bool dhtError);
void updateLCD(float temp, float hum, float aqi, float nh3, float co);

void setup() {
  Serial.begin(115200);
  dht.begin();
  lcd.begin(16, 4);

  pinMode(DSM501A_PM1, INPUT);
  pinMode(DSM501A_PM2, INPUT);

  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();

  // Warm-up sensors (DSM501A needs ~1 minute)
  lcd.print("Warming sensors");
  for (int i = 30; i > 0; i--) {
    lcd.setCursor(0, 1);
    lcd.print(i);
    lcd.print(" seconds    ");
    delay(1000);
  }

  // Calibrate MQ135 (optional: run in clean air)
  Ro = mq135.getResistance() / 3.6; // Default for CO2
  Serial.print("Calibrated Ro: ");
  Serial.println(Ro);

  lcd.clear();
  sampleStartTime = millis();
}

// Non-blocking dust sensor pulse measurement (call frequently in loop)
void measureDustPulse() {
  unsigned long now = millis();

  // PM1 sensor
  bool current_pm1_state = digitalRead(DSM501A_PM1) == LOW;
  if (current_pm1_state && !pm1_low_state) {
    // LOW pulse started
    pm1_low_start = now;
    pm1_low_state = true;
  } else if (!current_pm1_state && pm1_low_state) {
    // LOW pulse ended
    lowpulseoccupancy_pm1 += now - pm1_low_start;
    pm1_low_state = false;
  }

  // PM2 sensor
  bool current_pm2_state = digitalRead(DSM501A_PM2) == LOW;
  if (current_pm2_state && !pm2_low_state) {
    pm2_low_start = now;
    pm2_low_state = true;
  } else if (!current_pm2_state && pm2_low_state) {
    lowpulseoccupancy_pm2 += now - pm2_low_start;
    pm2_low_state = false;
  }

  // If LOW state is still ongoing at the end of the sample period, add elapsed time
  if ((now - sampleStartTime) >= SAMPLE_PERIOD) {
    // Handle ongoing LOW pulses
    if (pm1_low_state) {
      lowpulseoccupancy_pm1 += now - pm1_low_start;
      pm1_low_start = now; // reset start time for next period
    }
    if (pm2_low_state) {
      lowpulseoccupancy_pm2 += now - pm2_low_start;
      pm2_low_start = now;
    }

    // Calculate concentrations
    float ratio_pm1 = lowpulseoccupancy_pm1 / (SAMPLE_PERIOD * 10.0);
    float ratio_pm2 = lowpulseoccupancy_pm2 / (SAMPLE_PERIOD * 10.0);

    pm25_concentration = 1.1 * pow(ratio_pm1, 3) - 3.8 * pow(ratio_pm1, 2) + 520 * ratio_pm1 + 0.62;
    pm10_concentration = 1.1 * pow(ratio_pm2, 3) - 3.8 * pow(ratio_pm2, 2) + 520 * ratio_pm2 + 0.62;

    // Reset counters and timer for next sampling period
    lowpulseoccupancy_pm1 = 0;
    lowpulseoccupancy_pm2 = 0;
    sampleStartTime = now;
  }
}

void loop() {
  measureDustPulse();

  // DHT11 with retry logic every DHT_INTERVAL
  if (millis() - lastDHTRead >= DHT_INTERVAL) {
    lastDHTRead = millis();

    bool dhtError = false;
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    if (isnan(temp) || isnan(hum)) {
      dhtError = true;
      int retries = 0;
      while ((isnan(temp) || isnan(hum)) && (retries++ < 3)) {
        delay(100);
        temp = dht.readTemperature();
        hum = dht.readHumidity();
      }
      if (isnan(temp)) temp = lastTemp;
      if (isnan(hum)) hum = lastHum;
    } else {
      lastTemp = temp;
      lastHum = hum;
    }

    // Gas calculations
    float co = getCorrectedCO(temp, hum, Ro);
    float nh3 = estimateNH3(mq135.getResistance() / Ro);

    // Update LCD
    updateLCD(temp, hum, calculateAQI(pm25_concentration), nh3, co);

    // Send data every SEND_INTERVAL
    if (millis() - lastSendTime >= SEND_INTERVAL) {
      sendSensorData(temp, hum, co, nh3, dhtError);
      lastSendTime = millis();
    }
  }

  delay(10); // Small delay to stabilize loop
}

// Custom gas concentration calculations
float getCorrectedCO(float temp, float hum, float Ro) {
  float ratio = mq135.getResistance() / Ro;
  return CO_CURVE[0] * pow(ratio, CO_CURVE[1]) + CO_CURVE[2]; // CO ppm
}

float estimateNH3(float rs_ro_ratio) {
  return 2.5 * pow(rs_ro_ratio, -2.8); // NH3 ppm
}

// Rollover-safe AQI calculation
int calculateAQI(float pm25) {
  const float breaks[] = {0, 12.1, 35.5, 55.5, 150.5, 250.5, 500.5};
  const int aqi[] = {0, 50, 100, 150, 200, 300, 500};
  for (int i = 1; i < 7; i++) {
    if (pm25 <= breaks[i])
      return map(pm25 * 100, breaks[i - 1] * 100, breaks[i] * 100, aqi[i - 1], aqi[i]);
  }
  return 500;
}

void sendSensorData(float temp, float hum, float co, float nh3, bool dhtError) {
  StaticJsonDocument<512> doc;
  doc["temp"] = temp;
  doc["hum"] = hum;
  doc["PM25"] = pm25_concentration;
  doc["PM10"] = pm10_concentration;
  doc["CO"] = co;
  doc["NH3"] = nh3;
  doc["AQI"] = calculateAQI(pm25_concentration);
  doc["dht_error"] = dhtError;

  serializeJson(doc, Serial);
  Serial.println();
}

// Non-flickering LCD updates
void updateLCD(float temp, float hum, float aqi, float nh3, float co) {
  // Line 0: Temperature and Humidity
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.write(223); // Degree symbol
  lcd.print("C H:");
  lcd.print(hum, 0);
  lcd.print("%   ");

  // Line 1: PM2.5 and PM10
  lcd.setCursor(0, 1);
  lcd.print("PM2.5:");
  lcd.print(pm25_concentration, 1);
  lcd.print(" PM10:");
  lcd.print(pm10_concentration, 1);
  lcd.print(" ");

  // Line 2: AQI and NH3
  lcd.setCursor(0, 2);
  lcd.print("AQI:");
  lcd.print(aqi);
  lcd.print(" NH3:");
  lcd.print(nh3, 1);
  lcd.print("ppm ");

  // Line 3: CO
  lcd.setCursor(0, 3);
  lcd.print("CO:");
  lcd.print(co, 1);
  lcd.print("ppm       ");
}

// ===========================================================
// #include <ArduinoJson.h>
// #include <LiquidCrystal.h>
// #include <DHT.h>
// #include <MQ135.h>
// #include <math.h>

// // Sensor Pins
// #define DHTPIN 2
// #define DHTTYPE DHT11
// #define MQ135_PIN A0
// #define DSM501A_PM1 9  // PM2.5 (Vout1)
// #define DSM501A_PM2 10 // PM10 (Vout2)

// // LCD Configuration (RS, EN, D4, D5, D6, D7)
// const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 8;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// DHT dht(DHTPIN, DHTTYPE);
// MQ135 mq135(MQ135_PIN);

// // Dust Sensor Variables
// unsigned long sampleStartTime;
// unsigned long lowpulseoccupancy_pm1 = 0;
// unsigned long lowpulseoccupancy_pm2 = 0;
// const unsigned long SAMPLE_PERIOD = 30000; // 30 seconds
// float pm25_concentration = 0;
// float pm10_concentration = 0;

// // Timing Variables
// unsigned long lastDHTRead = 0;
// const unsigned long DHT_INTERVAL = 2000;
// unsigned long lastSendTime = 0;
// const unsigned long SEND_INTERVAL = 2000;

// // Calibration values
// float Ro = 10.0; // Calibrate this in clean air!
// const float CO_CURVE[3] = {10.0, -2.0, 1.0}; // CO calibration: a * (Rs/Ro)^b + c

// // Cache last values in case DHT fails
// float lastTemp = -1;
// float lastHum = -1;

// void setup() {
//   Serial.begin(115200);
//   dht.begin();
//   lcd.begin(16, 4);

//   pinMode(DSM501A_PM1, INPUT);
//   pinMode(DSM501A_PM2, INPUT);

//   lcd.setCursor(0, 0);
//   lcd.print("Initializing...");
//   delay(2000);
//   lcd.clear();

//   // Warm-up sensors (DSM501A needs ~1 minute)
//   lcd.print("Warming sensors");
//   for (int i = 30; i > 0; i--) {
//     lcd.setCursor(0, 1);
//     lcd.print(i);
//     lcd.print(" seconds    ");
//     delay(1000);
//   }

//   // Calibrate MQ135 (optional: run in clean air)
//   Ro = mq135.getResistance() / 3.6; // Default for CO2
//   Serial.print("Calibrated Ro: ");
//   Serial.println(Ro);

//   lcd.clear();
//   sampleStartTime = millis();
// }

// // Accurate PWM measurement for DSM501A
// void readDustSensors() {
//   lowpulseoccupancy_pm1 += pulseIn(DSM501A_PM1, LOW);
//   lowpulseoccupancy_pm2 += pulseIn(DSM501A_PM2, LOW);

//   // Rollover-safe 30s check
//   if ((millis() - sampleStartTime) >= SAMPLE_PERIOD) {
//     float ratio_pm1 = lowpulseoccupancy_pm1 / (SAMPLE_PERIOD * 10.0);
//     float ratio_pm2 = lowpulseoccupancy_pm2 / (SAMPLE_PERIOD * 10.0);

//     pm25_concentration = 1.1 * pow(ratio_pm1, 3) - 3.8 * pow(ratio_pm1, 2) + 520 * ratio_pm1 + 0.62;
//     pm10_concentration = 1.1 * pow(ratio_pm2, 3) - 3.8 * pow(ratio_pm2, 2) + 520 * ratio_pm2 + 0.62;

//     lowpulseoccupancy_pm1 = 0;
//     lowpulseoccupancy_pm2 = 0;
//     sampleStartTime = millis();
//   }
// }

// // Custom gas concentration calculations
// float getCorrectedCO(float temp, float hum, float Ro) {
//   float ratio = mq135.getResistance() / Ro;
//   return CO_CURVE[0] * pow(ratio, CO_CURVE[1]) + CO_CURVE[2]; // CO ppm
// }

// float estimateNH3(float rs_ro_ratio) {
//   return 2.5 * pow(rs_ro_ratio, -2.8); // NH3 ppm
// }

// // Rollover-safe AQI calculation
// int calculateAQI(float pm25) {
//   const float breaks[] = {0, 12.1, 35.5, 55.5, 150.5, 250.5, 500.5};
//   const int aqi[] = {0, 50, 100, 150, 200, 300, 500};
//   for (int i = 1; i < 7; i++) {
//     if (pm25 <= breaks[i]) 
//       return map(pm25 * 100, breaks[i-1] * 100, breaks[i] * 100, aqi[i-1], aqi[i]);
//   }
//   return 500;
// }

// void sendSensorData(float temp, float hum, float co, float nh3, bool dhtError) {
//   StaticJsonDocument<512> doc;
//   doc["temp"] = temp;
//   doc["hum"] = hum;
//   doc["PM25"] = pm25_concentration;
//   doc["PM10"] = pm10_concentration;
//   doc["CO"] = co;
//   doc["NH3"] = nh3;
//   doc["AQI"] = calculateAQI(pm25_concentration);
//   doc["dht_error"] = dhtError;

//   serializeJson(doc, Serial);
//   Serial.println();
// }

// // Non-flickering LCD updates
// void updateLCD(float temp, float hum, float aqi, float nh3, float co) {
//   // Line 0: Temperature and Humidity
//   lcd.setCursor(0, 0);
//   lcd.print("T:");
//   lcd.print(temp, 1);
//   lcd.write(223); // Degree symbol
//   lcd.print("C H:");
//   lcd.print(hum, 0);
//   lcd.print("%   ");

//   // Line 1: PM2.5 and PM10
//   lcd.setCursor(0, 1);
//   lcd.print("PM2.5:");
//   lcd.print(pm25_concentration, 1);
//   lcd.print(" PM10:");
//   lcd.print(pm10_concentration, 1);

//   // Line 2: CO and NH3
//   lcd.setCursor(0, 2);
//   lcd.print("CO:");
//   lcd.print(co, 1);
//   lcd.print(" NH3:");
//   lcd.print(nh3, 1);
//   lcd.print("    ");

//   // Line 3: AQI status
//   lcd.setCursor(0, 3);
//   lcd.print("AQI:");
//   lcd.print((int)aqi);
//   lcd.print(" ");
//   if (aqi <= 50) lcd.print("Good      ");
//   else if (aqi <= 100) lcd.print("Moderate  ");
//   else if (aqi <= 150) lcd.print("Unhealthy ");
//   else if (aqi <= 200) lcd.print("Very Unhlthy");
//   else lcd.print("Hazardous ");
// }

// void loop() {
//   readDustSensors();

//   // DHT11 with retry logic
//   bool dhtError = false;
//   float temp = dht.readTemperature();
//   float hum = dht.readHumidity();
  
//   if (isnan(temp) || isnan(hum)) {
//     dhtError = true;
//     int retries = 0;
//     while ((isnan(temp) || isnan(hum)) && (retries++ < 3)) {
//       delay(100);
//       temp = dht.readTemperature();
//       hum = dht.readHumidity();
//     }
//     if (isnan(temp)) temp = lastTemp;
//     if (isnan(hum)) hum = lastHum;
//   } else {
//     lastTemp = temp;
//     lastHum = hum;
//   }

//   // Gas calculations
//   float co = getCorrectedCO(temp, hum, Ro);
//   float nh3 = estimateNH3(mq135.getResistance() / Ro);

//   // Update displays
//   updateLCD(temp, hum, calculateAQI(pm25_concentration), nh3, co);

//   // Send data every 2 seconds
//   if (millis() - lastSendTime >= SEND_INTERVAL) {
//     sendSensorData(temp, hum, co, nh3, dhtError);
//     lastSendTime = millis();
//   }

//   delay(100); // Small delay to stabilize loop
// }
// ================================================================
// #include <ArduinoJson.h>
// #include <LiquidCrystal.h>
// #include <DHT.h>
// #include <MQ135.h>
// #include <math.h>

// // Sensor Pins
// #define DHTPIN 2
// #define DHTTYPE DHT11
// #define MQ135_PIN A0
// #define DSM501A_PM1 9
// #define DSM501A_PM2 10

// // LCD Configuration (RS, EN, D4, D5, D6, D7)
// const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 8;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// DHT dht(DHTPIN, DHTTYPE);
// MQ135 mq135(MQ135_PIN);

// // Dust Sensor Variables
// unsigned long sampleStartTime;
// unsigned long lowpulseoccupancy_pm1 = 0;
// unsigned long lowpulseoccupancy_pm2 = 0;
// const unsigned long SAMPLE_PERIOD = 30000; // 30 seconds
// float pm25_concentration = 0;
// float pm10_concentration = 0;

// // Timing Variables
// unsigned long lastDHTRead = 0;
// const unsigned long DHT_INTERVAL = 2000;
// unsigned long lastSendTime = 0;
// const unsigned long SEND_INTERVAL = 2000;

// // Calibration value
// float Ro = 10.0; // Default. Calibrate this in clean air!

// // Cache last values in case DHT fails
// float lastTemp = -1;
// float lastHum = -1;

// void setup() {
//   Serial.begin(115200);
//   dht.begin();
//   lcd.begin(16, 4);

//   pinMode(DSM501A_PM1, INPUT);
//   pinMode(DSM501A_PM2, INPUT);

//   lcd.setCursor(0, 0);
//   lcd.print("Initializing...");
//   delay(2000);
//   lcd.clear();

//   lcd.print("Warming sensors");
//   for (int i = 0; i < 30; i++) {
//     lcd.setCursor(0, 1);
//     lcd.print(30 - i);
//     lcd.print(" seconds    ");
//     delay(1000);
//   }

//   // Calibrate Ro (optional, update with clean air value)
//   float rs = mq135.getResistance();
//   Ro = rs / 3.6;
//   Serial.print("Calibrated Ro: ");
//   Serial.println(Ro);

//   lcd.clear();
//   sampleStartTime = millis();
// }

// void readDustSensors() {
//   if (digitalRead(DSM501A_PM1) == LOW)
//     lowpulseoccupancy_pm1 += 100;
//   if (digitalRead(DSM501A_PM2) == LOW)
//     lowpulseoccupancy_pm2 += 100;

//   if ((millis() - sampleStartTime) > SAMPLE_PERIOD) {
//     float ratio_pm1 = lowpulseoccupancy_pm1 / (SAMPLE_PERIOD * 10.0);
//     float ratio_pm2 = lowpulseoccupancy_pm2 / (SAMPLE_PERIOD * 10.0);

//     pm25_concentration = (1.1 * pow(ratio_pm1, 3) - 3.8 * pow(ratio_pm1, 2) + 520 * ratio_pm1 + 0.62);
//     pm10_concentration = (1.1 * pow(ratio_pm2, 3) - 3.8 * pow(ratio_pm2, 2) + 520 * ratio_pm2 + 0.62);

//     lowpulseoccupancy_pm1 = 0;
//     lowpulseoccupancy_pm2 = 0;
//     sampleStartTime = millis();
//   }
// }

// float estimateNH3(float rs_ro_ratio) {
//   float a = 2.5;
//   float b = -2.8;
//   return a * pow(rs_ro_ratio, b); // ppm estimate for NH3
// }

// void sendSensorData(float temp, float hum, float co, float nh3) {
//   StaticJsonDocument<256> doc;
//   doc["temp"] = temp;
//   doc["hum"] = hum;
//   doc["PM25"] = pm25_concentration;
//   doc["PM10"] = pm10_concentration;
//   doc["CO"] = co;
//   doc["NH3"] = nh3;
//   doc["AQI"] = calculateAQI(pm25_concentration);

//   serializeJson(doc, Serial);
//   Serial.println();
// }

// int calculateAQI(float pm25) {
//   if (pm25 <= 12.0) return round((pm25 / 12.0) * 50);
//   else if (pm25 <= 35.4) return round(((pm25 - 12.1) / (35.4 - 12.1)) * 49 + 51);
//   else if (pm25 <= 55.4) return round(((pm25 - 35.5) / (55.4 - 35.5)) * 49 + 101);
//   else if (pm25 <= 150.4) return round(((pm25 - 55.5) / (150.4 - 55.5)) * 49 + 151);
//   else if (pm25 <= 250.4) return round(((pm25 - 150.5) / (250.4 - 150.5)) * 99 + 201);
//   else if (pm25 <= 500.4) return round(((pm25 - 250.5) / (500.4 - 250.5)) * 199 + 301);
//   else return 500;
// }
// void clearLine(int line) {
//   lcd.setCursor(0, line);
//   lcd.print("                "); // 16 spaces
// }

// void updateLCD(float temp, float hum, float aqi, float nh3, float co) {
//   lcd.clear();

//   // Line 0: Temperature and Humidity
//   lcd.setCursor(0, 0);
//   lcd.print("T:");
//   lcd.print(temp, 1);
//   lcd.print((char)223);  // Degree symbol
//   lcd.print("C H:");
//   lcd.print(hum, 0);
//   lcd.print("%");

//   // Line 1: PM2.5 and PM10
//   lcd.setCursor(0, 1);
//   lcd.print("PM2.5:");
//   lcd.print(pm25_concentration, 1);
//   lcd.print(" PM10:");
//   lcd.print(pm10_concentration, 1);

//   // Line 2: CO and NH3
//   lcd.setCursor(0, 2);
//   lcd.print("CO:");
//   lcd.print(co, 1);
//   lcd.print(" NH3:");
//   lcd.print(nh3, 1);

//   // Line 3: AQI status
//   lcd.setCursor(0, 3);
//   lcd.print("AQI: ");
//   lcd.print((int)aqi);
//   lcd.print(" ");

//   // Add AQI category text
//   if (aqi <= 50) {
//     lcd.print("Good");
//   } else if (aqi <= 100) {
//     lcd.print("Moderate");
//   } else if (aqi <= 150) {
//     lcd.print("Unhealthy");
//   } else if (aqi <= 200) {
//     lcd.print("UnhlthySens");
//   } else if (aqi <= 300) {
//     lcd.print("V.Unhealthy");
//   } else {
//     lcd.print("Hazardous");
//   }
// }

// void loop() {
//   delay(2000);
//   readDustSensors();

//   bool dhtSuccess = false;
//   float temp = dht.readTemperature();
//   float hum = dht.readHumidity();

//   if (!isnan(temp) && !isnan(hum)) {
//     lastTemp = temp;
//     lastHum = hum;
//     dhtSuccess = true;
//   } else {
//     Serial.println("{\"error\":\"DHT read failed\"}");
//     temp = lastTemp;
//     hum = lastHum;
//   }

//   float correctedCO = mq135.getCorrectedPPM(temp, hum);
//   float rs = mq135.getResistance();
//   float rs_ro_ratio = rs / Ro;
//   float nh3_ppm = estimateNH3(rs_ro_ratio);
//   int aqi = calculateAQI(pm25_concentration);

//   // updateLCD(temp, hum, aqi, nh3_ppm, dhtSuccess);
//   updateLCD(temp, hum, aqi, nh3_ppm, correctedCO);


//   if (millis() - lastSendTime >= SEND_INTERVAL) {
//     sendSensorData(temp, hum, correctedCO, nh3_ppm);
//     lastSendTime = millis();
//   }

//   lastDHTRead = millis();
// }

// ================================================================
// #include <ArduinoJson.h>
// #include <LiquidCrystal.h>
// #include <DHT.h>
// #include <MQ135.h>
// #include <math.h>

// // Sensor Pins
// #define DHTPIN 2
// #define DHTTYPE DHT11
// #define MQ135_PIN A0
// #define DSM501A_PM1 9
// #define DSM501A_PM2 10

// // LCD Configuration (RS, EN, D4, D5, D6, D7)
// const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 8;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// DHT dht(DHTPIN, DHTTYPE);
// MQ135 mq135(MQ135_PIN);

// // Dust Sensor Variables
// unsigned long sampleStartTime;
// unsigned long lowpulseoccupancy_pm1 = 0;
// unsigned long lowpulseoccupancy_pm2 = 0;
// const unsigned long SAMPLE_PERIOD = 30000; // 30 seconds
// float pm25_concentration = 0;
// float pm10_concentration = 0;

// // Timing Variables
// unsigned long lastDHTRead = 0;
// const unsigned long DHT_INTERVAL = 2000;
// unsigned long lastSendTime = 0;
// const unsigned long SEND_INTERVAL = 2000;

// // Calibration value
// float Ro = 10.0; // Default. Calibrate this in clean air!

// void setup() {
//   Serial.begin(115200);
//   dht.begin();
//   lcd.begin(16, 4);

//   pinMode(DSM501A_PM1, INPUT);
//   pinMode(DSM501A_PM2, INPUT);

//   lcd.setCursor(0, 0);
//   lcd.print("Initializing...");
//   delay(2000);
//   lcd.clear();

//   lcd.print("Warming sensors");
//   for (int i = 0; i < 30; i++) {
//     lcd.setCursor(0, 1);
//     lcd.print(30 - i);
//     lcd.print(" seconds    ");
//     delay(1000);
//   }

//   // Calibrate Ro (optional, update with clean air value)
//   float rs = mq135.getResistance();
//   Ro = rs / 3.6;
//   Serial.print("Calibrated Ro: ");
//   Serial.println(Ro);

//   lcd.clear();
//   sampleStartTime = millis();
// }

// void readDustSensors() {
//   // Use digitalRead instead of blocking pulseIn
//   if (digitalRead(DSM501A_PM1) == LOW)
//     lowpulseoccupancy_pm1 += 100; // Approximate value
//   if (digitalRead(DSM501A_PM2) == LOW)
//     lowpulseoccupancy_pm2 += 100;

//   if ((millis() - sampleStartTime) > SAMPLE_PERIOD) {
//     float ratio_pm1 = lowpulseoccupancy_pm1 / (SAMPLE_PERIOD * 10.0);
//     float ratio_pm2 = lowpulseoccupancy_pm2 / (SAMPLE_PERIOD * 10.0);

//     pm25_concentration = (1.1 * pow(ratio_pm1, 3) - 3.8 * pow(ratio_pm1, 2) + 520 * ratio_pm1 + 0.62);
//     pm10_concentration = (1.1 * pow(ratio_pm2, 3) - 3.8 * pow(ratio_pm2, 2) + 520 * ratio_pm2 + 0.62);

//     lowpulseoccupancy_pm1 = 0;
//     lowpulseoccupancy_pm2 = 0;
//     sampleStartTime = millis();
//   }
// }

// float estimateNH3(float rs_ro_ratio) {
//   float a = 2.5;
//   float b = -2.8;
//   return a * pow(rs_ro_ratio, b); // ppm estimate for NH3
// }

// void sendSensorData(float temp, float hum, float co, float nh3) {
//   StaticJsonDocument<256> doc;
//   doc["temp"] = temp;
//   doc["hum"] = hum;
//   doc["PM25"] = pm25_concentration;
//   doc["PM10"] = pm10_concentration;
//   doc["CO"] = co;
//   doc["NH3"] = nh3;
//   doc["AQI"] = calculateAQI(pm25_concentration);

//   serializeJson(doc, Serial);
//   Serial.println();
// }

// int calculateAQI(float pm25) {
//   if (pm25 <= 12.0) return round((pm25 / 12.0) * 50);
//   else if (pm25 <= 35.4) return round(((pm25 - 12.1) / (35.4 - 12.1)) * 49 + 51);
//   else if (pm25 <= 55.4) return round(((pm25 - 35.5) / (55.4 - 35.5)) * 49 + 101);
//   else if (pm25 <= 150.4) return round(((pm25 - 55.5) / (150.4 - 55.5)) * 49 + 151);
//   else if (pm25 <= 250.4) return round(((pm25 - 150.5) / (250.4 - 150.5)) * 99 + 201);
//   else if (pm25 <= 500.4) return round(((pm25 - 250.5) / (500.4 - 250.5)) * 199 + 301);
//   else return 500;
// }

// void updateLCD(float temp, float hum, float aqi, float nh3) {
//   lcd.setCursor(0, 0);
//   lcd.print("T:");
//   lcd.print(temp, 1);
//   lcd.print("C H:");
//   lcd.print(hum, 0);
//   lcd.print("%   ");

//   lcd.setCursor(0, 1);
//   lcd.print("PM2.5:");
//   lcd.print(pm25_concentration, 1);
//   lcd.print(" AQI:");
//   lcd.print(aqi);
//   lcd.print("   ");

//   lcd.setCursor(0, 2);
//   lcd.print("NH3:");
//   lcd.print(nh3, 1);
//   lcd.print("ppm    ");
// }

// void loop() {
//   delay(2000);
//   readDustSensors();

//   if (millis() - lastDHTRead >= DHT_INTERVAL) {
//     float temp = dht.readTemperature();
//     float hum = dht.readHumidity();

//     if (isnan(temp) || isnan(hum)) {
//       Serial.println("{\"error\":\"DHT read failed\"}");
//     } else {
//       float correctedCO = mq135.getCorrectedPPM(temp, hum);
//       float rs = mq135.getResistance();
//       float rs_ro_ratio = rs / Ro;
//       float nh3_ppm = estimateNH3(rs_ro_ratio);
//       int aqi = calculateAQI(pm25_concentration);

//       updateLCD(temp, hum, aqi, nh3_ppm);

//       if (millis() - lastSendTime >= SEND_INTERVAL) {
//         sendSensorData(temp, hum, correctedCO, nh3_ppm);
//         lastSendTime = millis();
//       }
//     }

//     lastDHTRead = millis();
//   }
// }


// =================================================================
// #include <ArduinoJson.h>
// #include <LiquidCrystal.h>
// #include <DHT.h>
// #include <MQ135.h>
// #include <math.h>

// // Sensor Pins
// #define DHTPIN 2
// #define DHTTYPE DHT11
// #define MQ135_PIN A0
// #define DSM501A_PM1 9
// #define DSM501A_PM2 10

// // LCD Configuration (RS, EN, D4, D5, D6, D7)
// const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 8;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// DHT dht(DHTPIN, DHTTYPE);
// MQ135 mq135(MQ135_PIN);

// // Dust Sensor Variables
// unsigned long sampleStartTime;
// unsigned long lowpulseoccupancy_pm1 = 0;
// unsigned long lowpulseoccupancy_pm2 = 0;
// const unsigned long SAMPLE_PERIOD = 30000; // 30 seconds
// float pm25_concentration = 0;
// float pm10_concentration = 0;

// // Timing Variables
// unsigned long lastDHTRead = 0;
// const unsigned long DHT_INTERVAL = 2000;
// unsigned long lastSendTime = 0;
// const unsigned long SEND_INTERVAL = 2000;

// // Calibration value
// float Ro = 10.0; // Default. calibrate this in clean air!

// void setup() {
//   Serial.begin(9600);
//   dht.begin();
//   lcd.begin(16, 4);

//   pinMode(DSM501A_PM1, INPUT);
//   pinMode(DSM501A_PM2, INPUT);
  
//   lcd.setCursor(0, 0);
//   lcd.print("Initializing...");
//   delay(2000);
//   lcd.clear();

//   // Serial.println("Warming sensor!!!");
//   lcd.print("Warming sensors");
//   for (int i = 0; i < 30; i++) {
//     lcd.setCursor(0, 1);
//     lcd.print(30 - i);
//     // Serial.print(30 - i);
//     // Serial.println(" seconds    ");
//     lcd.print(" seconds    ");
//     delay(1000);
//   }

//   // Calibrate Ro (optional, update with measured clean air value)
//   float rs = mq135.getResistance();
//   Ro = rs / 3.6;
//   Serial.print("Calibrated Ro: ");
//   Serial.println(Ro);

//   lcd.clear();
//   sampleStartTime = millis();
// }

// void readDustSensors() {
//   lowpulseoccupancy_pm1 += pulseIn(DSM501A_PM1, LOW, 100000);
//   lowpulseoccupancy_pm2 += pulseIn(DSM501A_PM2, LOW, 100000);

//   if ((millis() - sampleStartTime) > SAMPLE_PERIOD) {
//     float ratio_pm1 = lowpulseoccupancy_pm1 / (SAMPLE_PERIOD * 1000.0);
//     float ratio_pm2 = lowpulseoccupancy_pm2 / (SAMPLE_PERIOD * 1000.0);

//     pm25_concentration = (1.1 * pow(ratio_pm1, 3) - 3.8 * pow(ratio_pm1, 2) + 520 * ratio_pm1 + 0.62);
//     pm10_concentration = (1.1 * pow(ratio_pm2, 3) - 3.8 * pow(ratio_pm2, 2) + 520 * ratio_pm2 + 0.62);

//     lowpulseoccupancy_pm1 = 0;
//     lowpulseoccupancy_pm2 = 0;
//     sampleStartTime = millis();
//   }
// }

// float estimateNH3(float rs_ro_ratio) {
//   float a = 2.5;
//   float b = -2.8;
//   return a * pow(rs_ro_ratio, b); // ppm estimate for NH3
// }

// void sendSensorData(float temp, float hum, float co, float nh3) {
//   StaticJsonDocument<256> doc;
//   doc["temp"] = temp;
//   doc["hum"] = hum;
//   doc["PM25"] = pm25_concentration;
//   doc["PM10"] = pm10_concentration;
//   doc["CO"] = co;
//   doc["NH3"] = nh3;
//   doc["AQI"] = calculateAQI(pm25_concentration);

//   serializeJson(doc, Serial);
//   Serial.println();
// }

// int calculateAQI(float pm25) {
//   if (pm25 <= 12.0) return round((pm25 / 12.0) * 50);
//   else if (pm25 <= 35.4) return round(((pm25 - 12.1) / (35.4 - 12.1)) * 49 + 51);
//   else if (pm25 <= 55.4) return round(((pm25 - 35.5) / (55.4 - 35.5)) * 49 + 101);
//   else if (pm25 <= 150.4) return round(((pm25 - 55.5) / (150.4 - 55.5)) * 49 + 151);
//   else if (pm25 <= 250.4) return round(((pm25 - 150.5) / (250.4 - 150.5)) * 99 + 201);
//   else if (pm25 <= 500.4) return round(((pm25 - 250.5) / (500.4 - 250.5)) * 199 + 301);
//   else return 500;
// }

// void updateLCD(float temp, float hum, float aqi, float nh3) {
//   lcd.setCursor(0, 0);
//   lcd.print("T:");
//   lcd.print(temp, 1);
//   lcd.print("C H:");
//   lcd.print(hum, 0);
//   lcd.print("%   ");

//   lcd.setCursor(0, 1);
//   lcd.print("PM2.5:");
//   lcd.print(pm25_concentration, 1);
//   lcd.print(" AQI:");
//   lcd.print(aqi);
//   lcd.print(" ");

//   lcd.setCursor(0, 2);
//   lcd.print("NH3:");
//   lcd.print(nh3, 1);
//   lcd.print("ppm    ");
// }

// void loop() {
//   readDustSensors();

//   if (millis() - lastDHTRead >= DHT_INTERVAL) {
//     float temp = dht.readTemperature();
//     float hum = dht.readHumidity();

//     if (isnan(temp) || isnan(hum)) {
//       Serial.println("{\"error\":\"DHT read failed\"}");
//       return;
//     }

//     float correctedCO = mq135.getCorrectedPPM(temp, hum);
//     float rs = mq135.getResistance();
//     float rs_ro_ratio = rs / Ro;
//     float nh3_ppm = estimateNH3(rs_ro_ratio);
//     int aqi = calculateAQI(pm25_concentration);

//     updateLCD(temp, hum, aqi, nh3_ppm);

//     if (millis() - lastSendTime >= SEND_INTERVAL) {
//       sendSensorData(temp, hum, correctedCO, nh3_ppm);
//       lastSendTime = millis();
//     }

//     lastDHTRead = millis();
//   }
// }
