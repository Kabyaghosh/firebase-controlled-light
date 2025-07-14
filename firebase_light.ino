#include <WiFi.h>
#include <WiFiManager.h>     // Install: https://github.com/tzapu/WiFiManager
#include <FirebaseESP32.h>   // Library v3.x

#define FIREBASE_HOST "smart-home-94ea7-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_API_KEY "AIzaSyArqTImm0-YrEfWEjrH6DCdY9vrJQsayIg"
#define USER_EMAIL "kabyaghosh4@gmail.com"
#define USER_PASSWORD "kabya@ghosh"

#define RELAY_PIN 2
#define BUZZER_PIN 4

FirebaseData firebaseData;
FirebaseData streamData;
FirebaseAuth auth;
FirebaseConfig config;

bool wifiConnected = false;
bool streamStarted = false;
int lastRelayState = -1;

// ───── Melody ─────
int melody[] = {262, 294, 330, 349, 392}; // C D E F G
int melodyDuration[] = {150, 150, 150, 150, 150};

void playMelody() {
  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, melody[i]);
    delay(melodyDuration[i]);
    noTone(BUZZER_PIN);
    delay(50);
  }
}

void twoQuickBeeps() {
  for (int i = 0; i < 2; i++) {
    tone(BUZZER_PIN, 880);
    delay(100);
    noTone(BUZZER_PIN);
    delay(100);
  }
}

void threeSoftBeeps() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 660);
    delay(80);
    noTone(BUZZER_PIN);
    delay(120);
  }
}

void threeLoudBeeps() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1000);
    delay(300);
    noTone(BUZZER_PIN);
    delay(200);
  }
}

void wifiLostBeep() {
  tone(BUZZER_PIN, 500);
  delay(150);
  noTone(BUZZER_PIN);
}

// ───── Stream Callback ─────
void streamCallback(StreamData data) {
  if (data.dataType() == "int") {
    int state = data.intData();
    digitalWrite(RELAY_PIN, state == 1 ? HIGH : LOW);
    lastRelayState = state;
    Serial.print("Relay Changed from Stream: ");
    Serial.println(state);
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("⏱️ Stream Timeout... attempting to reconnect");
  }
}

// ───── WiFi Connect ─────
bool connectWiFi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);  // 3 minutes
  if (!wm.autoConnect("ESP32_Setup", "kabya123")) {
    Serial.println("⛔ Failed to connect. Restarting...");
    ESP.restart();
    return false;
  }
  Serial.println("✅ WiFi Connected! IP: " + WiFi.localIP().toString());
  wifiConnected = true;
  return true;
}

// ───── Setup ─────
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  WiFi.setSleep(false);  // prevent WiFi drops

  playMelody();  // 🎵 Startup

  if (!connectWiFi()) return;
  threeSoftBeeps(); // ✅ Connection confirmed

  // Firebase setup
  config.host = FIREBASE_HOST;
  config.api_key = FIREBASE_API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Wait for sign-in
  unsigned long start = millis();
  Serial.print("Signing in to Firebase");
  while (auth.token.uid == "" && millis() - start < 15000) {
    Serial.print(".");
    delay(300);
  }

  if (auth.token.uid != "") {
    Serial.println("\n🔥 Firebase signed in");

    // Start stream
    if (Firebase.beginStream(streamData, "/relay")) {
      Firebase.setStreamCallback(streamData, streamCallback, streamTimeoutCallback);
      streamStarted = true;
      Serial.println("📶 Firebase stream started");
    } else {
      Serial.print("❌ Stream failed: ");
      Serial.println(streamData.errorReason());
    }

    // Initial relay state
    if (Firebase.getInt(firebaseData, "/relay")) {
      int state = firebaseData.intData();
      digitalWrite(RELAY_PIN, state == 1 ? HIGH : LOW);
      lastRelayState = state;
      Serial.print("Initial Relay State: ");
      Serial.println(state);
    }
  } else {
    Serial.println("\n❌ Firebase sign-in failed!");
    threeLoudBeeps();
    delay(500);
    ESP.restart();
  }
}

// ───── Loop ─────
void loop() {
  // If Wi-Fi disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📡 WiFi lost...");
    wifiLostBeep();
    delay(1000);
    ESP.restart();  // Try to reconnect with WiFiManager
  }

  // Refresh Firebase internal loop
  Firebase.ready();

  // Fallback polling every 5 seconds (if stream failed)
  static unsigned long lastCheck = 0;
  if (!streamStarted && millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (Firebase.getInt(firebaseData, "/relay")) {
      int relayState = firebaseData.intData();
      if (relayState != lastRelayState) {
        digitalWrite(RELAY_PIN, relayState == 1 ? HIGH : LOW);
        lastRelayState = relayState;
        Serial.print("Relay Changed (polling): ");
        Serial.println(relayState);
      }
    } else {
      Serial.print("🔥 Firebase Error: ");
      Serial.println(firebaseData.errorReason());
      threeLoudBeeps();
      delay(1000);
      ESP.restart();
    }
  }

  // Optional: beeping in config portal mode
  static unsigned long configBeepTimer = 0;
  if (!wifiConnected && millis() - configBeepTimer > 2000) {
    twoQuickBeeps();
    configBeepTimer = millis();
  }
}
