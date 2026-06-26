#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Update.h>
#include <sys/time.h>

// BLE Server, client tracking, Call Overlay notifications, Time Sync, and BLE OTA updates.
extern bool bleConnected;
extern bool bleAdvertised;
extern bool isIncomingCall;
extern String callerInfo;
extern int phoneBatteryPct;
extern bool otaUpdating;
extern size_t otaTotalSize;
extern size_t otaWrittenSize;
extern bool displayIsOff;
extern unsigned long lastDinoInputTime;
extern int currentMood;
extern EyeStyle currentEyeStyle;
extern Adafruit_SSD1306 display;
extern bool ntpSynced;

namespace BLE {

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    pServer->startAdvertising();
  }
};

class CallCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String val = pChar->getValue();
    if (val.length() > 0) {
      char cmd = val[0];
      if (cmd == 'R') {
        isIncomingCall = true;
        if (val.length() > 2 && val[1] == ':') {
          callerInfo = val.substring(2);
        } else {
          callerInfo = "Unknown";
        }
        if (displayIsOff) {
          display.ssd1306_command(SSD1306_DISPLAYON);
          displayIsOff = false;
        }
        lastDinoInputTime = millis();
      } else if (cmd == 'I') {
        isIncomingCall = false;
        callerInfo = "";
      }
    }
  }
};

class SyncCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String val = pChar->getValue();
    if (val.length() > 0) {
      char cmd = val[0];
      if (cmd == 'T') {
        // Time sync format: T:<Epoch>:<Timezone>
        int firstColon = val.indexOf(':');
        int secondColon = val.indexOf(':', firstColon + 1);
        if (firstColon != -1 && secondColon != -1) {
          String epochStr = val.substring(firstColon + 1, secondColon);
          String tzStr = val.substring(secondColon + 1);

          time_t tVal = (time_t)epochStr.toInt();
          struct timeval tv = { .tv_sec = tVal, .tv_usec = 0 };
          settimeofday(&tv, NULL);
          
          setenv("TZ", tzStr.c_str(), 1);
          tzset();
          ntpSynced = true; // Mark synced so we bypass network NTP queries
        }
      } else if (cmd == 'B') {
        // Battery status format: B:<Percentage>
        int colon = val.indexOf(':');
        if (colon != -1) {
          phoneBatteryPct = val.substring(colon + 1).toInt();
        }
      } else if (cmd == 'F') {
        // Find My Desk Buddy: trigger surprises & blinks
        if (displayIsOff) {
          display.ssd1306_command(SSD1306_DISPLAYON);
          displayIsOff = false;
        }
        lastDinoInputTime = millis();
        // Set mood to surprise to alert user
        currentMood = MOOD_SURPRISED;
        // Buzz sound alert (if buzzer pin is attached) or blink OLED
        for (int i = 0; i < 3; i++) {
          display.invertDisplay(true);
          delay(150);
          display.invertDisplay(false);
          delay(150);
        }
      }
    }
  }
};

class OtaCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String val = pChar->getValue();
    if (val.length() > 0) {
      char cmd = val[0];
      if (cmd == 'S') {
        // Start OTA format: S:<Size>
        int colon = val.indexOf(':');
        if (colon != -1) {
          otaTotalSize = val.substring(colon + 1).toInt();
          otaWrittenSize = 0;
          otaUpdating = true;

          // Wake display and show OTA progress
          if (displayIsOff) {
            display.ssd1306_command(SSD1306_DISPLAYON);
            displayIsOff = false;
          }
          lastDinoInputTime = millis();

          display.clearDisplay();
          display.setFont(NULL);
          display.setCursor(10, 20);
          display.print("Firmware Update...");
          display.display();

          if (!Update.begin(otaTotalSize)) {
            otaUpdating = false;
          }
        }
      } else if (cmd == 'D') {
        // Data chunk: D:<BinaryData>
        // Use raw data pointer to bypass string conversion issues
        uint8_t* rawData = pChar->getData();
        size_t rawLen = pChar->getLength();
        if (rawLen > 2 && rawData[0] == 'D' && rawData[1] == ':') {
          size_t chunkLen = rawLen - 2;
          uint8_t* chunk = rawData + 2;

          if (Update.write(chunk, chunkLen) == chunkLen) {
            otaWrittenSize += chunkLen;
            int pct = (otaWrittenSize * 100) / otaTotalSize;
            
            // Draw progress bar on screen
            display.clearDisplay();
            display.setCursor(10, 10);
            display.print("Updating OTA...");
            display.setCursor(10, 24);
            display.print(String(pct) + "% completed");
            display.drawRect(10, 42, 108, 10, SSD1306_WHITE);
            display.fillRect(12, 44, map(pct, 0, 100, 0, 104), 6, SSD1306_WHITE);
            display.display();
          }
        }
      } else if (cmd == 'E') {
        // End OTA
        if (Update.end(true)) {
          display.clearDisplay();
          display.setCursor(10, 20);
          display.print("Update Success!");
          display.setCursor(10, 36);
          display.print("Rebooting...");
          display.display();
          delay(2000);
          ESP.restart();
        } else {
          display.clearDisplay();
          display.setCursor(10, 20);
          display.print("Update Failed!");
          display.display();
          delay(3000);
          otaUpdating = false;
        }
      }
    }
  }
};

void initBLE() {
  BLEDevice::init("Desk Buddy");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // ANS Service
  BLEService *pService = pServer->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");

  // Call notifications characteristic
  BLECharacteristic *pCallChar = pService->createCharacteristic(
                                   "beb5483e-36e1-4688-b7f5-ea07361b26a8",
                                   BLECharacteristic::PROPERTY_WRITE
                                 );
  pCallChar->setCallbacks(new CallCharCallbacks());

  // Time & Battery synchronization characteristic
  BLECharacteristic *pSyncChar = pService->createCharacteristic(
                                   "a1234567-1fb5-459e-8fcc-c5c9c331914b",
                                   BLECharacteristic::PROPERTY_WRITE
                                 );
  pSyncChar->setCallbacks(new SyncCharCallbacks());

  // BLE OTA updates characteristic
  BLECharacteristic *pOtaChar = pService->createCharacteristic(
                                  "b1234567-1fb5-459e-8fcc-c5c9c331914b",
                                  BLECharacteristic::PROPERTY_WRITE
                                );
  pOtaChar->setCallbacks(new OtaCharCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  bleAdvertised = true;
}

} // namespace BLE

#endif
