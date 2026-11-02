@echo off
title ESP32 Flight Acceleration Monitor - Project Launcher
color 0B
cls

echo ===============================================================================
echo          ESP32 FLIGHT ACCELERATION & TELEMETRY MONITOR LAUNCHER
echo                 Internet of Things (IoT) PBL - Semester 5
echo ===============================================================================
echo.
echo  Target Hardware : ESP32 DevKit V1 + ADXL345 3-Axis Accelerometer (I2C)
echo  I2C Pins        : SDA = GPIO 21  ^|  SCL = GPIO 22  ^|  Power = 3.3V
echo.
echo ===============================================================================
echo  QUICK START GUIDE (Follow these simple steps):
echo ===============================================================================
echo  1. WIRING CHECK:
echo     - ESP32 3V3  --- ADXL345 VCC & CS (CS pulled HIGH enables I2C mode)
echo     - ESP32 GND  --- ADXL345 GND & SDO (SDO to GND sets address 0x53)
echo     - ESP32 D21  --- ADXL345 SDA
echo     - ESP32 D22  --- ADXL345 SCL
echo.
echo  2. CONFIGURE WI-FI HOTSPOT:
echo     - Open 'ESP32_Flight_Acceleration_Monitor.ino'
echo     - Edit lines 28-29 with your Phone Hotspot SSID and Password:
echo          const char* WIFI_SSID     = "YourHotspotName";
echo          const char* WIFI_PASSWORD = "YourPassword";
echo.
echo  3. INSTALL ARDUINO LIBRARIES (if not already installed):
echo     - In Arduino IDE: Tools -> Manage Libraries...
echo     - Search and install: "Adafruit ADXL345" and "Adafruit Unified Sensor"
echo.
echo  4. UPLOAD & MONITOR:
echo     - Board: Tools -> Board -> ESP32 Arduino -> ESP32 Dev Module
echo     - Port:  Tools -> Port -> COM[X] (Select your ESP32 COM port)
echo     - Upload Sketch (Ctrl+U)
echo     - Open Serial Monitor at 115200 baud (Ctrl+Shift+M)
echo     - Note the IP address printed on screen (e.g., http://192.168.43.50)
echo.
echo  5. OPEN FLIGHT TELEMETRY DASHBOARD:
echo     - Open any browser (Chrome / Edge / Phone browser connected to hotspot)
echo     - Navigate to the IP address shown in Serial Monitor!
echo ===============================================================================
echo.

set PROJECT_DIR=%~dp0
set INO_FILE=%PROJECT_DIR%ESP32_Flight_Acceleration_Monitor\ESP32_Flight_Acceleration_Monitor.ino

echo [1/3] Opening project directory in Windows Explorer...
explorer "%PROJECT_DIR%"

echo [2/3] Checking for Arduino IDE / Sketch association...
if exist "%INO_FILE%" (
    echo [FOUND] Arduino sketch detected:
    echo         %INO_FILE%
    echo.
    echo Opening sketch in default Arduino IDE editor...
    start "" "%INO_FILE%"
) else (
    echo [NOTICE] Sketch file not found at default path.
)

echo.
echo [3/3] System Ready! Press any key to close this launcher console...
pause >nul
