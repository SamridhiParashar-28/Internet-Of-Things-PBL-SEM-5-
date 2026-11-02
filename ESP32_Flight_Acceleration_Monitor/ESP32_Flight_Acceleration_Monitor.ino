/*
 * ======================================================================================
 * Project: ESP32 Flight Acceleration & Telemetry Monitor
 * Target Microcontroller: ESP32 DevKit V1 (ESP-WROOM-32)
 * Sensor: ADXL345 3-Axis Digital Accelerometer (I2C)
 * 
 * Hardware Connections:
 *   ESP32 GPIO 21 (SDA)  <----> ADXL345 SDA
 *   ESP32 GPIO 22 (SCL)  <----> ADXL345 SCL
 *   ESP32 3.3V (3V3)     <----> ADXL345 VCC & CS (Pull CS HIGH for I2C Mode)
 *   ESP32 GND            <----> ADXL345 GND & SDO (SDO to GND sets I2C address 0x53)
 * 
 * Libraries Required (Install via Arduino IDE Library Manager):
 *   1. "Adafruit ADXL345" by Adafruit
 *   2. "Adafruit Unified Sensor" by Adafruit
 *   3. "WebServer" and "WiFi" (Pre-installed with standard ESP32 Board Core)
 * ======================================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <math.h>

// ======================================================================================
// 1. WI-FI CREDENTIALS CONFIGURATION (EDIT FOR YOUR PHONE HOTSPOT OR ROUTER)
// ======================================================================================
const char* WIFI_SSID     = "MyPhoneHotspot";     // Replace with your Phone Hotspot SSID
const char* WIFI_PASSWORD = "password1234";       // Replace with your Hotspot Password

// Hardware I2C Pin Definitions for ESP32
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// System Update Intervals (Milliseconds)
#define SENSOR_SAMPLE_INTERVAL_MS 50     // Sample sensor every 50ms (20 Hz)
#define VIBRATION_BUFFER_SIZE     20     // Moving window size for RMS vibration calculation
#define IMPACT_HOLD_TIME_MS       1200   // Latch Impact status on dashboard for 1.2s

// Acceleration Conversion Constants
#define GRAVITY_STANDARD 9.80665f

// ======================================================================================
// 2. GLOBAL HARDWARE & TELEMETRY VARIABLES
// ======================================================================================
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
WebServer server(80);

// Inertial Measurement Variables
float accX_mps2 = 0.0f, accY_mps2 = 0.0f, accZ_mps2 = 0.0f; // m/s^2
float gX = 0.0f, gY = 0.0f, gZ = 0.0f;                       // G-force units
float gTotal = 1.0f;                                         // Net Euclidean G-force
float peakG = 1.0f;                                          // Peak recorded G-force
float pitchDeg = 0.0f;                                       // Degrees (-90 to +90)
float rollDeg = 0.0f;                                        // Degrees (-180 to +180)
float vibrationRMS = 0.0f;                                   // Vibration RMS (g)

// Flight Status State Machine
enum FlightStatus {
  STATUS_IDLE,
  STATUS_ACCELERATING,
  STATUS_FREEFALL,
  STATUS_IMPACT
};
FlightStatus currentStatus = STATUS_IDLE;
unsigned long lastImpactTime = 0;

// Vibration Circular Buffer
float gHistory[VIBRATION_BUFFER_SIZE];
int gHistoryIndex = 0;
bool gHistoryFilled = false;

// Timing Control
unsigned long lastSensorReadTime = 0;
bool sensorAvailable = false;

// ======================================================================================
// 3. EMBEDDED DASHBOARD (HTML5, MODERN AEROSPACE CSS & OFFLINE CANVAS JS)
//    Served completely from ESP32 Flash Memory (PROGMEM). Zero CDNs. 100% Offline.
// ======================================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Flight Acceleration Telemetry</title>
  <style>
    :root {
      --bg-space: #090d16;
      --card-bg: rgba(16, 23, 42, 0.82);
      --card-border: rgba(56, 189, 248, 0.18);
      --cyan: #00f2fe;
      --blue: #38bdf8;
      --emerald: #10b981;
      --amber: #f59e0b;
      --crimson: #ef4444;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
    }
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    }
    body {
      background: radial-gradient(circle at 50% 0%, #172554 0%, var(--bg-space) 75%);
      color: var(--text-main);
      min-height: 100vh;
      padding: 16px;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    /* Header */
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 12px;
      padding-bottom: 16px;
      margin-bottom: 20px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.08);
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .pulse-radar {
      width: 14px;
      height: 14px;
      background: var(--cyan);
      border-radius: 50%;
      box-shadow: 0 0 12px var(--cyan);
      animation: radarPulse 1.6s infinite ease-out;
    }
    @keyframes radarPulse {
      0% { transform: scale(0.9); opacity: 1; box-shadow: 0 0 4px var(--cyan); }
      70% { transform: scale(1.6); opacity: 0.3; box-shadow: 0 0 16px var(--cyan); }
      100% { transform: scale(0.9); opacity: 1; }
    }
    h1 {
      font-size: 1.45rem;
      font-weight: 700;
      letter-spacing: 0.5px;
      background: linear-gradient(90deg, #38bdf8, #a855f7);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .tagline {
      font-size: 0.8rem;
      color: var(--text-muted);
    }
    .header-actions {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .btn {
      background: rgba(56, 189, 248, 0.15);
      border: 1px solid var(--card-border);
      color: #38bdf8;
      padding: 8px 14px;
      font-size: 0.82rem;
      border-radius: 8px;
      cursor: pointer;
      transition: all 0.2s ease;
      font-weight: 600;
    }
    .btn:hover {
      background: rgba(56, 189, 248, 0.3);
      box-shadow: 0 0 12px rgba(56, 189, 248, 0.4);
    }
    /* Status Banner */
    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 6px 14px;
      border-radius: 9999px;
      font-size: 0.85rem;
      font-weight: 700;
      letter-spacing: 0.5px;
      text-transform: uppercase;
      background: rgba(16, 185, 129, 0.15);
      color: var(--emerald);
      border: 1px solid rgba(16, 185, 129, 0.4);
      transition: all 0.3s ease;
    }
    .status-badge.status-accelerating {
      background: rgba(245, 158, 11, 0.18);
      color: var(--amber);
      border-color: rgba(245, 158, 11, 0.5);
    }
    .status-badge.status-freefall {
      background: rgba(56, 189, 248, 0.2);
      color: var(--cyan);
      border-color: rgba(56, 189, 248, 0.5);
    }
    .status-badge.status-impact {
      background: rgba(239, 68, 68, 0.25);
      color: var(--crimson);
      border-color: rgba(239, 68, 68, 0.7);
      animation: impactBlink 0.35s infinite alternate;
    }
    @keyframes impactBlink {
      from { box-shadow: 0 0 4px var(--crimson); }
      to { box-shadow: 0 0 20px var(--crimson); }
    }
    /* Grid Layout */
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 16px;
      margin-bottom: 20px;
    }
    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 18px;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
      backdrop-filter: blur(10px);
      transition: transform 0.2s ease, border-color 0.2s ease;
    }
    .card:hover {
      transform: translateY(-2px);
      border-color: rgba(56, 189, 248, 0.35);
    }
    .card-title {
      font-size: 0.78rem;
      text-transform: uppercase;
      letter-spacing: 1px;
      color: var(--text-muted);
      margin-bottom: 8px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .primary-metric {
      font-size: 2.3rem;
      font-weight: 800;
      color: var(--text-main);
      line-height: 1.1;
      display: flex;
      align-items: baseline;
      gap: 6px;
    }
    .primary-metric span.unit {
      font-size: 1rem;
      font-weight: 500;
      color: var(--blue);
    }
    .sub-metric {
      font-size: 0.85rem;
      color: var(--text-muted);
      margin-top: 6px;
    }
    /* Mini Bars for XYZ */
    .axis-group {
      margin-top: 14px;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .axis-row {
      display: grid;
      grid-template-columns: 24px 1fr 65px;
      align-items: center;
      gap: 8px;
      font-size: 0.82rem;
    }
    .axis-tag {
      font-weight: 700;
    }
    .bar-track {
      background: rgba(255, 255, 255, 0.08);
      height: 8px;
      border-radius: 4px;
      overflow: hidden;
      position: relative;
    }
    .bar-fill {
      height: 100%;
      width: 50%;
      border-radius: 4px;
      transition: width 0.15s ease, background 0.2s ease;
    }
    .fill-x { background: linear-gradient(90deg, #ec4899, #f43f5e); }
    .fill-y { background: linear-gradient(90deg, #10b981, #06b6d4); }
    .fill-z { background: linear-gradient(90deg, #3b82f6, #6366f1); }
    /* Horizon Attitude Indicator */
    .attitude-box {
      display: flex;
      align-items: center;
      justify-content: space-around;
      margin-top: 12px;
      padding: 10px;
      background: rgba(0, 0, 0, 0.25);
      border-radius: 8px;
      border: 1px solid rgba(255, 255, 255, 0.05);
    }
    .att-item {
      text-align: center;
    }
    .att-val {
      font-size: 1.35rem;
      font-weight: 700;
      color: #38bdf8;
    }
    .att-lbl {
      font-size: 0.72rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.8px;
    }
    /* Chart Section */
    .chart-container {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 18px;
      margin-bottom: 20px;
    }
    .chart-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
    }
    .chart-header h3 {
      font-size: 0.95rem;
      font-weight: 600;
      letter-spacing: 0.4px;
      color: var(--text-main);
    }
    canvas {
      width: 100%;
      height: 220px;
      display: block;
      border-radius: 8px;
      background: rgba(0, 0, 0, 0.35);
    }
    /* Footer */
    footer {
      display: flex;
      justify-content: space-between;
      flex-wrap: wrap;
      gap: 10px;
      font-size: 0.75rem;
      color: var(--text-muted);
      padding-top: 16px;
      border-top: 1px solid rgba(255, 255, 255, 0.06);
    }
    .footer-highlight {
      color: #38bdf8;
      font-weight: 600;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="brand">
        <div class="pulse-radar"></div>
        <div>
          <h1>ESP32 Flight Acceleration Monitor</h1>
          <div class="tagline">Real-Time Inertial Telemetry & Aerodynamic Dynamics Engine</div>
        </div>
      </div>
      <div class="header-actions">
        <span id="flight-status" class="status-badge">Idle</span>
        <button class="btn" onclick="resetPeak()">Reset Peak G</button>
      </div>
    </header>

    <!-- Top Metrics Grid -->
    <div class="grid">
      <!-- Total G-Force Card -->
      <div class="card">
        <div class="card-title">
          <span>Net Acceleration</span>
          <span style="color: #38bdf8;">|G|</span>
        </div>
        <div class="primary-metric">
          <span id="val-gtotal">1.00</span>
          <span class="unit">g</span>
        </div>
        <div class="sub-metric">
          Equivalent: <strong id="val-net-mps2" style="color: #f8fafc;">9.81</strong> m/s²
        </div>
      </div>

      <!-- Peak G-Force Card -->
      <div class="card">
        <div class="card-title">
          <span>Peak G Recorded</span>
          <span style="color: #f59e0b;">MAX</span>
        </div>
        <div class="primary-metric">
          <span id="val-peakg" style="color: #f59e0b;">1.00</span>
          <span class="unit">g</span>
        </div>
        <div class="sub-metric">
          Limit Envelope: <span style="color: #10b981;">±16.0 g</span> (ADXL345 Full Scale)
        </div>
      </div>

      <!-- RMS Vibration Card -->
      <div class="card">
        <div class="card-title">
          <span>Vibration Noise (RMS)</span>
          <span style="color: #a855f7;">DYNAMICS</span>
        </div>
        <div class="primary-metric">
          <span id="val-vib" style="color: #a855f7;">0.02</span>
          <span class="unit">g</span>
        </div>
        <div class="sub-metric" id="vib-desc">
          Airframe Stability: <span style="color: #10b981;">Smooth / Clean</span>
        </div>
      </div>

      <!-- Attitude Pitch & Roll Card -->
      <div class="card">
        <div class="card-title">
          <span>Attitude (Estimated)</span>
          <span style="color: #06b6d4;">EULER</span>
        </div>
        <div class="attitude-box">
          <div class="att-item">
            <div id="val-pitch" class="att-val">0.0°</div>
            <div class="att-lbl">Pitch (θ)</div>
          </div>
          <div class="att-item" style="border-left: 1px solid rgba(255,255,255,0.1); padding-left: 14px;">
            <div id="val-roll" class="att-val">0.0°</div>
            <div class="att-lbl">Roll (φ)</div>
          </div>
        </div>
        <div class="sub-metric" style="text-align: center; margin-top: 8px;">
          Gravity-vector Euler decomposition
        </div>
      </div>
    </div>

    <!-- 3-Axis Breakdown Card -->
    <div class="card" style="margin-bottom: 20px;">
      <div class="card-title">
        <span>Tri-Axial Acceleration Decomposition</span>
        <span style="color: #94a3b8;">X / Y / Z Vector</span>
      </div>
      <div class="axis-group">
        <!-- Axis X -->
        <div class="axis-row">
          <span class="axis-tag" style="color: #f43f5e;">AX</span>
          <div class="bar-track">
            <div id="bar-x" class="bar-fill fill-x"></div>
          </div>
          <span id="val-gx" style="text-align: right; font-weight: 600;">0.00 g</span>
        </div>
        <!-- Axis Y -->
        <div class="axis-row">
          <span class="axis-tag" style="color: #10b981;">AY</span>
          <div class="bar-track">
            <div id="bar-y" class="bar-fill fill-y"></div>
          </div>
          <span id="val-gy" style="text-align: right; font-weight: 600;">0.00 g</span>
        </div>
        <!-- Axis Z -->
        <div class="axis-row">
          <span class="axis-tag" style="color: #3b82f6;">AZ</span>
          <div class="bar-track">
            <div id="bar-z" class="bar-fill fill-z"></div>
          </div>
          <span id="val-gz" style="text-align: right; font-weight: 600;">1.00 g</span>
        </div>
      </div>
    </div>

    <!-- Real-time Canvas Line Chart -->
    <div class="chart-container">
      <div class="chart-header">
        <h3>Live G-Force History (Last 50 Telemetry Frames)</h3>
        <span id="rate-badge" style="font-size: 0.76rem; color: #38bdf8;">Stream: 3.3 Hz (300ms)</span>
      </div>
      <canvas id="telemetryChart"></canvas>
    </div>

    <footer>
      <div>System: <span class="footer-highlight">ESP32 + ADXL345 (I2C 0x53)</span> | Protocol: <span class="footer-highlight">HTTP REST JSON</span></div>
      <div>Telemetry Buffer: 50 Samples | Status: <span id="conn-status" style="color: #10b981;">Connected (Live)</span></div>
    </footer>
  </div>

  <!-- Real-Time Telemetry Script -->
  <script>
    const MAX_POINTS = 50;
    const gHistory = new Array(MAX_POINTS).fill(1.0);
    const canvas = document.getElementById('telemetryChart');
    const ctx = canvas.getContext('2d');

    // Resize canvas for device pixel ratio
    function setupCanvasDpi() {
      const rect = canvas.getBoundingClientRect();
      const dpr = window.devicePixelRatio || 1;
      canvas.width = rect.width * dpr;
      canvas.height = rect.height * dpr;
      ctx.scale(dpr, dpr);
    }
    window.addEventListener('resize', setupCanvasDpi);
    setupCanvasDpi();

    // Chart Rendering
    function drawChart() {
      const rect = canvas.getBoundingClientRect();
      const w = rect.width;
      const h = rect.height;

      ctx.clearRect(0, 0, w, h);

      // Determine dynamic range (at least 0 to 2.5g)
      let minVal = 0.0;
      let maxVal = 2.5;
      for (let v of gHistory) {
        if (v > maxVal) maxVal = Math.ceil(v * 1.25);
      }

      // Draw Grid Lines & Labels
      ctx.strokeStyle = "rgba(255, 255, 255, 0.08)";
      ctx.lineWidth = 1;
      ctx.fillStyle = "#64748b";
      ctx.font = "10px sans-serif";

      const gridSteps = 4;
      for (let i = 0; i <= gridSteps; i++) {
        let yNorm = i / gridSteps;
        let yPos = h - (yNorm * (h - 24)) - 12;
        let labelVal = (minVal + yNorm * (maxVal - minVal)).toFixed(1);

        ctx.beginPath();
        ctx.moveTo(35, yPos);
        ctx.lineTo(w, yPos);
        ctx.stroke();

        ctx.fillText(labelVal + "g", 6, yPos + 3);
      }

      // Draw Nominal 1.0g Reference Line
      const nominalY = h - (((1.0 - minVal) / (maxVal - minVal)) * (h - 24)) - 12;
      ctx.strokeStyle = "rgba(56, 189, 248, 0.28)";
      ctx.setLineDash([4, 4]);
      ctx.beginPath();
      ctx.moveTo(35, nominalY);
      ctx.lineTo(w, nominalY);
      ctx.stroke();
      ctx.setLineDash([]);

      // Draw Telemetry Line & Gradient Area
      const stepX = (w - 45) / (MAX_POINTS - 1);
      ctx.beginPath();
      for (let i = 0; i < MAX_POINTS; i++) {
        let val = gHistory[i];
        let px = 40 + i * stepX;
        let py = h - (((val - minVal) / (maxVal - minVal)) * (h - 24)) - 12;
        if (i === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
      }

      // Glowing Stroke
      ctx.strokeStyle = "#00f2fe";
      ctx.lineWidth = 2.2;
      ctx.shadowColor = "#00f2fe";
      ctx.shadowBlur = 10;
      ctx.stroke();
      ctx.shadowBlur = 0; // Reset shadow

      // Draw Current Head Point
      let lastX = 40 + (MAX_POINTS - 1) * stepX;
      let lastVal = gHistory[MAX_POINTS - 1];
      let lastY = h - (((lastVal - minVal) / (maxVal - minVal)) * (h - 24)) - 12;

      ctx.beginPath();
      ctx.arc(lastX, lastY, 4.5, 0, 2 * Math.PI);
      ctx.fillStyle = "#ffffff";
      ctx.fill();
      ctx.strokeStyle = "#00f2fe";
      ctx.lineWidth = 2;
      ctx.stroke();
    }

    // Map helper for axis bars (-2g to +2g mapped to 0% - 100%)
    function mapBar(gVal) {
      let clamped = Math.max(-2.0, Math.min(2.0, gVal));
      let pct = ((clamped + 2.0) / 4.0) * 100;
      return pct.toFixed(1) + "%";
    }

    // Live Fetch Routine
    let failCount = 0;
    function fetchTelemetry() {
      fetch('/data')
        .then(res => {
          if (!res.ok) throw new Error("HTTP error " + res.status);
          return res.json();
        })
        .then(data => {
          failCount = 0;
          document.getElementById('conn-status').innerText = "Connected (Live)";
          document.getElementById('conn-status').style.color = "#10b981";

          // Numeric Updates
          document.getElementById('val-gtotal').innerText = data.gTotal.toFixed(2);
          document.getElementById('val-net-mps2').innerText = (data.gTotal * 9.80665).toFixed(2);
          document.getElementById('val-peakg').innerText = data.peakG.toFixed(2);
          document.getElementById('val-vib').innerText = data.vibrationRMS.toFixed(2);
          document.getElementById('val-pitch').innerText = data.pitch.toFixed(1) + "°";
          document.getElementById('val-roll').innerText = data.roll.toFixed(1) + "°";

          // Axis Values
          document.getElementById('val-gx').innerText = data.gX.toFixed(2) + " g (" + data.ax.toFixed(1) + " m/s²)";
          document.getElementById('val-gy').innerText = data.gY.toFixed(2) + " g (" + data.ay.toFixed(1) + " m/s²)";
          document.getElementById('val-gz').innerText = data.gZ.toFixed(2) + " g (" + data.az.toFixed(1) + " m/s²)";

          // Axis Visual Bars
          document.getElementById('bar-x').style.width = mapBar(data.gX);
          document.getElementById('bar-y').style.width = mapBar(data.gY);
          document.getElementById('bar-z').style.width = mapBar(data.gZ);

          // Vibration Description
          let vDesc = document.getElementById('vib-desc');
          if (data.vibrationRMS < 0.10) {
            vDesc.innerHTML = 'Airframe Stability: <span style="color: #10b981;">Smooth / Clean</span>';
          } else if (data.vibrationRMS < 0.35) {
            vDesc.innerHTML = 'Airframe Stability: <span style="color: #f59e0b;">Moderate Turbulence</span>';
          } else {
            vDesc.innerHTML = 'Airframe Stability: <span style="color: #ef4444;">Severe Vibration / Buffet</span>';
          }

          // Flight Status Badge
          let statusBadge = document.getElementById('flight-status');
          statusBadge.innerText = data.status;
          statusBadge.className = 'status-badge';
          if (data.status === "Accelerating") statusBadge.classList.add('status-accelerating');
          else if (data.status === "Free-fall") statusBadge.classList.add('status-freefall');
          else if (data.status === "Impact") statusBadge.classList.add('status-impact');

          // Append to chart history
          gHistory.shift();
          gHistory.push(data.gTotal);
          drawChart();
        })
        .catch(err => {
          failCount++;
          if (failCount > 3) {
            document.getElementById('conn-status').innerText = "Reconnecting...";
            document.getElementById('conn-status').style.color = "#ef4444";
          }
        });
    }

    // Reset Peak Action
    function resetPeak() {
      fetch('/reset_peak')
        .then(() => {
          document.getElementById('val-peakg').innerText = "1.00";
        })
        .catch(err => console.error("Reset failed", err));
    }

    // Telemetry Polling Loop (Every 300ms = ~3.3 updates/second)
    setInterval(fetchTelemetry, 300);
    fetchTelemetry();
  </script>
</body>
</html>
)rawliteral";

// ======================================================================================
// 4. INERTIAL DYNAMICS & SENSOR ACQUISITION ENGINE
// ======================================================================================
void setupSensor() {
  Serial.println(F("[ADXL345] Initializing I2C bus on GPIO 21 (SDA) & GPIO 22 (SCL)..."));
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000); // 400kHz Fast I2C

  if (!accel.begin()) {
    Serial.println(F("[ERROR] ADXL345 sensor NOT detected! Check 3.3V, GND, SDA, SCL connections."));
    sensorAvailable = false;
  } else {
    sensorAvailable = true;
    Serial.println(F("[ADXL345] Sensor successfully identified."));

    // Set full dynamic measurement range to +/- 16g
    accel.setRange(ADXL345_RANGE_16_G);
    accel.setDataRate(ADXL345_DATARATE_100_HZ);

    // Display sensor capabilities in Serial Monitor
    sensor_t sensorDetails;
    accel.getSensor(&sensorDetails);
    Serial.print  (F("Sensor Name: ")); Serial.println(sensorDetails.name);
    Serial.print  (F("Max Value:   ")); Serial.print(sensorDetails.max_value); Serial.println(F(" m/s^2"));
    Serial.print  (F("Min Value:   ")); Serial.print(sensorDetails.min_value); Serial.println(F(" m/s^2"));
    Serial.print  (F("Resolution:  ")); Serial.print(sensorDetails.resolution); Serial.println(F(" m/s^2"));
  }

  // Pre-fill circular buffer with nominal 1.0g
  for (int i = 0; i < VIBRATION_BUFFER_SIZE; i++) {
    gHistory[i] = 1.0f;
  }
}

void sampleInertialData() {
  if (!sensorAvailable) {
    // Graceful fallback simulation if physical sensor is detached for student testing
    gX = 0.05f; gY = -0.02f; gZ = 0.98f;
    accX_mps2 = gX * GRAVITY_STANDARD;
    accY_mps2 = gY * GRAVITY_STANDARD;
    accZ_mps2 = gZ * GRAVITY_STANDARD;
    gTotal = 1.0f;
    pitchDeg = 0.0f;
    rollDeg = 0.0f;
    vibrationRMS = 0.01f;
    currentStatus = STATUS_IDLE;
    return;
  }

  sensors_event_t event;
  accel.getEvent(&event);

  // Read raw acceleration in m/s^2
  accX_mps2 = event.acceleration.x;
  accY_mps2 = event.acceleration.y;
  accZ_mps2 = event.acceleration.z;

  // Convert to dimensionless G-force units (1 g = 9.80665 m/s^2)
  gX = accX_mps2 / GRAVITY_STANDARD;
  gY = accY_mps2 / GRAVITY_STANDARD;
  gZ = accZ_mps2 / GRAVITY_STANDARD;

  // Calculate Euclidean total G-force magnitude: |G| = sqrt(gx^2 + gy^2 + gz^2)
  gTotal = sqrtf((gX * gX) + (gY * gY) + (gZ * gZ));

  // Track Peak G-force
  if (gTotal > peakG) {
    peakG = gTotal;
  }

  // Approximate Euler Attitude (Pitch and Roll in degrees)
  // Roll (phi) = atan2(Ay, Az) * (180 / PI)
  rollDeg = atan2f(accY_mps2, accZ_mps2) * (180.0f / M_PI);
  
  // Pitch (theta) = atan2(-Ax, sqrt(Ay^2 + Az^2)) * (180 / PI)
  pitchDeg = atan2f(-accX_mps2, sqrtf((accY_mps2 * accY_mps2) + (accZ_mps2 * accZ_mps2))) * (180.0f / M_PI);

  // Moving Window RMS Vibration Calculation
  gHistory[gHistoryIndex] = gTotal;
  gHistoryIndex = (gHistoryIndex + 1) % VIBRATION_BUFFER_SIZE;
  if (gHistoryIndex == 0) gHistoryFilled = true;

  int count = gHistoryFilled ? VIBRATION_BUFFER_SIZE : gHistoryIndex;
  if (count < 2) count = 2;

  float sum = 0.0f;
  for (int i = 0; i < count; i++) {
    sum += gHistory[i];
  }
  float meanG = sum / count;

  float varianceSum = 0.0f;
  for (int i = 0; i < count; i++) {
    float diff = gHistory[i] - meanG;
    varianceSum += (diff * diff);
  }
  vibrationRMS = sqrtf(varianceSum / count);

  // Flight State Machine
  unsigned long now = millis();
  if (gTotal >= 3.0f) {
    currentStatus = STATUS_IMPACT;
    lastImpactTime = now;
  } else if ((now - lastImpactTime) < IMPACT_HOLD_TIME_MS) {
    currentStatus = STATUS_IMPACT; // Latch impact for visual confirmation
  } else if (gTotal < 0.25f) {
    currentStatus = STATUS_FREEFALL;
  } else if (gTotal > 1.35f) {
    currentStatus = STATUS_ACCELERATING;
  } else {
    currentStatus = STATUS_IDLE;
  }
}

// Convert FlightStatus enum to String for JSON output
const char* getStatusString(FlightStatus status) {
  switch (status) {
    case STATUS_ACCELERATING: return "Accelerating";
    case STATUS_FREEFALL:     return "Free-fall";
    case STATUS_IMPACT:       return "Impact";
    case STATUS_IDLE:
    default:                  return "Idle";
  }
}

// ======================================================================================
// 5. HTTP SERVER REST ENDPOINTS
// ======================================================================================

// Serve Primary Web Dashboard from PROGMEM
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// Serve Live Telemetry JSON endpoint (/data)
void handleData() {
  char jsonBuffer[384];
  snprintf(jsonBuffer, sizeof(jsonBuffer),
    "{"
    "\"ax\":%.2f,"
    "\"ay\":%.2f,"
    "\"az\":%.2f,"
    "\"gX\":%.2f,"
    "\"gY\":%.2f,"
    "\"gZ\":%.2f,"
    "\"gTotal\":%.2f,"
    "\"peakG\":%.2f,"
    "\"pitch\":%.1f,"
    "\"roll\":%.1f,"
    "\"vibrationRMS\":%.2f,"
    "\"status\":\"%s\""
    "}",
    accX_mps2, accY_mps2, accZ_mps2,
    gX, gY, gZ,
    gTotal, peakG,
    pitchDeg, rollDeg,
    vibrationRMS,
    getStatusString(currentStatus)
  );

  // Enable CORS & No-Cache Headers for live telemetry streaming
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", jsonBuffer);
}

// Reset Peak G Telemetry (/reset_peak)
void handleResetPeak() {
  peakG = gTotal;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "OK");
}

// 404 Fallback
void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found on ESP32 Flight Telemetry Server");
}

// ======================================================================================
// 6. WI-FI STATION SETUP ROUTINE
// ======================================================================================
void setupWiFi() {
  Serial.println(F("\n=========================================="));
  Serial.print(F("[WiFi] Connecting in Station mode to SSID: "));
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 30) {
    delay(500);
    Serial.print(F("."));
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n[WiFi] Connection Established!"));
    Serial.print(F("[WiFi] ESP32 Assigned IP Address: http://"));
    Serial.println(WiFi.localIP());
    Serial.print(F("[WiFi] Signal Strength (RSSI): "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
  } else {
    Serial.println(F("\n[WARNING] Wi-Fi connection timeout!"));
    Serial.println(F("[WARNING] Check SSID/Password or verify your Phone Hotspot is active."));
    Serial.println(F("[WiFi] Falling back to Access Point (AP) mode: ESP32-Flight-Monitor"));
    
    // Fallback AP mode so students are never locked out!
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Flight-Monitor", "12345678");
    Serial.print(F("[AP Mode] Connect to 'ESP32-Flight-Monitor' and open http://"));
    Serial.println(WiFi.softAPIP());
  }
  Serial.println(F("==========================================\n"));
}

// ======================================================================================
// 7. ARDUINO SETUP & SUPER-LOOP
// ======================================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("\n***************************************************"));
  Serial.println(F("  ESP32 FLIGHT ACCELERATION & TELEMETRY MONITOR   "));
  Serial.println(F("      IoT Project-Based Learning (Sem 5)          "));
  Serial.println(F("***************************************************\n"));

  // 1. Initialize ADXL345
  setupSensor();

  // 2. Connect to Wi-Fi
  setupWiFi();

  // 3. Register Web Server Route Handlers
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/reset_peak", HTTP_GET, handleResetPeak);
  server.onNotFound(handleNotFound);

  // 4. Start HTTP Server
  server.begin();
  Serial.println(F("[HTTP] Flight Telemetry Web Server started successfully!"));
  Serial.println(F("[HTTP] Open the IP address above in your browser to view the dashboard.\n"));
}

void loop() {
  // Handle incoming HTTP client requests
  server.handleClient();

  // Non-blocking sensor sampling every SENSOR_SAMPLE_INTERVAL_MS
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorReadTime >= SENSOR_SAMPLE_INTERVAL_MS) {
    lastSensorReadTime = currentMillis;
    sampleInertialData();

    // Periodic telemetry dump to Serial Monitor (every ~1 sec)
    static unsigned long lastSerialPrint = 0;
    if (currentMillis - lastSerialPrint >= 1000) {
      lastSerialPrint = currentMillis;
      Serial.printf("[TELEMETRY] |G|: %.2fg | Peak: %.2fg | Pitch: %.1f deg | Roll: %.1f deg | VibRMS: %.2fg | Status: %s\n",
                    gTotal, peakG, pitchDeg, rollDeg, vibrationRMS, getStatusString(currentStatus));
    }
  }
}
