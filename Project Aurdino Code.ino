// Include necessary libraries
#define BLYNK_TEMPLATE_ID "TMPL3vAw5Pi69"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation System"
#define BLYNK_AUTH_TOKEN "JDbJe6G1V9PAaHzqPr4cKKuSD9oGZ3C4"

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

// Wi-Fi and Blynk credentials
char auth[] = "JDbJe6G1V9PAaHzqPr4cKKuSD9oGZ3C4";
char ssid[] = "HKB";
char pass[] = "22012006";

// Pin configuration
#define RELAY_PIN_1 23
#define SOIL_MOISTURE_PIN 34
#define PIR_SENSOR_OUTPUT_PIN 13
#define DHTPIN 26
#define DHTTYPE DHT11

// Virtual pins
#define VPIN_BUTTON_1 V12
#define VPIN_SOIL_MOISTURE V2
#define VPIN_AUTO_MODE V10
#define VPIN_TEMP V0
#define VPIN_HUM V1
#define VPIN_PIR_BUTTON V6
#define VPIN_PIR_LED V5

// Global variables
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

bool autoMode = false;
bool pumpState = false;
int pirState = LOW;
int warm_up = 0; // PIR warm-up state
unsigned long lastManualOffTime = 0;
const unsigned long manualOffDelay = 2 * 60 * 1000; // 2 minutes

// Function to update pump state
void updatePumpState(bool state) {
    pumpState = state;
    digitalWrite(RELAY_PIN_1, state ? LOW : HIGH); // Active-low relay
    Serial.println(state ? "🚰 Pump TURNED ON" : "🚫 Pump TURNED OFF");
}

// Blynk callbacks
BLYNK_WRITE(VPIN_AUTO_MODE) {
    autoMode = param.asInt();
    if (!autoMode) {
        updatePumpState(false); // Turn off pump in manual mode
        Serial.println("🔴 Auto Mode DISABLED - Manual Control Active");
    } else {
        Serial.println("🟢 Auto Mode ENABLED");
    }
}

BLYNK_WRITE(VPIN_BUTTON_1) {
    bool manualRequest = param.asInt();
    if (!autoMode) {
        updatePumpState(manualRequest);
    } else if (!manualRequest) {
        updatePumpState(false);
        lastManualOffTime = millis();
        Serial.println("🔴 Pump TURNED OFF Manually (Auto Mode)");
    } else {
        Serial.println("⚠️ Cannot manually turn ON pump in Auto Mode!");
    }
}

BLYNK_WRITE(VPIN_PIR_BUTTON) {
    pirState = param.asInt();
    if (pirState == 1) {
        Blynk.virtualWrite(VPIN_PIR_LED, HIGH); // Turn on PIR LED
        warm_up = 1; // Start PIR warm-up
    } else {
        Blynk.virtualWrite(VPIN_PIR_LED, LOW); // Turn off PIR LED
        warm_up = 0; // Stop PIR sensor
    }
}

// Sensor Functions
void checkSoilMoisture() {
    int soilMoisture = analogRead(SOIL_MOISTURE_PIN);
    int soilMoisturePercentage = map(soilMoisture, 3500, 4095, 100, 0);

    Serial.print("🌱 Soil Moisture: ");
    Serial.print(soilMoisturePercentage);
    Serial.println("%");

    Blynk.virtualWrite(VPIN_SOIL_MOISTURE, soilMoisturePercentage);

    if (autoMode) {
        if (soilMoisturePercentage < 12 && !pumpState && millis() - lastManualOffTime > manualOffDelay) {
            updatePumpState(true);
        } else if (soilMoisturePercentage >= 60 && pumpState) {
            updatePumpState(false);
        }
    }
}

void sendDHTData() {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    Serial.print("🌡️ Temperature: ");
    Serial.print(t);
    Serial.println("°C");
    Serial.print("💧 Humidity: ");
    Serial.print(h);
    Serial.println("%");

    Blynk.virtualWrite(VPIN_TEMP, t);
    Blynk.virtualWrite(VPIN_HUM, h);
}

void checkPIR() {
    if (pirState == 1) {
        int sensor_output = digitalRead(PIR_SENSOR_OUTPUT_PIN);
        if (sensor_output == LOW) {
            if (warm_up == 1) {
                warm_up = 0;
                delay(2000); // Wait for PIR to stabilize
            }
            Serial.println("No motion detected.");
            Blynk.virtualWrite(VPIN_PIR_LED, LOW);
        } else {
            Serial.println("Motion detected!");
            Blynk.virtualWrite(VPIN_PIR_LED, HIGH);
            Blynk.logEvent("pirmotion", "WARNING! Motion Detected!");
        }
    }
}

// Setup function
void setup() {
    Serial.begin(115200);
    Blynk.begin(auth, ssid, pass);

    pinMode(RELAY_PIN_1, OUTPUT);
    digitalWrite(RELAY_PIN_1, HIGH); // Turn relay off initially
    pinMode(PIR_SENSOR_OUTPUT_PIN, INPUT);
    dht.begin();

    timer.setInterval(3000L, checkSoilMoisture);
    timer.setInterval(5000L, sendDHTData);
    timer.setInterval(1000L, checkPIR);
}

// Main loop
void loop() {
    Blynk.run();
    timer.run();
}

