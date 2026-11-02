# ESP32 Flight Acceleration Monitor
> **High-Rate Inertial Dynamics & Aerodynamic Telemetry Server**  
> *Internet of Things (IoT) Project-Based Learning — Semester 5*

---

## 1. Project Overview
The **ESP32 Flight Acceleration Monitor** is an edge-computed flight telemetry device designed using only an **ESP32 microcontroller** and an **ADXL345 3-axis digital accelerometer**. It connects in Station mode to a smartphone Wi-Fi hotspot or local router and serves a responsive, dark-themed, aerospace-style telemetry dashboard directly from its internal flash memory (zero external servers, zero CDNs).

The device measures tri-axial acceleration ($m/s^2$ and $g$), computes total resultant G-force, estimates airframe pitch and roll angles, quantifies structural vibration (RMS), tracks all-time peak G-forces, and executes an automated flight state machine classifying flight behavior in real time.

---

## 2. Hardware Required
This project strictly requires no extra components, displays, or external internet connections:
1. **ESP32 Development Board** (NodeMCU-32S, ESP32 DevKit V1, or any 30/38-pin ESP-WROOM-32)
2. **ADXL345 3-Axis Digital Accelerometer Module**
3. **Breadboard & 4–6 Jumper Wires** (Female-to-Male or Female-to-Female)
4. **Micro-USB Cable** (Data and Power)

---

## 3. Exact Hardware Wiring Table

The ADXL345 communicates with the ESP32 via standard I2C at **3.3V logic level**.

> [!CAUTION]
> Always connect **VCC to 3.3V (3V3)** on the ESP32. Do **NOT** connect ADXL345 to the 5V/VIN pin unless your breakout module explicitly has an onboard 3.3V LDO regulator.

| ADXL345 Sensor Pin | ESP32 GPIO / Pin | Function / Description |
|:---|:---|:---|
| **VCC** | **3V3** | 3.3V Regulated Power Rail |
| **GND** | **GND** | Common Ground |
| **CS** | **3V3** | **Tie HIGH to 3.3V** to enable I2C protocol mode (disables SPI) |
| **SDO** | **GND** | **Tie LOW to GND** to assign default I2C address `0x53` |
| **SDA** | **GPIO 21** | Hardware I2C Serial Data |
| **SCL** | **GPIO 22** | Hardware I2C Serial Clock |

*(Note: If your ADXL345 board already has onboard pull-ups on CS and pull-down on SDO, connecting just VCC, GND, SDA, and SCL is sufficient. However, explicitly tying CS to 3V3 and SDO to GND guarantees 100% reliable initialization across all clones).*

---

## 4. Software Setup & Library Installation

### Step 1: Install Arduino IDE and ESP32 Core
1. Download and install **Arduino IDE 2.x** from [arduino.cc](https://www.arduino.cc/en/software).
2. Open **File** > **Preferences** and paste this URL into **Additional boards manager URLs**:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Go to **Tools** > **Board** > **Boards Manager...**, search for `esp32`, and click **Install** (by *Espressif Systems*).

### Step 2: Install Sensor Libraries
In Arduino IDE, open **Tools** > **Manage Libraries...** (or press `Ctrl + Shift + I`), search and install:
1. **Adafruit ADXL345** (by *Adafruit*)
2. **Adafruit Unified Sensor** (by *Adafruit*)
*(When prompted to install dependencies, click "Install All")*.

*(Note: `WiFi.h`, `WebServer.h`, and `Wire.h` are built directly into the ESP32 core and require no external library installations).*

---

## 5. Configuring Wi-Fi Credentials
1. Open [`ESP32_Flight_Acceleration_Monitor.ino`](file:///e:/samridhi/sem5/IoT%20Lab/Internet-Of-Things-PBL-SEM-5-/ESP32_Flight_Acceleration_Monitor/ESP32_Flight_Acceleration_Monitor.ino).
2. At the top of the file (lines 28–29), locate the configuration constants:
   ```cpp
   const char* WIFI_SSID     = "MyPhoneHotspot";     // Replace with your Phone Hotspot SSID
   const char* WIFI_PASSWORD = "password1234";       // Replace with your Hotspot Password
   ```
3. Enter your phone hotspot name and password. Ensure your phone hotspot frequency is set to **2.4 GHz band** (standard ESP32 supports 2.4 GHz Wi-Fi).

---

## 6. How to Upload and Find the IP Address
1. Connect the ESP32 to your PC via a micro-USB cable.
2. In Arduino IDE:
   - **Board:** Select `Tools` > `Board` > `ESP32 Arduino` > `ESP32 Dev Module` (or your specific ESP32 variant).
   - **Port:** Select `Tools` > `Port` > `COM[X]` (e.g., COM3, COM4).
3. Click the **Upload** button (`Ctrl + U`). If the terminal shows `Connecting........_____`, press and hold the **BOOT** button on the ESP32 for 2 seconds until the upload starts.
4. Once uploaded, open **Tools** > **Serial Monitor** (`Ctrl + Shift + M`) and set the baud rate to **115200**.
5. Press the **EN / RESET** button on the ESP32.
6. The Serial Monitor will output:
   ```text
   [WiFi] Connecting in Station mode to SSID: MyPhoneHotspot
   ......
   [WiFi] Connection Established!
   [WiFi] ESP32 Assigned IP Address: http://192.168.43.50
   [HTTP] Flight Telemetry Web Server started successfully!
   ```

---

## 7. How to Open the Flight Dashboard
1. Ensure your smartphone or laptop is connected to the same phone hotspot / Wi-Fi network.
2. Open any web browser (Chrome, Edge, Safari, Firefox).
3. In the URL address bar, type the IP address displayed in the Serial Monitor (e.g., `http://192.168.43.50`).
4. The dashboard will instantly load with live real-time gauges, pitch/roll indicators, and a streaming G-force graph!

> [!TIP]
> **Safety Fallback AP:** If no Wi-Fi hotspot is reachable within 15 seconds, the ESP32 automatically generates its own wireless network named **`ESP32-Flight-Monitor`** (Password: `12345678`). Simply connect your phone to it and navigate to `http://192.168.4.1`.

---

## 8. Flight Status Classifications Explained

The onboard flight dynamics state machine categorizes motion into four distinct conditions:

| Status Message | Trigger Condition | Physical Interpretation |
|:---|:---|:---|
| **`Idle`** | $0.85\,g \le G_{\text{total}} \le 1.25\,g$ | Stationary on launchpad or cruising in steady 1-G level flight. |
| **`Accelerating`** | $G_{\text{total}} > 1.35\,g$ | Active thruster burn, steep climb, high-G turn, or hand push. |
| **`Free-fall`** | $G_{\text{total}} < 0.25\,g$ | Zero-G ballistic trajectory, parabolic drop, or sensor in mid-air toss. |
| **`Impact`** | $G_{\text{total}} \ge 3.00\,g$ | Hard landing, collision, launch shock, or sharp slap on testbench. Latched for 1.2 seconds for clear observation. |

---

## 9. Operating Modes: Handheld Testing vs Vehicle/Drone Deployment

### Handheld Bench Testing
- Hold the ESP32 and breadboard securely in your palm.
- **Pitch & Roll:** Tilt forward/backward to watch Pitch ($\theta$) vary between $\pm 90^\circ$, and tilt side-to-side to watch Roll ($\phi$) vary between $\pm 180^\circ$.
- **Free-fall Test:** Gently drop the assembly onto a soft cushion or couch from a few inches; observe the status switch to cyan `Free-fall` as $G_{\text{total}}$ plunges below $0.25\,g$.
- **Impact Test:** Tap the table near the sensor to trigger the red flashing `Impact` alarm and register peak G.

### Vehicle & Drone Deployment
- **Mounting Orientation:** Fix the sensor firmly to the vehicle frame near the Center of Gravity (CoG) using vibration-damping double-sided tape.
- **Vibration RMS Analysis:** In flight, propeller wash and motor harmonics cause structural micro-vibrations. RMS readings below $0.10\,g$ indicate balanced propellers; readings above $0.35\,g$ suggest dynamic imbalance, loose motor mounts, or aerodynamic turbulence.
- **Dynamic G-Load:** Monitor pull-up maneuvers, aerodynamic braking, and landing touchdown forces.

---

## 10. Syllabus Mapping (Units I–IV)

This project directly demonstrates core engineering outcomes of university-level IoT courses:

### Unit I: Introduction to IoT & Embedded Architectures
- **Microcontroller Architecture:** Dual-core 32-bit Xtensa LX6 architecture, hardware timers, and memory partitioning (`PROGMEM` flash storage).
- **Sensor Interfacing:** Digital MEMS transducer physics, capacitive acceleration sensing, and multi-range dynamic scaling ($\pm 16g$).

### Unit II: IoT Communication Protocols & Bus Topologies
- **I2C Serial Bus:** Synchronous 2-wire serial protocol (SDA/SCL), master-slave addressing (`0x53`), clock frequency configuration ($400\,\text{kHz}$ Fast Mode).
- **Wi-Fi 802.11 b/g/n:** Station (STA) mode connection handshake, DHCP dynamic IP negotiation, and SoftAP fallback mechanisms.

### Unit III: IoT Networking & Application Layer Protocols
- **HTTP / REST Architecture:** Stateless client-server model, endpoint routing (`/`, `/data`, `/reset_peak`), and asynchronous JSON payload serialization.
- **Zero-Dependency Edge Serving:** Serving minified HTML5/CSS3/JavaScript assets directly from embedded flash without cloud relays.

### Unit IV: Data Processing, Edge Analytics & Visualization
- **Real-Time Digital Signal Processing:** Euclidean vector norms, Euler attitude decomposition, and sliding-window Root-Mean-Square (RMS) vibration metrics.
- **Client-Side Rendering:** HTML5 Canvas hardware acceleration, dynamic auto-scaling, and telemetry visualization.

---

## 11. Quick Start: How to Run in Under 5 Minutes

```text
[Step 1] Connect Hardware:
         ADXL345 VCC & CS -> ESP32 3V3
         ADXL345 GND & SDO -> ESP32 GND
         ADXL345 SDA -> ESP32 GPIO 21
         ADXL345 SCL -> ESP32 GPIO 22

[Step 2] Double-click "Open_Project.bat" in this directory.
         It will open the folder and launch the Arduino IDE.

[Step 3] Update Wi-Fi constants (lines 28-29) with your Hotspot Name & Password.

[Step 4] Select Tools -> Board -> ESP32 Dev Module and your COM Port.
         Press Ctrl + U to upload.

[Step 5] Open Serial Monitor (115200 baud). Note the IP address.
         Open that IP address in your browser (Phone or PC). Enjoy live telemetry!
```

---

## 12. Project File Structure
```
Internet-Of-Things-PBL-SEM-5-/
├── ESP32_Flight_Acceleration_Monitor/
│   └── ESP32_Flight_Acceleration_Monitor.ino   # Complete production sketch
├── README.md                                    # Main documentation and guide
├── Open_Project.bat                             # Windows launcher script
└── PROJECT_IMPLEMENTATION_DOCUMENT.md          # Comprehensive implementation document
```

---
*Developed for IoT Laboratory & Project-Based Learning (PBL) Academic Submissions.*
