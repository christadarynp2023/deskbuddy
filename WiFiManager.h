#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include "DisplayManager.h"
#include "StorageManager.h"

namespace WiFiManager {

WebServer configServer(80);
DNSServer dnsServer;
bool inConfigMode = false;
unsigned long wifiConnectStart = 0;
const unsigned long wifiTimeout = 20000; // 20 seconds timeout for wifi connection before opening AP

// HTML template containing the custom placeholder values
String getPortalHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DeskBuddy Setup</title>
  <style>
    :root {
      --bg: #0b132b;
      --card-bg: rgba(28, 37, 65, 0.6);
      --primary: #48cae4;
      --primary-hover: #00b4d8;
      --accent: #5bc0be;
      --text: #edf2f4;
      --text-muted: #a3b18a;
      --border: rgba(255, 255, 255, 0.1);
    }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background: var(--bg);
      background-image: radial-gradient(circle at top right, #1c2541, var(--bg));
      color: var(--text);
      max-width: 480px;
      margin: 0 auto;
      padding: 20px;
      line-height: 1.5;
    }
    .card {
      background: var(--card-bg);
      backdrop-filter: blur(10px);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 24px;
      box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3);
      margin-bottom: 20px;
    }
    h1 {
      font-size: 24px;
      margin-top: 0;
      color: var(--primary);
      text-align: center;
      font-weight: 700;
      letter-spacing: 0.5px;
    }
    h2 {
      font-size: 18px;
      margin-top: 0;
      color: var(--accent);
      border-bottom: 1px solid var(--border);
      padding-bottom: 8px;
      margin-bottom: 16px;
    }
    label {
      display: block;
      font-size: 13px;
      font-weight: 600;
      margin-bottom: 6px;
      color: #90e0ef;
    }
    input[type="text"], input[type="password"], input[type="file"] {
      width: 100%;
      padding: 12px;
      margin-bottom: 16px;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: rgba(11, 19, 43, 0.8);
      color: var(--text);
      box-sizing: border-box;
      font-size: 14px;
      transition: all 0.3s ease;
    }
    input:focus {
      outline: none;
      border-color: var(--primary);
      box-shadow: 0 0 0 2px rgba(72, 202, 228, 0.2);
    }
    button {
      width: 100%;
      padding: 14px;
      background: var(--primary);
      color: #0b132b;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 700;
      cursor: pointer;
      transition: all 0.3s ease;
    }
    button:hover {
      background: var(--primary-hover);
      transform: translateY(-1px);
    }
    button:active {
      transform: translateY(1px);
    }
    .progress-container {
      display: none;
      margin-top: 15px;
    }
    .progress-bar-bg {
      background: rgba(255, 255, 255, 0.1);
      border-radius: 8px;
      height: 12px;
      overflow: hidden;
      margin-bottom: 6px;
    }
    .progress-bar {
      background: linear-gradient(90deg, var(--primary), var(--accent));
      width: 0%;
      height: 100%;
      border-radius: 8px;
      transition: width 0.1s ease;
    }
    .status-text {
      font-size: 12px;
      color: #90e0ef;
      display: flex;
      justify-content: space-between;
    }
    .footer {
      text-align: center;
      font-size: 11px;
      color: rgba(255, 255, 255, 0.4);
      margin-top: 30px;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>DeskBuddy Setup</h1>
    <p style="text-align:center; font-size:13px; color:rgba(255,255,255,0.7); margin-bottom:24px;">Configure connection settings or update firmware.</p>
    
    <form action="/save" method="POST">
      <h2>Wi-Fi &amp; Region</h2>
      <label for="ssid">Wi-Fi Network (SSID)</label>
      <input type="text" id="ssid" name="ssid" placeholder="Network Name" value="%SSID%">
      
      <label for="pass">Wi-Fi Password</label>
      <input type="password" id="pass" name="pass" placeholder="Password">
      
      <label for="tz">Timezone (Posix String)</label>
      <input type="text" id="tz" name="tz" placeholder="e.g. IST-5:30 or EST5EDT" value="%TZ%">
      
      <button type="submit">Save &amp; Reboot</button>
    </form>
  </div>

  <div class="card">
    <h2>Firmware Update</h2>
    <p style="font-size:12px; color:rgba(255,255,255,0.6); margin-bottom:16px;">Upload a compiled .bin file to update DeskBuddy.</p>
    
    <form id="uploadForm">
      <input type="file" id="fileInput" name="update" accept=".bin" required>
      <button type="submit" id="uploadBtn">Upload Firmware</button>
    </form>
    
    <div class="progress-container" id="progressContainer">
      <div class="progress-bar-bg">
        <div class="progress-bar" id="progressBar"></div>
      </div>
      <div class="status-text">
        <span id="statusLabel">Uploading...</span>
        <span id="percentLabel">0%</span>
      </div>
    </div>
  </div>

  <div class="footer">
    DeskBuddy Ecosystem &bull; Designed by Google DeepMind
  </div>

  <script>
    document.getElementById('uploadForm').addEventListener('submit', function(e) {
      e.preventDefault();
      var fileInput = document.getElementById('fileInput');
      if (fileInput.files.length === 0) return;
      
      var file = fileInput.files[0];
      var formData = new FormData();
      formData.append('update', file);
      
      var xhr = new XMLHttpRequest();
      var uploadBtn = document.getElementById('uploadBtn');
      var progressContainer = document.getElementById('progressContainer');
      var progressBar = document.getElementById('progressBar');
      var percentLabel = document.getElementById('percentLabel');
      var statusLabel = document.getElementById('statusLabel');
      
      uploadBtn.disabled = true;
      progressContainer.style.display = 'block';
      
      xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
          var percent = Math.round((e.loaded / e.total) * 100);
          progressBar.style.width = percent + '%';
          percentLabel.innerText = percent + '%';
          if (percent === 100) {
            statusLabel.innerText = 'Writing flash, please wait...';
          }
        }
      });
      
      xhr.addEventListener('load', function() {
        if (xhr.status === 200) {
          statusLabel.innerText = 'Update complete! Rebooting...';
          statusLabel.style.color = '#4ade80';
          setTimeout(function() {
            window.location.reload();
          }, 3000);
        } else {
          statusLabel.innerText = 'Update failed: ' + xhr.responseText;
          statusLabel.style.color = '#f87171';
          uploadBtn.disabled = false;
        }
      });
      
      xhr.addEventListener('error', function() {
        statusLabel.innerText = 'Upload connection error.';
        statusLabel.style.color = '#f87171';
        uploadBtn.disabled = false;
      });
      
      xhr.open('POST', '/update');
      xhr.send(formData);
    });
  </script>
</body>
</html>
)rawliteral";

  html.replace("%SSID%", wifiSsid);
  html.replace("%TZ%", tzString);
  return html;
}

void handleConfigRoot() {
  configServer.send(200, "text/html", getPortalHTML());
}

void handleConfigSave() {
  if (!configServer.hasArg("ssid") || configServer.arg("ssid").length() == 0) {
    configServer.send(400, "text/plain", "SSID is required");
    return;
  }
  String s = configServer.arg("ssid");
  String p = configServer.arg("pass");
  String tz = configServer.arg("tz");

  Storage::saveWiFiConfig(s, p, tz);

  configServer.send(200, "text/html",
    "<html><body style='font-family:sans-serif;background:#0b132b;color:#edf2f4;padding:40px;text-align:center;'>"
    "<h2 style='color:#48cae4'>Settings Saved!</h2><p>DeskBuddy is rebooting to connect to the new network...</p></body></html>");
  
  delay(2000);
  ESP.restart();
}

void handleCaptivePortalRedirect() {
  configServer.sendHeader("Location", "http://192.168.4.1/", true);
  configServer.send(302, "text/plain", ""); 
}

void startConfigPortal() {
  inConfigMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("DeskBuddy_Setup", "12345678");

  dnsServer.start(53, "*", WiFi.softAPIP());

  configServer.on("/", handleConfigRoot);
  configServer.on("/save", HTTP_POST, handleConfigSave);
  
  configServer.on("/update", HTTP_POST, []() {
    configServer.sendHeader("Connection", "close");
    configServer.send(200, "text/plain", (Update.hasError()) ? "Update failed" : "Update successful! Rebooting...");
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = configServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Web OTA Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Web OTA Success: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  configServer.on("/generate_204", handleCaptivePortalRedirect);
  configServer.on("/hotspot-detect.html", handleCaptivePortalRedirect);
  configServer.on("/canonical.html", handleCaptivePortalRedirect);
  configServer.on("/connecttest.txt", handleCaptivePortalRedirect);
  configServer.onNotFound(handleCaptivePortalRedirect);

  configServer.begin();

  display.clearDisplay();
  display.setFont(NULL);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("AP: Desk-\nBuddy_Setup\n\nURL:\n192.168.4.1\n\nScan to pair");

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String url = "https://christadarynp2023.github.io/deskbuddy?mac=" + mac;
  QR::draw(display, url.c_str(), 2);

  display.display();
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  if (wifiSsid.length() > 0) {
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    wifiConnectStart = millis();
  } else {
    startConfigPortal();
  }
}

void updateWiFi() {
  if (inConfigMode) {
    dnsServer.processNextRequest();
    configServer.handleClient();
  } else {
    if (WiFi.status() != WL_CONNECTED) {
      if (wifiSsid.length() > 0 && (millis() - wifiConnectStart > wifiTimeout)) {
        Serial.println("Wi-Fi Connection timeout. Starting captive portal.");
        startConfigPortal();
      }
    } else {
      if (!ntpSynced) {
        configTime(0, 0, "pool.ntp.org");
        setenv("TZ", tzString.c_str(), 1);
        tzset();
        ntpSynced = true;
        Serial.println("Time synced via NTP successfully.");
      }
    }
  }
}

} // namespace WiFiManager

#endif
