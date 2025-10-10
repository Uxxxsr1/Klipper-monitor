#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

struct Config
{
  static const char* ssid;
  static const char* password;
  static const char* klipperHost;
  static const int klipperPort = 7125;
  static const unsigned long updateInterval = 3000;
  static const unsigned long wifiTimeout = 15000;
  static constexpr float tempTolerance = 2.0f;

};
const char* Config::ssid = "MTSRouter_8DAA";
const char* Config::password = "65572047";
const char* Config::klipperHost = "192.168.1.166";
struct PrinterData {
  float nozzleTemp = 0;
  float nozzleTarget = 0;
  float bedTemp = 0;
  float bedTarget = 0;
  float printProgress = 0;
  String printState = "Ready";
  String fileName = "";
};
class Display
{
private:
    LiquidCrystal_I2C lcd;
public:
    Display(): lcd(0x27, 16, 2) {}

    void initilization() {
        lcd.init();
        lcd.backlight();
        lcd.clear();
    }

    void message(const String& line1, const String& line2 = "") {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(line1);
        if (!line2.isEmpty()) {
            lcd.setCursor(0, 1);
            lcd.print(line2);
        }
    }

    void showTemperatures(const PrinterData& data) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("N:");
        lcd.print(int(data.nozzleTemp));
        lcd.print("/");
        lcd.print(int(data.nozzleTarget));
        lcd.write(0xDF); // градус

        lcd.setCursor(0, 1);
        lcd.print("B:");
        lcd.print(int(data.bedTemp));
        lcd.print("/");
        lcd.print(int(data.bedTarget));
        lcd.write(0xDF);
    }

    void showPrintInfo(const PrinterData& data) {
        lcd.clear();
        lcd.setCursor(0, 0);
        String state = data.printState;
        if (state.length() > 16) state = state.substring(0, 16);
        lcd.print(state);

        lcd.setCursor(0, 1);
        lcd.print("Prog: ");
        lcd.print(int(data.printProgress));
        lcd.print("%");
    }

    void showFileName(const PrinterData& data) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("File:");
        String name = data.fileName;
        if (name.length() > 16) name = name.substring(0, 16);
        lcd.setCursor(0, 1);
        lcd.print(name);
    }

private:
    String getShortState(const String& state) {
        return state.length() > 6 ? state.substring(0, 6) : state;
    }
};
enum DisplayMode {
  MODE_TEMPERATURES,
  MODE_PRINT_INFO,
  MODE_FILE_NAME,
  MODE_COUNT
};
DisplayMode currentMode = MODE_TEMPERATURES;
constexpr unsigned long screenDuration = 5000;
unsigned long lastScreenChange = 0;
class KlipperClient {
private:
  WiFiClient client;
  
public:
  bool fetchData(PrinterData& data) {
    HTTPClient http;
    String url = String("http://") + Config::klipperHost + ":" + 
                 Config::klipperPort + "/printer/objects/query?webhooks&print_stats&extruder&heater_bed&virtual_sdcard";
    
    http.begin(client, url);
    http.setTimeout(3000);
    
    bool success = false;
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      success = parseJsonData(payload, data);
    } else {
      data.printState = "HTTP Error";
      Serial.println("HTTP failed: " + String(httpCode));
    }
    
    http.end();
    return success;
  }
  
private:
  bool parseJsonData(const String& json, PrinterData& data) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
      data.printState = "JSON Error";
      return false;
    }
    
    // Извлечение данных
    String printerStatus = doc["result"]["status"]["webhooks"]["state"] | "unknown";
    String rawPrintState = doc["result"]["status"]["print_stats"]["state"] | "unknown";
    
    data.fileName = doc["result"]["status"]["print_stats"]["filename"] | "";
    data.fileName.replace(".gcode", "");
    
    data.nozzleTemp = doc["result"]["status"]["extruder"]["temperature"] | 0.0;
    data.nozzleTarget = doc["result"]["status"]["extruder"]["target"] | 0.0;
    data.bedTemp = doc["result"]["status"]["heater_bed"]["temperature"] | 0.0;
    data.bedTarget = doc["result"]["status"]["heater_bed"]["target"] | 0.0;
    
    float progress = doc["result"]["status"]["virtual_sdcard"]["progress"] | 0.0;
    data.printProgress = progress * 100.0;
    
    data.printState = determinePrintState(printerStatus, rawPrintState, data);
    return true;
  }
  
  String determinePrintState(const String& printerStatus, const String& rawPrintState, const PrinterData& data) {
    if (printerStatus == "shutdown") return "Printer Off";
    if (printerStatus != "ready") return printerStatus;
    
    static const std::vector<std::pair<String, String>> stateMap = {
      {"standby", "Standby"},
      {"paused", "Paused"},
      {"error", "Error"},
      {"complete", "Complete"}
    };
    
    for (const auto& [input, output] : stateMap) {
      if (rawPrintState == input) return output;
    }
    
    if (rawPrintState == "printing") {
      bool isHeated = (fabs(data.nozzleTemp - data.nozzleTarget) <= Config::tempTolerance) &&
                     (fabs(data.bedTemp - data.bedTarget) <= Config::tempTolerance);
      return isHeated ? "Printing" : "Heating";
    }
    
    return rawPrintState;
  }
};

class NetworkManager {
public:
  bool connect(Display& display) {
    display.message("connecting", Config::ssid);
    
    WiFi.begin(Config::ssid, Config::password);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && 
           (millis() - startTime) < Config::wifiTimeout) {
      delay(500);
      Serial.print(".");
    }
    
    bool connected = (WiFi.status() == WL_CONNECTED);
    display.message(connected ? "connected" : "failed");
    
    if (connected) {
      Serial.println("\nconnected!");
    }
    
    return connected;
  }
  
  bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
  }
};

// Глобальные объекты
Display display;
KlipperClient klipperClient;
NetworkManager networkManager;
PrinterData printerData;
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  display.initilization();
  
  display.message("Klipper Monitor", "Starting...");
  
  if (networkManager.connect(display)) {
    delay(1000);
  } else {
    display.message("setup failed", "check WiFi");
    while(true) delay(1000);
  }
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastScreenChange >= screenDuration) {
    currentMode = static_cast<DisplayMode>((currentMode + 1) % MODE_COUNT);
    lastScreenChange = currentTime;
  }

  if (currentTime - lastUpdate >= Config::updateInterval) {
    lastUpdate = currentTime;
    if (!networkManager.isConnected()) {
      display.message("Reconnecting...");
      networkManager.connect(display);
    } else {
      if (!klipperClient.fetchData(printerData)) {
        display.message("Fetch Error", printerData.printState);
      }
    }
  }

  if (networkManager.isConnected() && 
      printerData.printState != "HTTP Error" && 
      printerData.printState != "JSON Error") {
    switch (currentMode) {
      case MODE_TEMPERATURES:
        display.showTemperatures(printerData);
        break;
      case MODE_PRINT_INFO:
        display.showPrintInfo(printerData);
        break;
      case MODE_FILE_NAME:
        display.showFileName(printerData);
        break;
    }
  }

  delay(100);
}