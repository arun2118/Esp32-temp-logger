#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <NimBLEDevice.h>

// --- AP Credentials ---
const char* apSSID = "Ruuvi_Multi_Logger";
const char* apPassword = "password123"; 

WebServer server(80);
NimBLEScan* pBLEScan;

// --- Multi-Sensor Configuration ---
const int NUM_SENSORS = 3;
String targetMACs[NUM_SENSORS] = {"", "", ""};
String sensorLabels[NUM_SENSORS] = {"Sensor Slot 1", "Sensor Slot 2", "Sensor Slot 3"};

// --- Live Telemetry Buffers ---
float currentTemp[NUM_SENSORS] = {0.0, 0.0, 0.0};
float currentHumi[NUM_SENSORS] = {0.0, 0.0, 0.0};
bool sensorSeen[NUM_SENSORS] = {false, false, false};
String discoveredTags = ""; 

// --- Data Logging Cache (10 Hours max) ---
const int MAX_POINTS = 130; 
struct LogData {
    unsigned long timestamp; 
    float temps[NUM_SENSORS];
    float humis[NUM_SENSORS];
};
LogData dataLog[MAX_POINTS];
int dataCount = 0;

// Mutex lock to safely handle multi-threaded memory access
SemaphoreHandle_t dataMutex;
bool userTriggeredScan = false;

// --- BLE Engine Callback ---
class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->haveManufacturerData()) {
            std::string data = advertisedDevice->getManufacturerData();
            
            if (data.length() >= 7 && (uint8_t)data[0] == 0x99 && (uint8_t)data[1] == 0x04) {
                int dataFormat = (uint8_t)data[2];
                if (dataFormat == 5) { 
                    int16_t rawTemp = ((int8_t)data[3] << 8) | (uint8_t)data[4];
                    float parsedTemp = rawTemp * 0.005;

                    uint16_t rawHumi = ((uint8_t)data[5] << 8) | (uint8_t)data[6];
                    float parsedHumi = rawHumi * 0.0025;

                    String deviceMac = String(advertisedDevice->getAddress().toString().c_str());

                    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
                        for (int i = 0; i < NUM_SENSORS; i++) {
                            if (targetMACs[i].length() > 0 && targetMACs[i] == deviceMac) {
                                currentTemp[i] = parsedTemp;
                                currentHumi[i] = parsedHumi;
                                sensorSeen[i] = true;
                            }
                        }
                        if (discoveredTags.indexOf(deviceMac) == -1) {
                            discoveredTags += "<div style='margin-bottom:6px;'>🏷️ Tag Heard! MAC: <code>" + deviceMac + "</code></div>";
                        }
                        xSemaphoreGive(dataMutex);
                    }
                }
            }
        }
    }
};
// --- Cummins Dark-Themed UI Dashboard ---
const char htmlDashboardHeader[] PROGMEM = "<!DOCTYPE html><html><head>"
"<meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>body{font-family:sans-serif; background:#121212; color:#e0e0e0; padding:15px; text-align:center;}"
"h2, h3{color:#00adb5; margin:10px 0;} .box{background:#1e1e1e; padding:15px; border-radius:8px; margin:0 auto 15px auto; max-width:750px; border:1px solid #333;}"
".grid{display:flex; flex-wrap:wrap; gap:10px; justify-content:center; max-width:750px; margin:0 auto 15px auto;}"
".sensor-box{background:#252525; border:1px solid #444; border-radius:6px; padding:10px; width:220px; text-align:left;}"
".sensor-title{font-size:14px; color:#00adb5; font-weight:bold; margin-bottom:5px; border-bottom:1px solid #333; padding-bottom:3px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;}"
".lbl{font-size:12px; color:#aaa; margin-top:4px;}"
".val{font-size:16px; font-weight:bold; color:#fff; margin-bottom:4px;}"
"input[type=file], input[type=text]{background:#2d2d2d; padding:8px; border-radius:4px; color:#fff; border:1px solid #444; margin:4px 0; width:90%; font-size:13px;}"
"input[type=button], .btn-action, button{background:#00adb5; color:#fff; border:none; padding:10px 15px; border-radius:4px; cursor:pointer; font-weight:bold; text-decoration:none; display:inline-block; margin:4px; font-size:14px;}"
".btn-clear{background:#d9534f;} .btn-scan{background:#f0ad4e; color:#222;} .btn-save{background:#28a745; width:100%; margin-top:10px;} code{background:#000; color:#0f0; padding:2px 6px; border-radius:3px; font-family:monospace; user-select:all; cursor:pointer;}"
"table{width:100%; border-collapse:collapse; margin-top:10px; font-size:12px; text-align:left;}"
"th{background:#00adb5; color:#fff; padding:8px;}"
"td{padding:8px; border-bottom:1px solid #333; font-family:monospace;}"
"tr:nth-child(even){background:#252525;}</style></head><body>"
"<h2>📊 RuuviTag Multi-Sensor Dashboard</h2>"
"<div class='box'><h3>📡 Live Monitored Sensors</h3><div class='grid' id='sensor-grid'>Reading hardware pins...</div></div>"
"<div class='box'><h3>🔍 Local Device Discovery Tool</h3>"
"<p style='font-size:12px; color:#aaa; margin:0 0 10px 0;'>Click any green MAC address block to copy it, then paste it into the Config Mapping profiles below.</p>"
"<button onclick='triggerManualScan()' class='btn-action btn-scan' id='scan-btn'>Scan for Nearby RuuviTags</button>"
"<div id='scan-results' style='margin-top:10px; text-align:left; padding-left:10px; color:#ffb703;'></div></div>"
"<div class='box'><h3>⚙️ Profile Mapping Configurations</h3>"
"<form action='/save-config' method='POST' style='text-align:left; max-width:500px; margin:0 auto;'>"
" <div style='border-bottom:1px solid #333; margin-bottom:10px; padding-bottom:10px;'>"
"  <strong>Slot 1 Name:</strong><br><input type='text' name='l0' id='cfg-l0'><br>"
"  <strong>Slot 1 MAC Address:</strong><br><input type='text' name='m0' id='cfg-m0' placeholder='aa:bb:cc:dd:ee:ff'>"
" </div>"
" <div style='border-bottom:1px solid #333; margin-bottom:10px; padding-bottom:10px;'>"
"  <strong>Slot 2 Name:</strong><br><input type='text' name='l1' id='cfg-l1'><br>"
"  <strong>Slot 2 MAC Address:</strong><br><input type='text' name='m1' id='cfg-m1' placeholder='aa:bb:cc:dd:ee:ff'>"
" </div>"
" <div style='margin-bottom:10px;'>"
"  <strong>Slot 3 Name:</strong><br><input type='text' name='l2' id='cfg-l2'><br>"
"  <strong>Slot 3 MAC Address:</strong><br><input type='text' name='m2' id='cfg-m2' placeholder='aa:bb:cc:dd:ee:ff'>"
" </div>"
" <button type='submit' class='btn-save'>💾 Save Sensor Mapping Settings</button>"
"</form></div>";

const char htmlDashboardFooter[] PROGMEM = "<div class='box'><h3>🗄️ Memory Management</h3>"
"<button onclick='clearLocalMemory()' class='btn-action btn-clear'>🗑️ Wipe Saved Log</button></div>"
"<div class='box'><h3>Wireless Firmware Management</h3><form id='upload-form' enctype='multipart/form-data'>"
"<input type='file' id='file-input' name='update' accept='.bin' required> "
"<input type='button' value='Flash Payload (.bin)' onclick='uploadFile()'></form>"
"<div class='progress-container' id='prg-wrapper' style='width:100%; background:#2d2d2d; border-radius:4px; margin-top:10px; display:none;'><div class='progress-bar' id='prg-bar' style='width:0%; height:18px; background:#00adb5; border-radius:4px; text-align:center; line-height:18px; color:white; font-size:11px;'>0%</div></div><div id='status-msg' style='margin-top:8px; font-weight:bold; color:#ffb703;'></div></div>"
"<script>"
"var firstLoad = true;"
"function pollTelemetry() {"
" fetch('/telemetry-json').then(r => r.json()).then(data => {"
"  let gridHtml = '';"
"  for(let i=0; i<3; i++){"
"    if (firstLoad) {"
"      document.getElementById('cfg-l'+i).value = data.sensors[i].label;"
"      document.getElementById('cfg-m'+i).value = data.sensors[i].mac === 'None Assigned' ? '' : data.sensors[i].mac;"
"    }"
"    let displayMac = data.sensors[i].mac;"
"    gridHtml += `<div class='sensor-box'><div class='sensor-title'>📌 ${data.sensors[i].label}</div>`+"
"                `<div class='lbl'>MAC:</div><div class='val' style='font-size:11px; color:#888;'>${displayMac}</div>`+"
"                `<div class='lbl'>Temperature:</div><div class='val' style='color:#ff4757;'>${data.sensors[i].t.toFixed(1)}°C</div>`+"
"                `<div class='lbl'>Humidity:</div><div class='val' style='color:#00adb5;'>${data.sensors[i].h.toFixed(1)}%</div></div>`;"
"  }"
"  document.getElementById('sensor-grid').innerHTML = gridHtml;"
"  firstLoad = false;"
" }).catch(err => console.log('Awaiting server window...'));"
"}"
"function triggerManualScan() {"
"  let btn = document.getElementById('scan-btn'); btn.disabled = true; btn.innerText = 'Sniffing BLE frequencies...';"
"  fetch('/scan-now', {method:'POST'}).then(r => r.text()).then(text => {"
"    document.getElementById('scan-results').innerHTML = text || 'No new tags heard. Move closer.';"
"    btn.disabled = false; btn.innerText = 'Scan for Nearby RuuviTags';"
"    pollTelemetry();"
"  });"
"}"
"setInterval(pollTelemetry, 2000);"
"setTimeout(pollTelemetry, 200);"
"function clearLocalMemory(){ if(confirm('Wipe data arrays?')){ fetch('/clear',{method:'POST'}).then(() => { window.location.reload(); }); } }"
"function uploadFile(){ var fi=document.getElementById('file-input'); if(fi.files.length===0){alert('Select .bin!');return;} var fd=new FormData(); fd.append('update',fi.files); var xhr=new XMLHttpRequest(); xhr.open('POST','/update',true); document.getElementById('prg-wrapper').style.display='block'; document.getElementById('status-msg').innerText='Uploading firmware...';"
"xhr.upload.addEventListener('progress',function(e){ if(e.lengthComputable){ var p=Math.round((e.loaded/e.total)*100); document.getElementById('prg-bar').style.width=p+'%'; document.getElementById('prg-bar').innerText=p+'%'; } });"
"xhr.onload=function(){ if(xhr.status===200){ document.getElementById('status-msg').style.color='#00ff00'; document.getElementById('status-msg').innerText='✅ Success! Rebooting...'; setTimeout(function(){ window.location.reload(); }, 5000); }else{ document.getElementById('status-msg').innerText='❌ Failed: '+xhr.responseText; } }; xhr.send(fd); }</script></body></html>";
// --- Endpoint Handlers ---
void handleRoot() {
    String htmlLogTable = "<div class='box'><h3>📈 10-Hour Rolling Timeline Log</h3>";
    htmlLogTable += "<table><tr><th>Time Elapse</th>";
    for(int s=0; s<NUM_SENSORS; s++) {
        htmlLogTable += "<th>" + sensorLabels[s] + "</th>";
    }
    htmlLogTable += "</tr>";

    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        if(dataCount == 0) {
            htmlLogTable += "<tr><td colspan='4' style='text-align:center; color:#aaa;'>Waiting for data sweep... Log matches on 5 min mark.</td></tr>";
        } else {
            for (int i = dataCount - 1; i >= 0; i--) {
                htmlLogTable += "<tr><td>" + String(dataLog[i].timestamp) + " mins ago</td>";
                for(int s=0; s<NUM_SENSORS; s++) {
                    htmlLogTable += "<td>" + String(dataLog[i].temps[s], 1) + "°C / " + String(dataLog[i].humis[s], 0) + "%</td>";
                }
                htmlLogTable += "</tr>";
            }
        }
        xSemaphoreGive(dataMutex);
    }
    htmlLogTable += "</table></div>";

    String pageResponse = String(htmlDashboardHeader) + htmlLogTable + String(htmlDashboardFooter);
    server.send(200, "text/html; charset=utf-8", pageResponse);
}

void handleScanNow() {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        discoveredTags = "";
        userTriggeredScan = true;
        xSemaphoreGive(dataMutex);
    }
    
    // Give background worker thread 4 seconds to execute the scanner pass
    delay(4200); 
    server.send(200, "text/html; charset=utf-8", discoveredTags);
}

void handleSaveConfig() {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (server.hasArg("m" + String(i))) {
                String cleanMac = server.arg("m" + String(i));
                cleanMac.trim(); cleanMac.toLowerCase();
                targetMACs[i] = cleanMac;
            }
            if (server.hasArg("l" + String(i))) {
                String cleanLabel = server.arg("l" + String(i));
                cleanLabel.trim();
                if (cleanLabel.length() > 0) sensorLabels[i] = cleanLabel;
            }
        }
        dataCount = 0; // Wipe history cache on profile mapping edits
        xSemaphoreGive(dataMutex);
    }
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Saved");
}

void handleTelemetryJson() {
    String json = "{\"sensors\":[";
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        for(int i = 0; i < NUM_SENSORS; i++) {
            json += "{\"mac\":\"" + (targetMACs[i] == "" ? "None Assigned" : targetMACs[i]) + "\",";
            json += "\"label\":\"" + sensorLabels[i] + "\",";
            json += "\"t\":" + String(currentTemp[i], 2) + ",";
            json += "\"h\":" + String(currentHumi[i], 2) + "}";
            if(i < NUM_SENSORS - 1) json += ",";
        }
        xSemaphoreGive(dataMutex);
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void handleClear() {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        dataCount = 0;
        for(int i=0; i<NUM_SENSORS; i++) {
            currentTemp[i] = 0.0; currentHumi[i] = 0.0; sensorSeen[i] = false;
        }
        xSemaphoreGive(dataMutex);
    }
    server.send(200, "text/plain", "OK");
}

void handleUpdateResponse() {
    bool shouldReboot = !Update.hasError();
    server.sendHeader("Connection", "close");
    server.send(shouldReboot ? 200 : 500, "text/plain", shouldReboot ? "OK" : "FAIL");
    if(shouldReboot) { delay(1000); ESP.restart(); }
}

void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) Update.printError(Serial);
    }
}

// --- Background Core Task Worker (FreeRTOS Engine) ---
void bleWorkerTask(void *pvParameters) {
    unsigned long localLastLogTime = millis();
    unsigned long localLastLiveTime = millis();
    
    // Seed initial run markers
    pBLEScan->start(2, false);
    pBLEScan->clearResults();

    while(1) {
        // Condition A: User clicked manual discovery tracking sweep
        if (userTriggeredScan) {
            pBLEScan->start(4, false);
            pBLEScan->clearResults();
            userTriggeredScan = false;
        }

        // Condition B: Periodic current numbers refresh (every 30 seconds)
        if (millis() - localLastLiveTime >= 30000) {
            pBLEScan->start(2, false);
            pBLEScan->clearResults();
            localLastLiveTime = millis();
        }

        // Condition C: Log data snapshot into structural matrix (every 5 minutes)
        if (millis() - localLastLogTime >= 300000) {
            pBLEScan->start(2, false);
            pBLEScan->clearResults();

            if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
                for (int i = 0; i < dataCount; i++) {
                    dataLog[i].timestamp += 5;
                }
                if (dataCount < MAX_POINTS) {
                    dataLog[dataCount].timestamp = 0;
                    for(int s=0; s<NUM_SENSORS; s++) {
                        dataLog[dataCount].temps[s] = currentTemp[s];
                        dataLog[dataCount].humis[s] = currentHumi[s];
                    }
                    dataCount++;
                } else {
                    for (int i = 1; i < MAX_POINTS; i++) dataLog[i - 1] = dataLog[i];
                    dataLog[MAX_POINTS - 1].timestamp = 0;
                    for(int s=0; s<NUM_SENSORS; s++) {
                        dataLog[MAX_POINTS - 1].temps[s] = currentTemp[s];
                        dataLog[MAX_POINTS - 1].humis[s] = currentHumi[s];
                    }
                }
                xSemaphoreGive(dataMutex);
            }
            localLastLogTime = millis();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Sleep background loop slightly to clear watchdogs
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    dataMutex = xSemaphoreCreateMutex();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    WiFi.setTxPower(WIFI_POWER_8_5dBm); 

    server.on("/", HTTP_GET, handleRoot);
    server.on("/scan-now", HTTP_POST, handleScanNow);
    server.on("/save-config", HTTP_POST, handleSaveConfig);
    server.on("/telemetry-json", HTTP_GET, handleTelemetryJson);
    server.on("/clear", HTTP_POST, handleClear);
    server.on("/update", HTTP_POST, handleUpdateResponse, handleUpdateUpload);
    server.begin();

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(200);
    pBLEScan->setWindow(150);

    // Spawns the background radio task runner on core thread scheduler
    xTaskCreatePinnedToCore(bleWorkerTask, "BLE_Worker", 4096, NULL, 1, NULL, 0);
}

void loop() {
    server.handleClient();
    delay(2); // Yields CPU core control smoothly back to the network stack
}
