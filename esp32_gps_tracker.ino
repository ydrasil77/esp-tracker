/*
 * ESP32-C3 GPS Tracker with OLED Display (Browser GPS + Bluetooth)
 * Device Name: doompatrol
 * 
 * Hardware Requirements:
 * - ESP32-C3 with built-in OLED screen only
 * - Optional: Battery (when available)
 * 
 * Pin Configuration:
 * - OLED: Built-in I2C (SDA: GPIO8, SCL: GPIO9)
 * - Status LED: GPIO2 (built-in)
 * 
 * Features:
 * - Browser-based GPS data via Web Bluetooth
 * - OLED display with received GPS data
 * - WiFi connectivity for web interface
 * - Bluetooth Low Energy (BLE) server
 * - Battery monitoring (when battery available)
 * - Web-based GPS interface
 */

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

// Device Configuration
#define DEVICE_NAME "doompatrol"
#define FIRMWARE_VERSION "2.0.0"

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Configuration - Change these to your WiFi credentials
const char* ssid = "YourWiFiSSID";         
const char* password = "YourWiFiPassword"; 

// Web Server
WebServer server(80);

// BLE Configuration
BLEServer* pServer = NULL;
BLECharacteristic* pGPSCharacteristic = NULL;
BLECharacteristic* pStatusCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// BLE Service UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define GPS_CHAR_UUID       "87654321-4321-4321-4321-cba987654321"
#define STATUS_CHAR_UUID    "11111111-2222-3333-4444-555555555555"

// Status LED
#define STATUS_LED 2

// Global Variables
float latitude = 55.6761;   // Default Copenhagen coordinates
float longitude = 12.5683;
float altitude = 45.0;
float speed_kmh = 0.0;
int satellites = 8;
float hdop = 1.2;
bool gps_fix = false;
unsigned long last_gps_update = 0;
unsigned long last_display_update = 0;
unsigned long last_wifi_check = 0;
int battery_percent = 95;  // Default when no battery connected
bool wifi_connected = false;
bool ble_connected = false;

// Preferences for storing settings
Preferences preferences;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      ble_connected = true;
      Serial.println("BLE Client connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      ble_connected = false;
      Serial.println("BLE Client disconnected");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("Received GPS data via BLE:");
        Serial.println(rxValue.c_str());
        
        // Parse JSON GPS data from browser
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, rxValue.c_str());
        
        if (doc.containsKey("lat") && doc.containsKey("lon")) {
          latitude = doc["lat"];
          longitude = doc["lon"];
          
          if (doc.containsKey("alt")) altitude = doc["alt"];
          if (doc.containsKey("speed")) speed_kmh = doc["speed"];
          if (doc.containsKey("accuracy")) hdop = doc["accuracy"] / 100.0;
          
          gps_fix = true;
          last_gps_update = millis();
          
          Serial.printf("Updated GPS: %.6f, %.6f\n", latitude, longitude);
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32-C3 Browser GPS Tracker Initializing ===");
  Serial.println("Device: " + String(DEVICE_NAME));
  Serial.println("Firmware: " + String(FIRMWARE_VERSION));
  
  // Initialize status LED
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH);
  
  // Initialize preferences
  preferences.begin("gps_tracker", false);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED display allocation failed!");
    while(1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("DOOMPATROL GPS");
  display.println("Initializing...");
  display.display();
  delay(2000);
  
  // Initialize BLE
  setupBLE();
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int wifi_attempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_attempts < 30) {
    delay(500);
    Serial.print(".");
    wifi_attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Setup web server routes
    setupWebServer();
    server.begin();
    Serial.println("Web server started");
  } else {
    wifi_connected = false;
    Serial.println();
    Serial.println("WiFi connection failed - using BLE only mode");
  }
  
  digitalWrite(STATUS_LED, LOW);
  Serial.println("=== Initialization Complete ===");
  Serial.println("Open the web interface to start GPS tracking");
  if (wifi_connected) {
    Serial.println("Web interface: http://" + WiFi.localIP().toString());
  }
  Serial.println("BLE device name: " + String(DEVICE_NAME));
}

void loop() {
  unsigned long current_time = millis();
  
  // Update display every 1 second
  if (current_time - last_display_update > 1000) {
    updateDisplay();
    last_display_update = current_time;
  }
  
  // Handle web server requests
  if (wifi_connected) {
    server.handleClient();
  }
  
  // Handle BLE connections
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }
  
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  // Check WiFi connection every 30 seconds
  if (current_time - last_wifi_check > 30000) {
    checkWiFiConnection();
    last_wifi_check = current_time;
  }
  
  // Update battery status
  updateBatteryStatus();
  
  // Blink status LED
  blinkStatusLED();
  
  // Check if GPS data is stale
  if (millis() - last_gps_update > 10000) {
    gps_fix = false;
  }
  
  delay(100);
}

void setupBLE() {
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pGPSCharacteristic = pService->createCharacteristic(
                      GPS_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pGPSCharacteristic->setCallbacks(new MyCallbacks());

  pStatusCharacteristic = pService->createCharacteristic(
                         STATUS_CHAR_UUID,
                         BLECharacteristic::PROPERTY_READ |
                         BLECharacteristic::PROPERTY_NOTIFY
                       );

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  pServer->startAdvertising();
  Serial.println("BLE GPS service started, waiting for connections...");
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Header
  display.setCursor(0, 0);
  display.print("DOOMPATROL GPS");
  display.setCursor(100, 0);
  display.print(battery_percent);
  display.print("%");
  
  // Connection status
  display.setCursor(0, 10);
  if (wifi_connected) {
    display.print("WiFi: OK");
  } else {
    display.print("WiFi: --");
  }
  
  display.setCursor(70, 10);
  if (ble_connected) {
    display.print("BLE: OK");
  } else {
    display.print("BLE: --");
  }
  
  // GPS Fix status
  display.setCursor(0, 20);
  if (gps_fix) {
    display.print("GPS: BROWSER");
  } else {
    display.print("GPS: WAITING");
  }
  
  // Coordinates
  display.setCursor(0, 30);
  display.print("LAT: ");
  display.print(latitude, 4);
  
  display.setCursor(0, 40);
  display.print("LON: ");
  display.print(longitude, 4);
  
  // Additional data
  display.setCursor(0, 50);
  display.print("ALT: ");
  display.print(altitude, 1);
  display.print("m");
  
  display.setCursor(70, 50);
  display.print("SPD: ");
  display.print(speed_kmh, 1);
  
  // Status
  display.setCursor(0, 57);
  if (gps_fix) {
    display.print("TRACKING ACTIVE");
  } else {
    display.print("OPEN WEB INTERFACE");
  }
  
  display.display();
}

void setupWebServer() {
  // Serve the main GPS interface
  server.on("/", []() {
    String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DOOMPATROL GPS Interface</title>
    <style>
        body { 
            font-family: monospace; 
            background: #000; 
            color: #00ff00; 
            margin: 0; 
            padding: 20px; 
        }
        .container { 
            max-width: 800px; 
            margin: 0 auto; 
        }
        .header { 
            text-align: center; 
            border: 2px solid #00ff00; 
            padding: 20px; 
            margin-bottom: 20px; 
        }
        .section { 
            border: 1px solid #00ff00; 
            padding: 15px; 
            margin: 10px 0; 
            border-radius: 5px; 
        }
        .button { 
            background: #000; 
            color: #00ff00; 
            border: 2px solid #00ff00; 
            padding: 10px 20px; 
            cursor: pointer; 
            margin: 5px; 
            border-radius: 5px; 
        }
        .button:hover { 
            background: #00ff00; 
            color: #000; 
        }
        .status { 
            color: #ffff00; 
        }
        .data { 
            color: #ffffff; 
        }
        #gpsData { 
            min-height: 100px; 
            border: 1px solid #333; 
            padding: 10px; 
            margin: 10px 0; 
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🛰️ DOOMPATROL GPS TRACKER 🛰️</h1>
            <div class="status">ESP32-C3 Browser GPS Interface</div>
        </div>
        
        <div class="section">
            <h3>📍 GPS Controls</h3>
            <button class="button" onclick="startGPS()">Start GPS Tracking</button>
            <button class="button" onclick="stopGPS()">Stop GPS</button>
            <button class="button" onclick="connectBLE()">Connect Bluetooth</button>
            <div class="status" id="gpsStatus">Click Start GPS to begin tracking</div>
        </div>
        
        <div class="section">
            <h3>📊 Live GPS Data</h3>
            <div id="gpsData" class="data">Waiting for GPS data...</div>
        </div>
        
        <div class="section">
            <h3>🗺️ ASCII Map Integration</h3>
            <button class="button" onclick="openMap()">Open ASCII Map Visualizer</button>
            <div class="status">View your location on the retro ASCII map</div>
        </div>
        
        <div class="section">
            <h3>📡 Device Status</h3>
            <div id="deviceStatus" class="data">Loading...</div>
        </div>
    </div>

    <script>
        let watchId = null;
        let bleDevice = null;
        let bleCharacteristic = null;
        
        function startGPS() {
            if (navigator.geolocation) {
                document.getElementById('gpsStatus').textContent = 'Starting GPS...';
                
                const options = {
                    enableHighAccuracy: true,
                    timeout: 5000,
                    maximumAge: 0
                };
                
                watchId = navigator.geolocation.watchPosition(
                    position => {
                        const gpsData = {
                            lat: position.coords.latitude,
                            lon: position.coords.longitude,
                            alt: position.coords.altitude || 0,
                            speed: (position.coords.speed || 0) * 3.6, // Convert m/s to km/h
                            accuracy: position.coords.accuracy,
                            timestamp: new Date().toISOString()
                        };
                        
                        updateGPSDisplay(gpsData);
                        sendToESP32(gpsData);
                        sendViaBLE(gpsData);
                        
                        document.getElementById('gpsStatus').textContent = 'GPS Active - High Accuracy Mode';
                    },
                    error => {
                        document.getElementById('gpsStatus').textContent = 'GPS Error: ' + error.message;
                        console.error('GPS Error:', error);
                    },
                    options
                );
            } else {
                document.getElementById('gpsStatus').textContent = 'GPS not supported by this browser';
            }
        }
        
        function stopGPS() {
            if (watchId) {
                navigator.geolocation.clearWatch(watchId);
                watchId = null;
                document.getElementById('gpsStatus').textContent = 'GPS Stopped';
            }
        }
        
        async function connectBLE() {
            try {
                document.getElementById('gpsStatus').textContent = 'Connecting to Bluetooth...';
                
                bleDevice = await navigator.bluetooth.requestDevice({
                    filters: [{ name: 'doompatrol' }],
                    optionalServices: ['12345678-1234-1234-1234-123456789abc']
                });
                
                const server = await bleDevice.gatt.connect();
                const service = await server.getPrimaryService('12345678-1234-1234-1234-123456789abc');
                bleCharacteristic = await service.getCharacteristic('87654321-4321-4321-4321-cba987654321');
                
                document.getElementById('gpsStatus').textContent = 'Bluetooth Connected! Start GPS to send data.';
            } catch (error) {
                document.getElementById('gpsStatus').textContent = 'Bluetooth Error: ' + error.message;
                console.error('Bluetooth Error:', error);
            }
        }
        
        function updateGPSDisplay(data) {
            const display = document.getElementById('gpsData');
            display.innerHTML = `
                <strong>📍 Current Location:</strong><br>
                Latitude: ${data.lat.toFixed(6)}°<br>
                Longitude: ${data.lon.toFixed(6)}°<br>
                Altitude: ${data.alt.toFixed(1)} meters<br>
                Speed: ${data.speed.toFixed(1)} km/h<br>
                Accuracy: ±${data.accuracy.toFixed(1)} meters<br>
                Updated: ${new Date(data.timestamp).toLocaleTimeString()}
            `;
        }
        
        function sendToESP32(data) {
            fetch('/gps-update', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            }).catch(err => console.log('HTTP send failed:', err));
        }
        
        async function sendViaBLE(data) {
            if (bleCharacteristic) {
                try {
                    const jsonString = JSON.stringify(data);
                    const encoder = new TextEncoder();
                    await bleCharacteristic.writeValue(encoder.encode(jsonString));
                } catch (error) {
                    console.log('BLE send failed:', error);
                }
            }
        }
        
        function openMap() {
            window.open('/map', '_blank');
        }
        
        // Update device status every 5 seconds
        setInterval(async () => {
            try {
                const response = await fetch('/status');
                const status = await response.json();
                document.getElementById('deviceStatus').innerHTML = `
                    Device: ${status.device}<br>
                    Firmware: ${status.firmware}<br>
                    Uptime: ${Math.floor(status.uptime / 60)} minutes<br>
                    Battery: ${status.battery}%<br>
                    WiFi: ${status.wifi ? 'Connected' : 'Disconnected'}<br>
                    Free Memory: ${status.free_heap} bytes
                `;
            } catch (error) {
                document.getElementById('deviceStatus').textContent = 'Unable to fetch device status';
            }
        }, 5000);
    </script>
</body>
</html>
)======";
    server.send(200, "text/html", html);
  });

  // Handle GPS updates from browser
  server.on("/gps-update", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, server.arg("plain"));
      
      if (doc.containsKey("lat") && doc.containsKey("lon")) {
        latitude = doc["lat"];
        longitude = doc["lon"];
        if (doc.containsKey("alt")) altitude = doc["alt"];
        if (doc.containsKey("speed")) speed_kmh = doc["speed"];
        if (doc.containsKey("accuracy")) hdop = doc["accuracy"] / 100.0;
        
        gps_fix = true;
        last_gps_update = millis();
        
        server.send(200, "text/plain", "GPS data received");
        return;
      }
    }
    server.send(400, "text/plain", "Invalid GPS data");
  });

  // Serve the ASCII map (redirect to existing map)
  server.on("/map", []() {
    server.sendRedirect("/");
  });

  // GPS data endpoint
  server.on("/gps", []() {
    DynamicJsonDocument doc(512);
    
    doc["fix"] = gps_fix;
    doc["lat"] = latitude;
    doc["lon"] = longitude;
    doc["alt"] = altitude;
    doc["speed"] = speed_kmh;
    doc["satellites"] = satellites;
    doc["hdop"] = hdop;
    doc["timestamp"] = millis();
    
    String response;
    serializeJson(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
  });
  
  // Status endpoint
  server.on("/status", []() {
    DynamicJsonDocument doc(256);
    
    doc["device"] = DEVICE_NAME;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["battery"] = battery_percent;
    doc["wifi"] = wifi_connected;
    doc["ble"] = ble_connected;
    doc["gps_fix"] = gps_fix;
    doc["free_heap"] = ESP.getFreeHeap();
    
    String response;
    serializeJson(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
  });
  
  // Reboot endpoint
  server.on("/reboot", []() {
    server.send(200, "text/plain", "Rebooting device...");
    delay(1000);
    ESP.restart();
  });

  // Handle CORS preflight
  server.on("/gps-update", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200);
  });
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    wifi_connected = false;
    Serial.println("WiFi connection lost, attempting reconnection...");
    WiFi.reconnect();
  } else {
    wifi_connected = true;
  }
}

void updateBatteryStatus() {
  // Simulate battery reading - replace with actual battery voltage reading when available
  static int batteryCounter = 0;
  batteryCounter++;
  
  // Simulate slow battery drain
  if (batteryCounter % 100 == 0 && battery_percent > 10) {
    battery_percent--;
  }
  
  // Reset to 95% occasionally to simulate charging
  if (battery_percent < 20) {
    battery_percent = 95;
  }
}

void blinkStatusLED() {
  static unsigned long last_blink = 0;
  static bool led_state = false;
  
  unsigned long blink_interval;
  
  if (gps_fix) {
    blink_interval = 1000; // Slow blink when GPS data is available
  } else if (wifi_connected || ble_connected) {
    blink_interval = 500;  // Medium blink when connected but no GPS
  } else {
    blink_interval = 200;  // Fast blink when no connections
  }
  
  if (millis() - last_blink > blink_interval) {
    led_state = !led_state;
    digitalWrite(STATUS_LED, led_state);
    last_blink = millis();
  }
}