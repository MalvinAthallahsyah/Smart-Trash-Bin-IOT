#define BLYNK_TEMPLATE_ID "TMPL6hUXdhmRF"
#define BLYNK_TEMPLATE_NAME "Smart Trash Bin"
#define BLYNK_AUTH_TOKEN "MZtLSxBANKBTjKXKJGhIJajwHaBhg-e7"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ---------- WiFi (Wokwi) ----------
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// ---------- Pin ----------
const int PIN_TRIG = 5;
const int PIN_ECHO = 18;
const int PIN_BUZZER = 19;

// ---------- Threshold (cm) ----------
const float THRESHOLD_FULL = 10.0;    // < 10 cm  -> FULL
const float THRESHOLD_WARNING = 20.0; // 10-20 cm -> WARNING, > 20 cm -> NORMAL

// ---------- Interval (ms) ----------
const unsigned long SENSOR_INTERVAL = 3000; // baca & kirim data tiap 3 detik
const unsigned long BEEP_INTERVAL = 300;    // kedip buzzer saat FULL

// ---------- State ----------
enum BinStatus
{
  NORMAL,
  WARNING,
  FULL
};
BinStatus currentStatus = NORMAL;
BinStatus previousStatus = NORMAL;

float distanceCm = 0;
bool buzzerActive = false;   // true saat status FULL
bool buzzerPinState = false; // kondisi fisik pin (untuk efek beep)

unsigned long lastSensorMillis = 0;
unsigned long lastBeepMillis = 0;

// Mengukur jarak (cm). Mengembalikan -1 jika tidak ada echo.
float readDistanceCm()
{
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_ECHO, HIGH, 30000UL); // timeout 30 ms
  if (duration == 0)
    return -1;
  return duration * 0.0343f / 2.0f;
}

const char *statusToText(BinStatus s)
{
  switch (s)
  {
  case FULL:
    return "FULL";
  case WARNING:
    return "WARNING";
  default:
    return "NORMAL";
  }
}

void updateStatus(float d)
{
  if (d < THRESHOLD_FULL)
    currentStatus = FULL;
  else if (d <= THRESHOLD_WARNING)
    currentStatus = WARNING;
  else
    currentStatus = NORMAL;

  buzzerActive = (currentStatus == FULL);
}

void updateStatusColor() {
  const char* color = "#23C48E";            // hijau  = NORMAL
  if (currentStatus == WARNING) color = "#ED9D00"; // kuning
  if (currentStatus == FULL)    color = "#D3435C"; // merah
  Blynk.setProperty(V1, "color", color);
}

void sendToBlynk()
{
  Blynk.virtualWrite(V0, distanceCm);
  Blynk.virtualWrite(V1, statusToText(currentStatus));
  Blynk.virtualWrite(V2, buzzerActive ? "ON" : "OFF");
  updateStatusColor();

  // Notifikasi hanya saat transisi menjadi FULL (tidak spam)
  if (currentStatus == FULL && previousStatus != FULL)
  {
    Blynk.logEvent("trash_full", "Trash Bin Full");
  }
}

void handleSensor()
{
  float d = readDistanceCm();
  if (d < 0)
  {
    Serial.println("Sensor: tidak ada echo, pembacaan dilewati");
    return;
  }
  distanceCm = d;
  updateStatus(distanceCm);

  Serial.printf("Jarak: %.1f cm | Status: %s | Buzzer: %s\n",
                distanceCm, statusToText(currentStatus),
                buzzerActive ? "ON" : "OFF");

  sendToBlynk();
  previousStatus = currentStatus;
}

// Buzzer berkedip tanpa delay
void handleBuzzer()
{
  if (!buzzerActive)
  {
    if (buzzerPinState)
    {
      digitalWrite(PIN_BUZZER, LOW);
      buzzerPinState = false;
    }
    return;
  }
  unsigned long now = millis();
  if (now - lastBeepMillis >= BEEP_INTERVAL)
  {
    lastBeepMillis = now;
    buzzerPinState = !buzzerPinState;
    digitalWrite(PIN_BUZZER, buzzerPinState);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass); // server default: blynk.cloud
}

void loop()
{
  Blynk.run();

  unsigned long now = millis();
  if (now - lastSensorMillis >= SENSOR_INTERVAL)
  {
    lastSensorMillis = now;
    handleSensor();
  }

  handleBuzzer();
}
