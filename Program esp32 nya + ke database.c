#include <WiFi.h>
#include <HTTPClient.h>

// 1. KONFIGURASI JARINGAN & SERVER XAMPP
// Wifi
const char* ssid = "Nazril"; 
const char* password = "Nnnnnnnn";
// IP Address lokal
const char* serverHost = "10.56.80.161"; 
const int serverPort = 80;
const char* serverPath = "/iot_water/save_data.php"; // Folder dan file PHP

// 2. DEFINISI PIN & PARAMETER KONTROL
#define WATER_LEVEL_PIN   34  // Pin analog untuk Sensor Water Level K-0135
#define TDS_SENSOR_PIN    35  // Pin analog untuk Sensor TDS
#define FILL_VALVE_PIN    25  // Pin digital untuk Relay Solenoid Valve ISI Air (Output)
#define DRAIN_VALVE_PIN   26  // Pin digital untuk Relay Solenoid Valve BUANG Air (Output)
// Tahap 1: Volume Air - Mengisi Air
#define MINIMUM_WATER_LEVEL_ADC  100  // Nilai ADC minimum (air perlu diisi jika <= 100)
#define NORMAL_WATER_LEVEL_ADC  1500  // Nilai ADC normal (sampai level ini, pengisian dihentikan)
// Tahap 2: Kualitas Air - Mengganti Air
#define MAX_TDS_PPM_THRESHOLD  200   // Nilai TDS maksimum (ppm). Di atas nilai ini, air diganti
#define TDS_CHECK_INTERVAL     30000  // Interval cek TDS (ms), misalnya 30 detik
// Relay menggunakan Active LOW
#define VALVE_ON  LOW
#define VALVE_OFF HIGH
// Variabel State Global & TDS
unsigned long lastTDSCheckTime = 0;
#define VREF              3.3   // Tegangan referensi (Volt) ADC ESP32
#define SCOUNT            30    // Jumlah sampel
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25.0; // Anggap suhu konstan 25C

// 3. FUNGSI UTILITY
// Fungsi Median Filtering (dari Referensi TDS)
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) return bTab[(iFilterLen - 1) / 2];
  else return (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}
//Fungsi Pembacaan Sensor TDS
float readTDS() {
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TDS_SENSOR_PIN);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) analogBufferIndex = 0;
  }

  for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
    analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
  }

  averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4095.0;
  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
  float compensationVoltage = averageVoltage / compensationCoefficient;
  // Rumus kalibrasi TDS
  tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage
              - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;

  return tdsValue;
}

// Fungsi Kontrol Valve
void controlValve(int pin, bool state) {
  if (state == true) { // ON
    digitalWrite(pin, VALVE_ON);
  } else { // OFF
    digitalWrite(pin, VALVE_OFF);
  }
}

// 4. FUNGSI IOT LOGGING
// Fungsi Koneksi Wi-Fi
void setupWifi() {
  Serial.print("Menghubungkan ke ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}
// --- Fungsi Pengiriman Data (HTTP POST) ---
void sendEvent(const char* eventType, float sensorValue) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://" + String(serverHost) + ":" + String(serverPort) + String(serverPath);

    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Buat data POST: event_type=FILL&sensor_value=1500.00
    String httpRequestData = "event_type=" + String(eventType) + 
                             "&sensor_value=" + String(sensorValue, 2); // 2 desimal

    Serial.print("Mengirim data [");
    Serial.print(eventType);
    Serial.print("]: ");
    Serial.println(httpRequestData);

    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
      Serial.print("Response Code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error saat mengirim data: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi tidak terhubung. Gagal mengirim data.");
  }
}


// 5. SETUP & LOOP
// Setup
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- Sistem Kontrol Air IoT Dimulai ---");
  // Inisialisasi Wi-Fi
  setupWifi(); 
  // Inisialisasi pin input
  pinMode(WATER_LEVEL_PIN, INPUT);
  pinMode(TDS_SENSOR_PIN, INPUT);
  // Inisialisasi pin output (Valve/Relay)
  pinMode(FILL_VALVE_PIN, OUTPUT);
  pinMode(DRAIN_VALVE_PIN, OUTPUT);
  // Pastikan kedua valve nonaktif di awal
  controlValve(FILL_VALVE_PIN, false);
  controlValve(DRAIN_VALVE_PIN, false);
}

// Loop Utama
void loop() {
  // Matikan kedua valve jika tidak ada aksi yang diperlukan
  controlValve(FILL_VALVE_PIN, false);
  controlValve(DRAIN_VALVE_PIN, false);
  // 1. Cek Kualitas Air (TDS) - Tahap 2
  if (millis() - lastTDSCheckTime >= TDS_CHECK_INTERVAL) {
    float currentTDS = readTDS();
    Serial.print("\n[TDS Check] TDS Value: ");
    Serial.print(currentTDS, 0);
    Serial.println(" ppm");
    // LOGIKA REVISI: JIKA TDS > MAX_TDS_PPM_THRESHOLD MAKA GANTI AIR
    if (currentTDS > MAX_TDS_PPM_THRESHOLD) {
      Serial.println("** TDS TINGGI! Memulai proses GANTI AIR (Buang)...");
      // Output: Aktifkan Solenoid Valve Pembuangan
      controlValve(DRAIN_VALVE_PIN, true);
      Serial.println("-> Valve BUANG ON.");
      // Tunggu hingga tangki kosong. Ganti delay ini dengan monitoring level yang sebenarnya!
      Serial.println("...Membuang air (simulasi 10 detik)...");
      delay(10000);
      // Output: Nonaktifkan Solenoid Valve Pembuangan
      controlValve(DRAIN_VALVE_PIN, false);
      Serial.println("-> Valve BUANG OFF. Penggantian selesai.");
      // --- LOGGING KE WEBSITE/DATABASE ---
      sendEvent("DRAIN", currentTDS); 
      // Setelah ini, sistem akan otomatis masuk ke tahap pengisian air di langkah 2
      lastTDSCheckTime = millis();
    } else {
      Serial.println("TDS normal/rendah. Melanjutkan pengecekan level air.");
    }
    lastTDSCheckTime = millis();
  }

  // 2. Cek Volume Air - Tahap 1
  int currentLevelADC = analogRead(WATER_LEVEL_PIN);
  Serial.print("[Level Check] Level ADC: ");
  Serial.println(currentLevelADC);
  // LOGIKA: JIKA Level Air ADC <= MINIMUM_WATER_LEVEL_ADC MAKA ISI AIR
  if (currentLevelADC <= MINIMUM_WATER_LEVEL_ADC) {
    Serial.println("!! Level air SANGAT RENDAH! Memulai pengisian air...");
    // Output: Aktifkan Solenoid Valve untuk air masuk
    controlValve(FILL_VALVE_PIN, true);
    Serial.println("-> Valve ISI ON.");
    // Tunggu (blocking) hingga level air mencapai nilai normal
    while (analogRead(WATER_LEVEL_PIN) < NORMAL_WATER_LEVEL_ADC) {
      delay(500); 
      Serial.print("...Mengisi air. Level saat ini: ");
      Serial.println(analogRead(WATER_LEVEL_PIN));
    }
    // Level air sudah mencapai normal
    controlValve(FILL_VALVE_PIN, false);
    Serial.println("-> Valve ISI OFF. Level air normal.");
    // --- LOGGING KE WEBSITE/DATABASE ---
    sendEvent("FILL", (float)NORMAL_WATER_LEVEL_ADC); 
    delay(5000); // Beri jeda setelah pengisian
  } else {
    // Level air normal
    controlValve(FILL_VALVE_PIN, false);
  }
  // Update pembacaan TDS terus menerus (non-blocking)
  readTDS();
  // Jeda singkat sebelum loop berikutnya
  delay(1000);
}