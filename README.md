# Klipper Status Monitor for ESP8266

## Overview

This project implements a Klipper 3D printer status monitoring system using an ESP8266 microcontroller with an I2C LCD display. The device connects to a Klipper instance via WiFi and displays real-time printing information including temperatures, print progress, and file names.

## Hardware Requirements

- ESP8266 development board (NodeMCU, Wemos D1 Mini, etc.)
- I2C LCD display (16x2 characters)
- Jumper wires for connections
- Power supply (5V)

## Pin Connections

- **SDA** → GPIO 4 (D2 on NodeMCU)
- **SCL** → GPIO 5 (D1 on NodeMCU)
- **VCC** → 5V
- **GND** → GND

## Software Dependencies

- Arduino IDE with ESP8266 Core
- Required libraries:
  - `Wire.h` (I2C communication)
  - `LiquidCrystal_I2C.h` (LCD control)
  - `ESP8266WiFi.h` (WiFi connectivity)
  - `ESP8266HTTPClient.h` (HTTP requests)
  - `ArduinoJson.h` (JSON parsing)

## Configuration

Edit the `Config` structure in the code to match your environment:

```cpp
struct Config {
  static const char* ssid;          // WiFi network name
  static const char* password;      // WiFi password
  static const char* klipperHost;   // Klipper IP address (e.g., "192.168.1.166")
  static const int klipperPort;     // Klipper port (default: 7125)
  static const unsigned long updateInterval;  // Data refresh rate (ms)
  static const unsigned long wifiTimeout;     // WiFi connection timeout (ms)
  static constexpr float tempTolerance;       // Temperature tolerance (°C)
};
```

## Features

### Display Modes

The system cycles through three display modes automatically every 5 seconds:

1. **Temperature Mode**
   - Nozzle current/target temperature
   - Bed current/target temperature
   - Displays in Celsius with degree symbol

2. **Print Information Mode**
   - Printer state (Printing, Paused, Heating, etc.)
   - Print progress percentage

3. **File Name Mode**
   - Current printing file name (without .gcode extension)
   - Truncated to fit 16-character display

### Printer State Detection

The system intelligently determines printer status by analyzing:
- Webhooks state (printer readiness)
- Print statistics state
- Temperature conditions
- Specific states handled: shutdown, standby, paused, error, complete, printing, heating

### Error Handling

- WiFi connection status monitoring
- HTTP request timeout handling (3 seconds)
- JSON parsing error detection
- Automatic reconnection attempts

## Klipper API Integration

The monitor queries Klipper's REST API endpoint:
```
http://[klipperHost]:[port]/printer/objects/query?webhooks&print_stats&extruder&heater_bed&virtual_sdcard
```

### Data Retrieved

- Printer state from webhooks
- Print statistics and filename
- Extruder temperature and target
- Heater bed temperature and target
- Print progress from virtual SD card

## Operation

### Startup Sequence

1. Initializes LCD display
2. Attempts WiFi connection with timeout
3. Displays connection status
4. Enters main monitoring loop

### Main Loop Operations

- Cycles display modes every 5 seconds
- Fetches data from Klipper every 3 seconds
- Handles network reconnection automatically
- Updates display with current information

## Customization

### Timing Parameters

- `updateInterval`: Data refresh rate (default: 3000ms)
- `wifiTimeout`: Connection timeout (default: 15000ms)
- `screenDuration`: Display mode duration (default: 5000ms)

### Temperature Settings

- `tempTolerance`: Heating threshold (default: 2.0°C)

## Troubleshooting

### Common Issues

1. **WiFi Connection Failed**
   - Verify SSID and password
   - Check router settings
   - Ensure ESP8266 is in range

2. **Klipper Connection Failed**
   - Verify Klipper host IP address
   - Check Moonraker is running on specified port
   - Confirm network connectivity between devices

3. **LCD Not Displaying**
   - Verify I2C address (default: 0x27)
   - Check wiring connections
   - Confirm 5V power supply

4. **Incorrect Data Display**
   - Check Klipper API accessibility
   - Verify JSON response format
   - Monitor serial output for error messages

## Serial Output

The device outputs debug information to Serial at 115200 baud, including:
- Connection status
- HTTP response codes
- Error messages
- JSON parsing issues

## License

This project is designed for personal use with Klipper 3D printer systems. Ensure compliance with local regulations and network policies.
