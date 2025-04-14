#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

#define SDA_PIN 4  // GPIO4 (D2)
#define SCL_PIN 5  // GPIO5 (D1)
#define LED_PIN 12 // GPIO12 (D6)
#define BUZZER_PIN 13 // GPIO13 (D7)

// Initialize the LCD (I2C Address: 0x27, 16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long pkts = 0;        // Total packets
unsigned long deauths = 0;     // Deauthentication packets
unsigned long prevTime = 0;    // Previous time for periodic tasks
unsigned long curTime = 0;     // Current time
bool attackDetected = false;   // Flag to track attack status
const int fixedChannel = 6;    // Fixed Wi-Fi channel

// Callback function for promiscuous mode
void sniffer(uint8_t *buf, uint16_t len) {
  pkts++;

  // Check for Deauth frame type (0xA0 or 0xC0 for Deauth)
  if ((buf[12] == 0xA0 || buf[12] == 0xC0) && len > 30) {  
    deauths++;

    // Extract MAC address of attacker (source MAC is at index 10 and 16 in the frame)
    uint8_t attackerMac[6];
    for (int i = 0; i < 6; i++) {
      attackerMac[i] = buf[10 + i];
    }

    // If multiple deauths detected, trigger alarm
    if (deauths >= 3) {  
      attackDetected = true;  // Set attack flag
      
      Serial.println("🔥 Deauth Attack Detected!");
      Serial.print("🚨 Attacker MAC: ");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Deauth Detected!");

      lcd.setCursor(0, 1);
      for (int i = 0; i < 6; i++) {
        if (attackerMac[i] < 0x10) Serial.print("0");
        Serial.print(attackerMac[i], HEX);
        lcd.print(attackerMac[i], HEX);
        if (i < 5) {
          Serial.print(":");
          lcd.print(":");
        }
      }
      Serial.println();

      // Activate LED and buzzer
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize I2C for LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Monitoring...");
  
  // Initialize LED and Buzzer in OFF state
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Configure Wi-Fi in promiscuous mode
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
  WiFi.disconnect();
  wifi_set_promiscuous_rx_cb(sniffer);
  wifi_set_channel(fixedChannel);
  wifi_promiscuous_enable(1);

  Serial.println("Monitoring started on channel: " + String(fixedChannel));
}

void loop() {
  curTime = millis();

  if (curTime - prevTime >= 1000) {  // Every 1 second
    prevTime = curTime;

    if (deauths == 0 && attackDetected) {  
      // If no deauths detected, turn off alarm
      attackDetected = false;
      Serial.println("✅ No Deauth Attacks Detected");
      
      // Turn off LED and Buzzer
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);

      // Update LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Monitoring...");
    }

    // Reset packet counters
    pkts = 0;
    deauths = 0;
  }
}
