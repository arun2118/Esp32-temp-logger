# 📊 Standalone Wireless Field Multi-Logger & Thermodynamic Dashboard

A robust, field-tested data logging firmware for **ESP32 series microcontrollers** designed to passively capture environmental telemetry from up to three **Bluetooth Low Energy (BLE) broadcasting sensors** simultaneously. 

Engineered specifically for off-grid operations, troubleshooting industrial insulation failures, cold-storage units, or condensation leaks, this system runs entirely **without internet access or cellular signals**. It acts as a standalone appliance, serving a local, dark-themed diagnostic panel over an independent Wi-Fi Access Point (AP).

---

## ✨ Features

* **Complete Grid Independence:** 100% self-contained. The responsive web layout, trend charts, and chronological timeline logs require absolutely zero internet connectivity or third-party cloud script downloads.
* **Universal BLE Advertisement Sniffing:** Passively intercepts open, asynchronous Bluetooth LE beacon broadcasts from environmental sensors nearby. No complex pairing protocols or active handshake connections required.
* **Real-Time Magnus-Tetens Dew Point Calculations:** The firmware dynamically computes the exact thermodynamic dew point for each individual sensor location. This lets you track condensation vulnerability directly on-screen by monitoring how close ambient temperatures are to hitting the local saturation threshold (°Dp).
* **Non-Blocking Dual-Core FreeRTOS Architecture:** Separates the BLE radio sniffer task from the web server handling. A dedicated background task engine wake-sniffs the airwaves at scheduled intervals, keeping the Wi-Fi AP solid, fast, and fully responsive 100% of the time.
* **Persistent Power-Loss Safe Recovery:** Leverages a micro-split partition implementation of the ESP32's native non-volatile flash file framework (`Preferences.h`). Mapped sensor names, MAC targets, and accumulated history tracks persist seamlessly across total battery or power disconnects.
* **Endless Queue Circular Buffer Logging (FIFO):** Data continuously stacks across a massive rolling logging timeline. When the memory footprint allocation is exhausted, it auto-purges the single oldest log entry row to seamlessly insert fresh data points indefinitely.
* **Local CSV Spreadsheet Export Engine:** Incorporates an ultra-efficient chunked file streamer path. Click the download interface button to immediately pull a `.csv` timeline format log directly to your phone or laptop for advanced analytical modeling inside **Google Sheets** or **Microsoft Excel**.
* **Zero-Reading Data Filtering:** Features raw packet sanity validations that automatically filter out uninitialized or dead signal packets, preventing zero-blocks (0.0°F / 0%) from writing into your persistent flash cache timelines.
* **Wireless Over-The-Air Management (OTA):** Once compiled in VS Code, code changes can be deployed wirelessly through the embedded update component card on your mobile browser.

---

## 🏗️ System Overview

```text
       📡 [ BLE Sensor 1 ]             📡 [ BLE Sensor 2 ]             📡 [ BLE Sensor 3 ]
                │                               │                               │
                └────────────────────── BLE Beacon Wave ────────────────────────┘
                                                │
                                                ▼ (Passive Broadcast Sniffing)
                                     ┌────────────────────┐
                                     │  ESP32 Board Hub   │
                                     └────────────────────┘
                                                │
                                                ▼ (Generates Independent Local Network)
                                          📶 [ WiFi AP ]
                                                │
                             ui─────────────────┴──────────────────┐
                             ▼                                     ▼
                     📱 Smart Phone                        💻 Field Laptop
                   (Web: 192.168.4.1)                    (Web: 192.168.4.1)
```

---

## 🛠️ Software Deployment (VS Code & PlatformIO)

### 1. Project Configuration Layout
Configure your local project **`platformio.ini`** deployment script file utilizing the environment variables mapped below:

```ini
; PlatformIO Project Configuration File for ESP32 Local BLE Data Hub
[env:esp32-field-logger]
platform = espressif32
board = your_esp32_board_id ; Match this to your exact ESP32 hardware variant
framework = arduino
monitor_speed = 115200
build_flags = 
    -D CORE_DEBUG_LEVEL=3 
    -D ARDUINO_USB_MODE=1 
    -D ARDUINO_USB_CDC_ON_BOOT=1
board_build.partitions = partitions.csv

lib_deps =
    h2zero/NimBLE-Arduino @ ^1.4.1
```

### 2. Layout Partitions CSV Allocation
To preserve a dedicated sector for wireless dual-slot firmware transfers right beside the persistent storage preferences directory blocks, create a configuration file named **`partitions.csv`** directly inside your project's root folder:

```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1C0000,
app1,     app,  ota_1,   0x1D0000,0x1C0000,
spiffs,   data, spiffs,  0x390000,0x60000,
```

### 3. Flash Code Setup
Paste the multi-part C++ code block patterns cleanly into your local **`src/main.cpp`** workspace directory file. Trigger the compilation routine via `Ctrl + Alt + B` and upload via your physical USB data cable path.

---

## 📲 Operating Instructions

1. **Powering On:** Plug your programmed ESP32 unit into a portable 5V utility USB power bank or mobile battery asset pack.
2. **Establishing Connection:** Open your phone or laptop's network tray panel and join the secure microchip network link node:
   * **Network Name (SSID):** `Ruuvi_Multi_Logger`
   * **Network Security Key:** `password123`
3. **Navigating To Dashboard:** Open any native mobile web browser (Chrome, Safari, Firefox) and input this URL path direct address target: `192.168.4.1`
4. **Discovering Active Sensors:** Scroll down panel views until you locate the **Local Device Discovery Tool** block card. Click **Scan for Nearby RuuviTags**. The board radio will pause to intercept the raw airwave structures and display every active nearby tag's green MAC string.
5. **Configuring Matrix Target Profiles:** Copy the green code MAC address values, click and insert them into the desired **Profile Mapping Configuration** slot block field lines, provide a targeted label (e.g., `Lexan Door Interior`, `Condensation Leak Hole`), and press **Save Sensor Mapping Settings**.
6. **Logging Execution Timeline:** The web engine dashboard will refresh immediately and begin parsing incoming telemetry variations smoothly every 1.5 seconds. Valid snapshot data rows will write permanently into the rolling flash table cache every 5 minutes.

---

## 📈 Analyzing Data in Google Sheets / Excel

1. Inside your offline dashboard screen view, scroll downward to the **Memory Management** action container box card.
2. Tap the green **Download CSV Log** action link button. The ESP32 will immediately process the internal log rows out of flash memory and stream a native download file called `ruuvi_field_log.csv`.
3. Open a clean tracking canvas spreadsheet profile workbook workspace inside **Google Sheets** or **Excel**.
4. Go to **`File > Import > Upload`**, select your local `ruuvi_field_log.csv` file trace, set *Import Location* options parameters targets values to **Replace spreadsheet**, and apply data inserts tracking properties.
5. Highlight your custom columns data trace logs tracking arrays, choose **`Insert > Chart`**, and pick your preferred graphical mapping elements structure (such as **Line Chart** or **Area Graph**) to generate highly accurate environmental thermal analytics curves.

