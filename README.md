# 📊 RuuviTag Offline Field Multi-Logger & Dashboard

A robust, production-ready **ESP32-C3** data logging firmware that captures telemetry from up to three **RuuviTag BLE Sensors** simultaneously. Designed specifically for off-the-grid field applications, this system operates entirely **without internet access or cellular data**, serving an interactive dark-themed dashboard over a local Wi-Fi Access Point (AP).

---

## ✨ Features

* **True Network Independence:** 100% operational off-grid. Dashboard layout, graphics, and timeline tables require absolutely zero internet connectivity or third-party script downloads.
* **Non-Blocking Dual-Core Architecture:** Runs on a dedicated **FreeRTOS Background Worker Task** thread. This separates the BLE radio sniffer from the Wi-Fi stack, completely eliminating network dropouts or freezes during sensor scans.
* **Smart Power-Loss Recovery:** Automatically saves all sensor labels, targeted MAC configurations, and tracking historical entries straight into the ESP32-C3's internal **Non-Volatile Flash Memory (Preferences)**. If power cuts out, data is instantly restored at boot.
* **Endless Queue Storage Loop (FIFO):** Data continuously stacks beyond the 10-hour mark. Once the internal memory allocation threshold is reached, it auto-purges the oldest historical row to inject fresh tracking points indefinitely.
* **Dynamic Offline CSS Trend Graphing:** Features an offline-native visual chart engine using pure CSS grids that dynamically scale and map Fahrenheit temperature trends on your phone's screen.
* **Fahrenheit Native Engine:** Direct binary-level conversion of Ruuvi raw hex payloads into Fahrenheit values out of the box.
* **Wireless Firmware Management:** Build updates inside VS Code and upload them safely over-the-air (OTA) through your phone's browser—no USB cables needed after the initial flash.

---

## 🏗️ Hardware Architecture & Pin Out

```text
       📡 [ RuuviTag 1 ]          📡 [ RuuviTag 2 ]          📡 [ RuuviTag 3 ]
               │                          │                          │
               └───────────────── BLE 5.0 Broadcast ─────────────────┘
                                          │
                                          ▼ (Sniffs Raw BLE Advertisements)
                               ┌─────────────────────┐
                               │  ESP32-C3 dev board │
                               └─────────────────────┘
                                          │
                                          ▼ (Generates Independent Local Network)
                                    📶 [ WiFi AP ]
                                          │
                       ┌──────────────────┴──────────────────┐
                       ▼                                     ▼
               📱 Smart Phone                        💻 Field Laptop
             (Web: 192.168.4.1)                    (Web: 192.168.4.1)
```

### Hardware Requirements
* **Microcontroller:** Any standard 4MB ESP32-C3 development board (e.g., ESP32-C3 Super Mini or DevKitM-1).
* **Sensors:** 1 to 3 RuuviTag environmental monitors running default Data Format 5 firmware.
* **Power Source:** A common 5V USB power bank or utility battery brick.

---

## 🛠️ Software Setup (VS Code & PlatformIO)

### 1. Project Configuration Mapping
Create or overwrite your local project's **`platformio.ini`** file with the working configuration mapping configuration below:

```ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
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

### 2. Custom Partition Scheme Setup
To avoid file-size capacity errors, create a file named **`partitions.csv`** directly in the root directory folder of your project (the same level as `platformio.ini`) and insert the text block below:

```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1C0000,
app1,     app,  ota_1,   0x1D0000,0x1C0000,
spiffs,   data, spiffs,  0x390000,0x60000,
```

### 3. Deploy Source Code
Paste your code into your local **`src/main.cpp`** layout configuration workspace file, then build and flash via USB by pressing `Ctrl + Alt + B` followed by the PlatformIO Upload button.

---

## 📲 How To Use in the Field

1. **Connect:** Power your ESP32-C3 unit via a USB battery bank out in the field.
2. **Join Network:** Open your mobile device's Wi-Fi panel and connect to the local access network:
   * **SSID:** `Ruuvi_Multi_Logger`
   * **Password:** `password123`
3. **Open Panel:** Fire up your web browser and navigate directly to this IP address: `192.168.4.1`
4. **Discover Tags:** Scroll to the **Local Device Discovery Tool** card section and hit **Scan for Nearby RuuviTags**. The board will sniff out all nearby tags and output their specific MAC addresses.
5. **Map & Name:** Copy the green code MAC string values, paste them into your desired Slot profile input configurations box below, give them customized entity names (e.g., `Deep Freezer`, `Lab Vent`), and press **Save Sensor Mapping Settings**.
6. **Track Data:** The page will refresh and begin parsing live telemetry variables completely automatically every 1.5 seconds. A history milestone row will write directly to the persistent flash timeline queue every 5 minutes.

---

## 🗃️ Memory and System Control

* **Wipe Log Cache:** Clicking the crimson **Wipe Saved Log** button safely completely resets the flash database partition arrays without losing your mapped names or MAC address configurations.
* **Over-The-Air Update:** When deploying new code modifications in VS Code, go to `.pio > build > esp32-c3-devkitm-1`, grab `firmware.bin`, and upload it right through the bottom panel card on the wireless dashboard to safely update system logic on the fly.
