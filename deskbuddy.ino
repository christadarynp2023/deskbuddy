// watch complete making video from here , please subscribe and support us
// you can buy components and this kit from www.esclabs.in 

// ==================================================
// EDISON SCIENCE CORNER  - ESCLABS
// ==================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"
#include <math.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Update.h>

// ==================================================
// 1. STATE MACHINE & ENUMS
// ==================================================

enum AppState {
  STATE_BOOT,
  STATE_CLOCK,               // Time + Date + Pet Idle Page
  STATE_MENU,                // Scrolling menu
  STATE_POMO_SETUP,
  STATE_POMO_RUNNING,
  STATE_POMO_COMPLETE,
  STATE_POMO_BREAK,
  STATE_DINO_START,
  STATE_DINO_PLAY,
  STATE_DINO_GAMEOVER,
  STATE_MOOD_CHECKIN,        // Wellness mood logging
  STATE_MOOD_RESPONSE,       // Custom supportive text
  STATE_PET_STATS,           // Pet feeding/playing dashboard
  STATE_SETTINGS_MENU,
  STATE_SETTINGS_BRIGHTNESS,
  STATE_SETTINGS_SOUND,
  STATE_SETTINGS_SPEED,
  STATE_SETTINGS_POMO_TIME,
  STATE_SETTINGS_BREAK_TIME,
  STATE_SETTINGS_FORMAT,
  STATE_SETTINGS_SLEEP,
  STATE_SETTINGS_PET_RESET,
  STATE_SETTINGS_ABOUT,
  STATE_ONBOARDING_QR,
  STATE_IDLE
};

enum EyeStyle { STYLE_CLASSIC, STYLE_BOLD, STYLE_ROUND, STYLE_SQUINTY };
enum AnimSpeed { SPEED_NORMAL, SPEED_FAST, SPEED_SLOW };

// ==================================================
// 2. GLOBAL VARIABLES (Shared across headers)
// ==================================================

AppState currentState = STATE_BOOT;
EyeStyle currentEyeStyle = STYLE_CLASSIC;
AnimSpeed currentAnimSpeed = SPEED_NORMAL;

// Wi-Fi and timezone configs
String wifiSsid = "";
String wifiPass = "";
String tzString = "";
const char* ntpServer = "pool.ntp.org";
bool ntpSynced = false;

// Screen and settings
bool screenHighBrightness = true;
bool soundOn = true;
bool clockFormat24h = false;
int sleepTimeoutMins = 5;
bool displayIsOff = false;

// Moods
#define MOOD_NORMAL 0
#define MOOD_HAPPY 1
#define MOOD_SURPRISED 2
#define MOOD_SLEEPY 3
#define MOOD_ANGRY 4
#define MOOD_SAD 5
#define MOOD_EXCITED 6
#define MOOD_LOVE 7
#define MOOD_SUSPICIOUS 8
#define MOOD_COOL 9

int currentMood = MOOD_NORMAL;

// Virtual Pet stats
float petHappiness = 50.0;
float petEnergy = 50.0;
int petXP = 0;
float petX = 12.0;
bool petWalkingLeft = false;

// Mood history
int moodHistory[5] = { -1, -1, -1, -1, -1 };
int moodHistorySize = 0;

// BLE companion states
bool bleConnected = false;
bool bleAdvertised = false;
bool isIncomingCall = false;
String callerInfo = "";
int phoneBatteryPct = 100;
bool otaUpdating = false;
size_t otaTotalSize = 0;
size_t otaWrittenSize = 0;

// Chrome Dino highscore
unsigned int dinoHighScore = 0;
#define CONFIG_HOLD_MS 3000
int pomoDuration = 25;
int pomoBreakDuration = 5;


// Inactivity timer
unsigned long lastDinoInputTime = 0;

// Now include the modular header files in correct order
#include "DisplayManager.h"
#include "StorageManager.h"
#include "QRGenerator.h"
#include "CompanionPet.h"
#include "AnimationManager.h"
#include "BleManager.h"
#include "WiFiManager.h"

using namespace Storage;
using namespace Pet;
using namespace BLE;
using namespace WiFiManager;

// ==================================================
// 3. MENU & NAVIGATION CONFIG
// ==================================================

// Main Menu items
const char* menuItems[] = {
  "🕒 Clock & Pet",
  "🍅 Pomodoro Focus",
  "🐾 Pet Stats",
  "😊 Mood Checkin",
  "🦖 Dino Runner",
  "🔗 Pair Phone",
  "⚙ Settings"
};
const int MENU_SIZE = 7;
int selectedMenuItem = 0;

// Sub-menu selection indices
int pomoSetupIndex = 0;       // 0: Time, 1: Start, 2: Back
int moodSelectIndex = 0;       // 0-4: Moods
int settingsMenuIndex = 0;     // 0: Brightness, 1: Sound, 2: Speed, 3: Pomo time, 4: Break time, 5: Format, 6: Sleep, 7: Pet Reset, 8: About, 9: Back
int settingsBrightnessIndex = 0; 
int settingsSoundIndex = 0;
int settingsSpeedIndex = 0;
int settingsPomoIndex = 0;
int settingsBreakIndex = 0;
int settingsFormatIndex = 0;
int settingsSleepIndex = 0;
int settingsResetIndex = 0;
int settingsStyleIndex = 0; 

int petActionIndex = 0; // 0: Feed, 1: Play, 2: Back

// MOOD CHECK-IN LOG
const char* wellnessMessage = "";
unsigned long moodResponseStart = 0;

// Timing & System variables
unsigned long lastFrameTime = 0;
unsigned long lastPetUpdate = 0;     // Decay timer
unsigned long bootStateTime = 0;
int bootStage = 0;

// POMODORO CONFIG
int completedSessionsToday = 0;
unsigned long pomoTimerStart = 0;
unsigned long pomoTimerDurationMs = 0;
unsigned long pomoCompleteStart = 0;
bool pomoBreakCompleted = false;
unsigned long pomoBreakCompleteTime = 0;

// Touch parameters
unsigned long pressStartTime = 0;
int tapCounter = 0;
unsigned long lastTapTime = 0;
bool lastPinState = false;
int pendingTap = 0;          
const unsigned long DOUBLE_TAP_DELAY = 200; 

// ==================================================
// 4. CHROME DINO RETRO GAME ENGINE
// ==================================================

float dinoY = 34.0; 
float dinoVelocityY = 0.0;
const float GRAVITY = 0.55;
const float JUMP_FORCE = -7.5;
bool dinoIsJumping = false;

struct Obstacle {
  float x;
  int y;
  int width;
  int height;
  bool active;
  int type;
};
Obstacle obstacles[2];

float cloudX = 128.0;
float cloudY = 12.0;
float groundOffset = 0.0;
unsigned long dinoScore = 0;
float gameSpeed = 3.0;

void spawnObstacles() {
  for (int i = 0; i < 2; i++) {
    if (!obstacles[i].active) {
      int other = 1 - i;
      if (obstacles[other].active && obstacles[other].x > 60.0) {
        continue; 
      }
      
      obstacles[i].active = true;
      obstacles[i].x = 128.0 + random(10, 60);
      obstacles[i].type = random(0, 3);
      
      if (obstacles[i].type == 0) {
        obstacles[i].width = 8;
        obstacles[i].height = 12;
        obstacles[i].y = 50 - 12;
      } else if (obstacles[i].type == 1) {
        obstacles[i].width = 16;
        obstacles[i].height = 12;
        obstacles[i].y = 50 - 12;
      } else {
        obstacles[i].width = 8;
        obstacles[i].height = 6;
        obstacles[i].y = 50 - 6;
      }
      break;
    }
  }
}

bool checkCollisions() {
  int dinoLeft = 18;
  int dinoRight = 28;
  int dinoTop = (int)dinoY + 2;
  int dinoBottom = (int)dinoY + 16;

  for (int i = 0; i < 2; i++) {
    if (obstacles[i].active) {
      int obsLeft = (int)obstacles[i].x;
      int obsRight = obsLeft + obstacles[i].width;
      int obsTop = obstacles[i].y;
      int obsBottom = obsTop + obstacles[i].height;

      if (dinoRight > obsLeft && dinoLeft < obsRight && dinoBottom > obsTop && dinoTop < obsBottom) {
        return true;
      }
    }
  }
  return false;
}

void updateDinoGame() {
  if (dinoIsJumping) {
    dinoVelocityY += GRAVITY;
    dinoY += dinoVelocityY;
    if (dinoY >= 34.0) {
      dinoY = 34.0;
      dinoIsJumping = false;
      dinoVelocityY = 0.0;
    }
  }

  spawnObstacles();
  for (int i = 0; i < 2; i++) {
    if (obstacles[i].active) {
      obstacles[i].x -= gameSpeed;
      if (obstacles[i].x < -obstacles[i].width) {
        obstacles[i].active = false;
      }
    }
  }

  dinoScore++;
  if (dinoScore % 150 == 0) {
    gameSpeed += 0.15;
    if (gameSpeed > 6.5) gameSpeed = 6.5;
  }

  cloudX -= 0.35;
  if (cloudX < -20.0) {
    cloudX = 128.0 + random(10, 50);
    cloudY = random(5, 20);
  }
  groundOffset -= gameSpeed;

  if (checkCollisions()) {
    if (dinoScore > dinoHighScore) {
      dinoHighScore = dinoScore;
      saveDinoHighScore();
    }
    currentState = STATE_DINO_GAMEOVER;
    lastDinoInputTime = millis();
  }
}

// ==================================================
// 5. TOUCH & INPUT GESTURE PROCESSING
// ==================================================

void handleTouch() {
  bool currentPinState = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  if (currentPinState && !lastPinState) {
    pressStartTime = now;

    // Wake screen immediately if asleep, consuming the touch
    if (displayIsOff) {
      display.ssd1306_command(SSD1306_DISPLAYON);
      displayIsOff = false;
      lastDinoInputTime = now;
      lastPinState = currentPinState;
      tapCounter = 0;
      pendingTap = 0;
      return;
    }

    // Snappy Dino press response during gameplay
    if (currentState == STATE_DINO_PLAY) {
      if (!dinoIsJumping) {
        dinoVelocityY = JUMP_FORCE;
        dinoIsJumping = true;
      }
    }
  } else if (!currentPinState && lastPinState) {
    unsigned long duration = now - pressStartTime;
    if (duration < 500 && !displayIsOff) { 
      tapCounter++;
      lastTapTime = now;
    }
  }

  lastPinState = currentPinState;

  if (tapCounter > 0) {
    if (tapCounter == 2) {
      pendingTap = 2; // Double tap
      tapCounter = 0;
    } else if (now - lastTapTime > DOUBLE_TAP_DELAY) {
      pendingTap = 1; // Single tap
      tapCounter = 0;
    }
  }
}

// ==================================================
// 6. BOOT & INITIALIZATION ANIMATION
// ==================================================

void runBootSequence() {
  unsigned long elapsed = millis() - bootStateTime;
  
  if (bootStage == 0) {
    leftEye.targetH = 0; rightEye.targetH = 0;
    leftEye.targetW = 36; rightEye.targetW = 36;
    leftEye.targetX = 18; rightEye.targetX = 74;
    leftEye.targetY = 12; rightEye.targetY = 12;
    leftEye.h = 0; rightEye.h = 0;
    
    if (elapsed > 1000) {
      bootStage = 1;
      leftEye.targetH = 24; rightEye.targetH = 24;
    }
  } 
  else if (bootStage == 1) {
    if (elapsed > 2000) {
      leftEye.targetH = 2; rightEye.targetH = 2;
      bootStage = 2;
    }
  } 
  else if (bootStage == 2) {
    if (elapsed > 2200) {
      leftEye.targetH = 24; rightEye.targetH = 24;
      bootStage = 3;
    }
  } 
  else if (bootStage == 3) {
    if (elapsed > 2800) {
      leftEye.targetH = 2; rightEye.targetH = 2;
      bootStage = 4;
    }
  }
  else if (bootStage == 4) {
    if (elapsed > 3000) {
      leftEye.targetH = 24; rightEye.targetH = 24;
      bootStage = 5;
    }
  }
  else if (bootStage == 5) {
    if (elapsed > 3500) {
      bootStage = 6;
    }
  }
  else if (bootStage == 6) {
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds("Hello!!!", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 54);
    display.print("Hello!!!");
    
    if (elapsed > 6000) {
      currentState = STATE_CLOCK;
      lastDinoInputTime = millis();
    }
  }
  
  leftEye.update(); rightEye.update();
  drawUltraProEye(leftEye, true, MOOD_NORMAL);
  drawUltraProEye(rightEye, false, MOOD_NORMAL);
}

void drawClockPage() {
  updatePhysicsAndMood();
  updatePetPosition();

  struct tm t;
  bool timeValid = getLocalTime(&t);
  
  display.setTextColor(SSD1306_WHITE);
  drawBLEIcon(2, 2);

  if (timeValid) {
    char timeStr[10];
    char dateStr[30];
    char dayStr[20];

    bool showColon = (millis() / 500) % 2 == 0;
    int hr = t.tm_hour;
    const char* ampm = "";

    if (!clockFormat24h) {
      ampm = (hr >= 12) ? "PM" : "AM";
      hr = hr % 12;
      if (hr == 0) hr = 12;
      
      if (showColon) {
        sprintf(timeStr, "%d:%02d", hr, t.tm_min);
      } else {
        sprintf(timeStr, "%d %02d", hr, t.tm_min);
      }
    } else {
      if (showColon) {
        sprintf(timeStr, "%02d:%02d", hr, t.tm_min);
      } else {
        sprintf(timeStr, "%02d %02d", hr, t.tm_min);
      }
    }

    strftime(dayStr, sizeof(dayStr), "%A", &t);
    for (int i = 0; dayStr[i]; i++) {
      if (dayStr[i] >= 'a' && dayStr[i] <= 'z') {
        dayStr[i] -= 32;
      }
    }

    strftime(dateStr, sizeof(dateStr), "%b %d, %Y", &t);

    int16_t x1, y1;
    uint16_t w, h;

    // 1. Draw Day (Top) - FreeSans9pt7b
    display.setFont(&FreeSans9pt7b);
    display.getTextBounds(dayStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 14);
    display.print(dayStr);

    // 2. Draw Time (Middle) - FreeSansBold18pt7b
    display.setFont(&FreeSansBold18pt7b);
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 44);
    display.print(timeStr);

    // 3. Draw AM/PM (Top-right corner) - default font
    if (!clockFormat24h) {
      display.setFont(NULL);
      display.setCursor(114, 2);
      display.print(ampm);
    }

    // 4. Draw Date (Bottom) - FreeSans9pt7b
    display.setFont(&FreeSans9pt7b);
    display.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 60);
    display.print(dateStr);

  } else {
    display.setFont(NULL);
    display.setTextSize(1);
    display.setCursor(18, 22);
    display.print("Connecting WiFi...");
    display.setCursor(18, 38);
    display.print("Syncing Time (NTP)");
  }
}

void drawMenuPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  drawBLEIcon(120, 2);

  display.setCursor(32, 2);
  display.print("DESK BUDDY");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);
  
  int startItem = selectedMenuItem - 1;
  if (startItem < 0) startItem = 0;
  if (startItem + 3 > MENU_SIZE) startItem = MENU_SIZE - 3;
  
  int y = 18;
  for (int i = startItem; i < startItem + 3; i++) {
    if (i == selectedMenuItem) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y);
    display.print(menuItems[i]);
    y += 14;
  }
}

void drawPomoSetupPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  drawBLEIcon(120, 2);
  display.setCursor(18, 2);
  display.print("POMODORO SETUP");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { "Focus: ", "Start Focus", "Back" };
  int y = 20;
  for (int i = 0; i < 3; i++) {
    if (i == pomoSetupIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(12, y);
    display.print(items[i]);
    if (i == 0) {
      display.print(pomoDuration);
      display.print(" min");
    }
    y += 14;
  }
}

void drawPomoRunningPage() {
  updatePhysicsAndMood();
  drawFaceSide(-12, 0, MOOD_NORMAL);

  unsigned long elapsed = millis() - pomoTimerStart;
  unsigned long remaining = (pomoTimerDurationMs > elapsed) ? (pomoTimerDurationMs - elapsed) : 0;
  unsigned long remSeconds = remaining / 1000;
  int mins = remSeconds / 60;
  int secs = remSeconds % 60;

  unsigned long encIndex = (millis() / 15000);
  const char* messages[] = { "Stay focused!", "You got this!", "Almost there!", "Brain power!", "Keep going!" };
  int numMsgs = sizeof(messages) / sizeof(messages[0]);
  const char* msg = messages[encIndex % numMsgs];

  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  
  display.drawBitmap(56, 36, bmp_book, 16, 16, SSD1306_WHITE);
  display.setCursor(76, 36);
  display.print("Focus!");

  char timeStr[15];
  sprintf(timeStr, "%02d:%02d", mins, secs);
  display.setCursor(76, 46);
  display.print(timeStr);
  
  display.setCursor(6, 48);
  display.print(msg);

  int progressWidth = map(elapsed, 0, pomoTimerDurationMs, 0, 128);
  display.drawRect(0, 58, 128, 4, SSD1306_WHITE);
  display.fillRect(0, 58, progressWidth, 4, SSD1306_WHITE);

  if (remaining == 0) {
    currentState = STATE_POMO_COMPLETE;
    pomoCompleteStart = millis();
    completedSessionsToday++;
    
    // Boost Pet XP & Happiness for Focus Completion!
    petXP += 100;
    petHappiness += 15.0; if (petHappiness > 100.0) petHappiness = 100.0;
    petEnergy += 20.0; if (petEnergy > 100.0) petEnergy = 100.0;
    saveConfig();
  }
}

void drawPomoCompletePage() {
  int savedMood = currentMood;
  currentMood = MOOD_HAPPY;
  updatePhysicsAndMood();
  currentMood = savedMood;
  
  drawUltraProEye(leftEye, true, MOOD_HAPPY);
  drawUltraProEye(rightEye, false, MOOD_HAPPY);
  drawMouth(64, 28, MOOD_HAPPY);
  
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(35, 38);
  display.print("Great Job!");
  display.setCursor(32, 48);
  display.print("Break Time!");

  unsigned long elapsed = millis() - pomoCompleteStart;
  if (elapsed > 4000) {
    currentState = STATE_POMO_BREAK;
    pomoTimerStart = millis();
    pomoTimerDurationMs = pomoBreakDuration * 60UL * 1000UL; 
    pomoBreakCompleted = false;
  }
}

void drawPomoBreakPage() {
  unsigned long elapsed = millis() - pomoTimerStart;
  unsigned long remaining = (pomoTimerDurationMs > elapsed) ? (pomoTimerDurationMs - elapsed) : 0;
  
  if (pomoBreakCompleted) {
    display.setFont(NULL);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(24, 28);
    display.print("Ready to Focus?");
    
    int savedMood = currentMood;
    currentMood = MOOD_HAPPY;
    updatePhysicsAndMood();
    currentMood = savedMood;
    drawFaceSide(-12, 0, MOOD_HAPPY);
    
    if (millis() - pomoBreakCompleteTime > 3000) {
      currentState = STATE_POMO_SETUP;
    }
    return;
  }

  int savedMood = currentMood;
  currentMood = MOOD_HAPPY;
  updatePhysicsAndMood();
  currentMood = savedMood;
  
  drawFaceSide(-12, 0, MOOD_HAPPY);

  unsigned long remSeconds = remaining / 1000;
  int mins = remSeconds / 60;
  int secs = remSeconds % 60;

  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.drawBitmap(56, 36, bmp_coffee, 16, 16, SSD1306_WHITE);
  display.setCursor(76, 36);
  display.print("Break");
  
  char timeStr[15];
  sprintf(timeStr, "%02d:%02d", mins, secs);
  display.setCursor(76, 46);
  display.print(timeStr);
  
  int progressWidth = map(elapsed, 0, pomoTimerDurationMs, 0, 128);
  display.drawRect(0, 58, 128, 4, SSD1306_WHITE);
  display.fillRect(0, 58, progressWidth, 4, SSD1306_WHITE);

  if (remaining == 0) {
    pomoBreakCompleted = true;
    pomoBreakCompleteTime = millis();
  }
}

void drawDinoStartPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(32, 10);
  display.print("DINO GAME");
  display.setCursor(14, 26);
  display.print("Double Tap to Play");
  display.setCursor(38, 42);
  display.print("1xTap: Back");
  
  bool blink = (millis() / 300) % 2 == 0;
  display.drawBitmap(56, 48, blink ? bmp_dino_run1 : bmp_dino_run2, 16, 16, SSD1306_WHITE);
}

void drawDinoGamePage() {
  unsigned long now = millis();
  updateDinoGame();

  display.fillCircle(cloudX + 4, cloudY + 4, 4, SSD1306_WHITE);
  display.fillCircle(cloudX + 8, cloudY + 2, 5, SSD1306_WHITE);
  display.fillCircle(cloudX + 12, cloudY + 4, 4, SSD1306_WHITE);
  display.fillRect(cloudX + 2, cloudY + 4, 12, 4, SSD1306_WHITE);

  display.drawLine(0, 50, 128, 50, SSD1306_WHITE);
  int dashOffset = (int)abs(groundOffset) % 40;
  for (int x = -dashOffset; x < 128; x += 40) {
    if (x >= 0) {
      display.drawLine(x, 52, x + 5, 52, SSD1306_WHITE);
      display.drawLine(x + 15, 55, x + 18, 55, SSD1306_WHITE);
    }
  }

  const unsigned char* dinoSprite = bmp_dino_run1;
  if (dinoIsJumping) {
    dinoSprite = bmp_dino_jump;
  } else {
    dinoSprite = ((now / 80) % 2 == 0) ? bmp_dino_run1 : bmp_dino_run2;
  }
  display.drawBitmap(16, (int)dinoY, dinoSprite, 16, 16, SSD1306_WHITE);

  for (int i = 0; i < 2; i++) {
    if (obstacles[i].active) {
      const unsigned char* obsSprite = bmp_cactus1;
      if (obstacles[i].type == 1) obsSprite = bmp_cactus2;
      else if (obstacles[i].type == 2) obsSprite = bmp_rock;
      display.drawBitmap((int)obstacles[i].x, obstacles[i].y, obsSprite, obstacles[i].width, obstacles[i].height, SSD1306_WHITE);
    }
  }

  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(55, 2);
  char scoreStr[20];
  sprintf(scoreStr, "HI %05d  %05d", dinoHighScore, dinoScore);
  display.print(scoreStr);
}

void drawDinoGameOverPage() {
  updatePhysicsAndMood();
  
  drawUltraProEye(leftEye, true, MOOD_SAD);
  drawUltraProEye(rightEye, false, MOOD_SAD);
  drawMouth(64, 28, MOOD_SAD);

  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(35, 38);
  display.print("GAME OVER");
  char scoreStr[30];
  sprintf(scoreStr, "Score:%d  Best:%d", dinoScore, dinoHighScore);
  display.setCursor(14, 48);
  display.print(scoreStr);
  display.setCursor(6, 56);
  display.print("1x: Menu | 2x: Replay");
}

void drawMoodCheckinPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(28, 2);
  display.print("HOW ARE YOU?");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* moods[] = { "HAPPY", "SAD", "TIRED", "ANGRY", "SURP" };
  const char* icons[] = { ":-)", ":-(", "zZz", ">:<", "o_o" };
  
  int x = 4;
  for (int i = 0; i < 5; i++) {
    if (i == moodSelectIndex) {
      display.fillRect(x - 2, 16, 24, 22, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(x, 18);
    display.print(icons[i]);
    display.setCursor(x - 1, 28);
    display.print(moods[i]);
    
    x += 25;
  }

  // Back option
  if (moodSelectIndex == 5) {
    display.fillRect(100, 40, 24, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(102, 41);
  display.print("Back");

  // Draw Mood History Log
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 52);
  display.print("Log: ");
  int hX = 35;
  for (int i = 0; i < moodHistorySize; i++) {
    int hMood = moodHistory[i];
    if (hMood >= 0 && hMood < 5) {
      display.setCursor(hX, 52);
      display.print(icons[hMood]);
      hX += 18;
    }
  }
}

void drawMoodResponsePage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 8);
  display.print(wellnessMessage);
  
  int stage = 0;
  if (petXP > 800) stage = 3;
  else if (petXP > 400) stage = 2;
  else if (petXP > 150) stage = 1;

  // Let pet wave to comfort or celebrate
  drawPetSprite(56, 44, false, false, true);

  if (millis() - moodResponseStart > 4000) {
    currentState = STATE_CLOCK;
  }
}

void drawPetStatsPage() {
  updatePhysicsAndMood();
  
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  drawBLEIcon(120, 2);

  // 1. Draw split-screen vertical divider line
  display.drawLine(48, 0, 48, 64, SSD1306_WHITE);

  // 2. Draw side eyes and mouth (X-offset = -12)
  drawFaceSide(-12, 0, MOOD_NORMAL);

  // 3. Draw pet strolling at the bottom-left centered
  bool isLowEnergy = (petEnergy < 20);
  bool isWaving = (petHappiness > 75) && ((millis() / 2000) % 2 == 0);
  drawPetSprite(16, 46, !isLowEnergy, isLowEnergy, isWaving);

  // 4. Draw stats info on the right side
  const char* stages[] = { "Baby", "Child", "Teen", "Adult" };
  int stage = 0;
  if (petXP > 800) stage = 3;
  else if (petXP > 400) stage = 2;
  else if (petXP > 150) stage = 1;

  // Stage name and XP
  display.setCursor(52, 2);
  display.print(stages[stage]);
  display.setCursor(52, 12);
  display.print("XP:");
  display.print(petXP);

  // Happy progress bar
  display.setCursor(52, 24);
  display.print("Hpy:");
  int hBarWidth = map((int)petHappiness, 0, 100, 0, 42);
  display.drawRect(80, 24, 44, 7, SSD1306_WHITE);
  display.fillRect(81, 25, hBarWidth, 5, SSD1306_WHITE);

  // Energy progress bar
  display.setCursor(52, 34);
  display.print("Eng:");
  int eBarWidth = map((int)petEnergy, 0, 100, 0, 42);
  display.drawRect(80, 34, 44, 7, SSD1306_WHITE);
  display.fillRect(81, 35, eBarWidth, 5, SSD1306_WHITE);

  // Feed, Play, Back actions
  const char* actions[] = { "Feed", "Play", "Back" };
  int boxX[] = { 51, 77, 103 };
  int textX[] = { 53, 79, 105 };

  for (int i = 0; i < 3; i++) {
    if (i == petActionIndex) {
      display.fillRect(boxX[i], 48, 24, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(textX[i], 50);
    display.print(actions[i]);
  }
}

void drawSettingsMenuPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(38, 2);
  display.print("SETTINGS");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { 
    "Brightness", "Sound", "Anim Speed", 
    "Pomo Duration", "Break Duration", "Clock Format", 
    "Sleep Timeout", "Pet Reset", "About", "Back" 
  };
  
  int startItem = settingsMenuIndex - 1;
  if (startItem < 0) startItem = 0;
  if (startItem + 3 > 10) startItem = 10 - 3;
  
  int y = 18;
  for (int i = startItem; i < startItem + 3; i++) {
    if (i == settingsMenuIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y);
    display.print(items[i]);
    y += 14;
  }
}

void drawSettingsBrightnessPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(28, 2);
  display.print("BRIGHTNESS");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { "High", "Low", "Back" };
  int y = 20;
  for (int i = 0; i < 3; i++) {
    if (i == settingsBrightnessIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.print(items[i]);
    if (i == 0 && screenHighBrightness) display.print(" *");
    if (i == 1 && !screenHighBrightness) display.print(" *");
    y += 14;
  }
}

void drawSettingsSoundPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(44, 2);
  display.print("SOUND");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { "On", "Off", "Back" };
  int y = 20;
  for (int i = 0; i < 3; i++) {
    if (i == settingsSoundIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.print(items[i]);
    if (i == 0 && soundOn) display.print(" *");
    if (i == 1 && !soundOn) display.print(" *");
    y += 14;
  }
}

void drawSettingsSpeedPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 2);
  display.print("ANIMATION SPEED");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { "Normal", "Fast", "Slow", "Back" };
  int y = 18;
  for (int i = 0; i < 4; i++) {
    if (i == settingsSpeedIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.print(items[i]);
    if (i == 0 && currentAnimSpeed == SPEED_NORMAL) display.print(" *");
    if (i == 1 && currentAnimSpeed == SPEED_FAST) display.print(" *");
    if (i == 2 && currentAnimSpeed == SPEED_SLOW) display.print(" *");
    y += 11;
  }
}

void drawSettingsPomoPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 2);
  display.print("POMO DURATION");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { "Time: ", "Save", "Back" };
  int y = 20;
  for (int i = 0; i < 3; i++) {
    if (i == settingsPomoIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.print(items[i]);
    if (i == 0) {
      display.print(pomoDuration);
      display.print(" min");
    }
    y += 14;
  }
}

void drawSettingsBreakPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 2);
  display.print("BREAK DURATION");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { "Time: ", "Save", "Back" };
  int y = 20;
  for (int i = 0; i < 3; i++) {
    if (i == settingsBreakIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.print(items[i]);
    if (i == 0) {
      display.print(pomoBreakDuration);
      display.print(" min");
    }
    y += 14;
  }
}

void drawSettingsFormatPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 2);
  display.print("CLOCK FORMAT");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { "12 Hour", "24 Hour", "Back" };
  int y = 20;
  for (int i = 0; i < 3; i++) {
    if (i == settingsFormatIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.print(items[i]);
    if (i == 0 && !clockFormat24h) display.print(" *");
    if (i == 1 && clockFormat24h) display.print(" *");
    y += 14;
  }
}

void drawSettingsSleepPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 2);
  display.print("SLEEP TIMEOUT");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  const char* items[] = { "Timeout: ", "Save", "Back" };
  int y = 20;
  for (int i = 0; i < 3; i++) {
    if (i == settingsSleepIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.print(items[i]);
    if (i == 0) {
      if (sleepTimeoutMins == 0) {
        display.print("Never");
      } else {
        display.print(sleepTimeoutMins);
        display.print(" min");
      }
    }
    y += 14;
  }
}

void drawSettingsResetPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(32, 2);
  display.print("RESET PET");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  display.setCursor(4, 16);
  display.print("Reset statistics?");

  const char* items[] = { "Reset Now", "Back" };
  int y = 30;
  for (int i = 0; i < 2; i++) {
    if (i == settingsResetIndex) {
      display.fillRect(10, y - 2, 108, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(16, y);
    display.print(items[i]);
    y += 14;
  }
}

void drawSettingsAboutPage() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(28, 2);
  display.print("ABOUT DEVIC");
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  display.setCursor(4, 16);
  display.print("Desk Buddy v2.0");
  display.setCursor(4, 26);
  display.print("Wellness Companion");
  display.setCursor(4, 36);
  display.print("Hardware: ESP32-C3");

  display.fillRect(0, 48, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(12, 50);
  display.print("> Double Tap: Back");
}

// ==================================================
// 10. INPUT ROUTER (State Machine Controller)
// ==================================================

void processInput() {
  if (pendingTap == 0) return;
  
  int tap = pendingTap;
  pendingTap = 0; 
  
  lastDinoInputTime = millis();
  
  // Wake from Clock Screen back to Menu
  if (currentState == STATE_CLOCK) {
    currentState = STATE_MENU;
    leftEye.targetH = 24; rightEye.targetH = 24;
    leftEye.targetW = 36; rightEye.targetW = 36;
    return;
  }
  
  switch (currentState) {
    case STATE_ONBOARDING_QR:
      if (tap == 2 || tap == 1) {
        currentState = STATE_MENU;
      }
      break;

    case STATE_BOOT:
      if (tap == 2) { 
        currentState = STATE_CLOCK;
      }
      break;
      
    case STATE_MENU:
      if (tap == 1) {
        selectedMenuItem = (selectedMenuItem + 1) % MENU_SIZE;
      } else if (tap == 2) {
        if (selectedMenuItem == 0) {
          currentState = STATE_CLOCK;
        } else if (selectedMenuItem == 1) {
          currentState = STATE_POMO_SETUP;
          pomoSetupIndex = 0;
        } else if (selectedMenuItem == 2) {
          currentState = STATE_PET_STATS;
          petActionIndex = 0;
        } else if (selectedMenuItem == 3) {
          currentState = STATE_MOOD_CHECKIN;
          moodSelectIndex = 0;
        } else if (selectedMenuItem == 4) {
          currentState = STATE_DINO_START;
        } else if (selectedMenuItem == 5) {
          currentState = STATE_ONBOARDING_QR;
        } else if (selectedMenuItem == 6) {
          currentState = STATE_SETTINGS_MENU;
          settingsMenuIndex = 0;
        }
      }
      break;
      
    case STATE_POMO_SETUP:
      if (tap == 1) {
        pomoSetupIndex = (pomoSetupIndex + 1) % 3;
      } else if (tap == 2) {
        if (pomoSetupIndex == 0) {
          pomoDuration += 5;
          if (pomoDuration > 60) pomoDuration = 5;
        } else if (pomoSetupIndex == 1) {
          currentState = STATE_POMO_RUNNING;
          pomoTimerStart = millis();
          pomoTimerDurationMs = (unsigned long)pomoDuration * 60 * 1000;
        } else if (pomoSetupIndex == 2) {
          currentState = STATE_MENU;
        }
      }
      break;
      
    case STATE_POMO_RUNNING:
      if (tap == 2) { 
        currentState = STATE_POMO_SETUP;
      }
      break;
      
    case STATE_POMO_COMPLETE:
      currentState = STATE_POMO_BREAK;
      pomoTimerStart = millis();
      pomoTimerDurationMs = pomoBreakDuration * 60UL * 1000UL;
      pomoBreakCompleted = false;
      break;
      
    case STATE_POMO_BREAK:
      if (pomoBreakCompleted) {
        currentState = STATE_POMO_SETUP;
      } else {
        if (tap == 2) { 
          currentState = STATE_POMO_SETUP;
        }
      }
      break;
      
    case STATE_PET_STATS:
      if (tap == 1) {
        petActionIndex = (petActionIndex + 1) % 3;
      } else if (tap == 2) {
        if (petActionIndex == 0) { // Feed
          petEnergy += 30.0; if (petEnergy > 100.0) petEnergy = 100.0;
          petXP += 5;
          saveConfig();
        } else if (petActionIndex == 1) { // Play
          if (petEnergy > 15) {
            petHappiness += 20.0; if (petHappiness > 100.0) petHappiness = 100.0;
            petEnergy -= 15.0;
            petXP += 10;
            saveConfig();
          }
        } else if (petActionIndex == 2) { // Back
          currentState = STATE_MENU;
        }
      }
      break;

    case STATE_MOOD_CHECKIN:
      if (tap == 1) {
        moodSelectIndex = (moodSelectIndex + 1) % 6;
      } else if (tap == 2) {
        if (moodSelectIndex < 5) {
          // Push mood log history
          for (int i = 4; i > 0; i--) {
            moodHistory[i] = moodHistory[i - 1];
          }
          moodHistory[0] = moodSelectIndex;
          if (moodHistorySize < 5) moodHistorySize++;

          // Select supportive message
          if (moodSelectIndex == 0) {
            wellnessMessage = "Awesome! Keep shining!\nGood vibes only! :-)";
            petHappiness += 10.0; if (petHappiness > 100.0) petHappiness = 100.0;
          } else if (moodSelectIndex == 1) {
            wellnessMessage = "It's okay to feel sad.\nPet is here for you! <3";
            petHappiness += 20.0; if (petHappiness > 100.0) petHappiness = 100.0;
          } else if (moodSelectIndex == 2) {
            wellnessMessage = "Take a deep breath.\nStretch and rest! zZz";
          } else if (moodSelectIndex == 3) {
            wellnessMessage = "Breath out anger...\nFind your peace. >:<";
          } else {
            wellnessMessage = "Life has surprises!\nEnjoy the wonder! o_o";
          }

          petXP += 20;
          saveConfig();

          currentState = STATE_MOOD_RESPONSE;
          moodResponseStart = millis();
        } else {
          currentState = STATE_MENU;
        }
      }
      break;

    case STATE_MOOD_RESPONSE:
      currentState = STATE_CLOCK;
      break;

    case STATE_DINO_START:
      if (tap == 1) {
        currentState = STATE_MENU;
      } else if (tap == 2) {
        currentState = STATE_DINO_PLAY;
        dinoScore = 0;
        gameSpeed = 3.0;
        dinoY = 34.0;
        dinoVelocityY = 0.0;
        dinoIsJumping = false;
        obstacles[0].active = false;
        obstacles[1].active = false;
      }
      break;
      
    case STATE_DINO_PLAY:
      break;
      
    case STATE_DINO_GAMEOVER:
      if (tap == 1) {
        currentState = STATE_MENU;
      } else if (tap == 2) {
        currentState = STATE_DINO_PLAY;
        dinoScore = 0;
        gameSpeed = 3.0;
        dinoY = 34.0;
        dinoVelocityY = 0.0;
        dinoIsJumping = false;
        obstacles[0].active = false;
        obstacles[1].active = false;
      }
      break;
      
    case STATE_SETTINGS_MENU:
      if (tap == 1) {
        settingsMenuIndex = (settingsMenuIndex + 1) % 10;
      } else if (tap == 2) {
        if (settingsMenuIndex == 0) {
          currentState = STATE_SETTINGS_BRIGHTNESS; settingsBrightnessIndex = 0;
        } else if (settingsMenuIndex == 1) {
          currentState = STATE_SETTINGS_SOUND; settingsSoundIndex = 0;
        } else if (settingsMenuIndex == 2) {
          currentState = STATE_SETTINGS_SPEED; settingsSpeedIndex = 0;
        } else if (settingsMenuIndex == 3) {
          currentState = STATE_SETTINGS_POMO_TIME; settingsPomoIndex = 0;
        } else if (settingsMenuIndex == 4) {
          currentState = STATE_SETTINGS_BREAK_TIME; settingsBreakIndex = 0;
        } else if (settingsMenuIndex == 5) {
          currentState = STATE_SETTINGS_FORMAT; settingsFormatIndex = 0;
        } else if (settingsMenuIndex == 6) {
          currentState = STATE_SETTINGS_SLEEP; settingsSleepIndex = 0;
        } else if (settingsMenuIndex == 7) {
          currentState = STATE_SETTINGS_PET_RESET; settingsResetIndex = 0;
        } else if (settingsMenuIndex == 8) {
          currentState = STATE_SETTINGS_ABOUT;
        } else if (settingsMenuIndex == 9) {
          currentState = STATE_MENU;
        }
      }
      break;
      
    case STATE_SETTINGS_BRIGHTNESS:
      if (tap == 1) {
        settingsBrightnessIndex = (settingsBrightnessIndex + 1) % 3;
      } else if (tap == 2) {
        if (settingsBrightnessIndex == 0) {
          screenHighBrightness = true;
          display.ssd1306_command(SSD1306_SETCONTRAST);
          display.ssd1306_command(255);
          saveConfig();
        } else if (settingsBrightnessIndex == 1) {
          screenHighBrightness = false;
          display.ssd1306_command(SSD1306_SETCONTRAST);
          display.ssd1306_command(1);
          saveConfig();
        }
        currentState = STATE_SETTINGS_MENU;
      }
      break;

    case STATE_SETTINGS_SOUND:
      if (tap == 1) {
        settingsSoundIndex = (settingsSoundIndex + 1) % 3;
      } else if (tap == 2) {
        if (settingsSoundIndex == 0) {
          soundOn = true; saveConfig();
        } else if (settingsSoundIndex == 1) {
          soundOn = false; saveConfig();
        }
        currentState = STATE_SETTINGS_MENU;
      }
      break;
      
    case STATE_SETTINGS_SPEED:
      if (tap == 1) {
        settingsSpeedIndex = (settingsSpeedIndex + 1) % 4;
      } else if (tap == 2) {
        if (settingsSpeedIndex == 0) {
          currentAnimSpeed = SPEED_NORMAL; saveConfig();
        } else if (settingsSpeedIndex == 1) {
          currentAnimSpeed = SPEED_FAST; saveConfig();
        } else if (settingsSpeedIndex == 2) {
          currentAnimSpeed = SPEED_SLOW; saveConfig();
        }
        currentState = STATE_SETTINGS_MENU;
      }
      break;
      
    case STATE_SETTINGS_POMO_TIME:
      if (tap == 1) {
        settingsPomoIndex = (settingsPomoIndex + 1) % 3;
      } else if (tap == 2) {
        if (settingsPomoIndex == 0) {
          pomoDuration += 5;
          if (pomoDuration > 60) pomoDuration = 5;
        } else {
          saveConfig();
          currentState = STATE_SETTINGS_MENU;
        }
      }
      break;

    case STATE_SETTINGS_BREAK_TIME:
      if (tap == 1) {
        settingsBreakIndex = (settingsBreakIndex + 1) % 3;
      } else if (tap == 2) {
        if (settingsBreakIndex == 0) {
          pomoBreakDuration += 5;
          if (pomoBreakDuration > 30) pomoBreakDuration = 5;
        } else {
          saveConfig();
          currentState = STATE_SETTINGS_MENU;
        }
      }
      break;

    case STATE_SETTINGS_FORMAT:
      if (tap == 1) {
        settingsFormatIndex = (settingsFormatIndex + 1) % 3;
      } else if (tap == 2) {
        if (settingsFormatIndex == 0) {
          clockFormat24h = false; saveConfig();
        } else if (settingsFormatIndex == 1) {
          clockFormat24h = true; saveConfig();
        }
        currentState = STATE_SETTINGS_MENU;
      }
      break;

    case STATE_SETTINGS_SLEEP:
      if (tap == 1) {
        settingsSleepIndex = (settingsSleepIndex + 1) % 3;
      } else if (tap == 2) {
        if (settingsSleepIndex == 0) {
          if (sleepTimeoutMins == 0) sleepTimeoutMins = 1;
          else if (sleepTimeoutMins == 1) sleepTimeoutMins = 5;
          else if (sleepTimeoutMins == 5) sleepTimeoutMins = 10;
          else sleepTimeoutMins = 0; // Never
        } else {
          saveConfig();
          currentState = STATE_SETTINGS_MENU;
        }
      }
      break;

    case STATE_SETTINGS_PET_RESET:
      if (tap == 1) {
        settingsResetIndex = (settingsResetIndex + 1) % 2;
      } else if (tap == 2) {
        if (settingsResetIndex == 0) {
          petXP = 0;
          petHappiness = 50.0;
          petEnergy = 50.0;
          saveConfig();
        }
        currentState = STATE_SETTINGS_MENU;
      }
      break;
      
    case STATE_SETTINGS_ABOUT:
      currentState = STATE_SETTINGS_MENU;
      break;
      
    case STATE_IDLE:
      break;
  }
}

// ==================================================
// 11. BOOT & INITIALIZATION
// ==================================================

void drawOnboardingQRPage() {
  display.clearDisplay();
  display.setFont(NULL);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 10);
  display.print("Scan");
  display.setCursor(2, 22);
  display.print("QR to");
  display.setCursor(2, 34);
  display.print("pair");
  display.setCursor(2, 46);
  display.print("phone");

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String url = "https://christadarynp2023.github.io/deskbuddy?mac=" + mac;
  QR::draw(display, url.c_str(), 2);
}

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000); 
  pinMode(TOUCH_PIN, INPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);

  bool forceConfig = false;
  for (unsigned long t = millis(); millis() - t < CONFIG_HOLD_MS; ) {
    if (digitalRead(TOUCH_PIN)) { forceConfig = true; break; }
    delay(80);
  }

  loadConfig();

  if (forceConfig) {
    startConfigPortal();
    return;
  }

  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(screenHighBrightness ? 255 : 1);

  // Initialize Spring Eyes
  leftEye.init(18, 12, 36, 24);
  rightEye.init(74, 12, 36, 24);

  // Initialize BLE Server
  initBLE();

  // Background Asynchronous Wi-Fi Connection
  initWiFi();

  currentState = STATE_BOOT;
  bootStage = 0;
  bootStateTime = millis();
  
  lastDinoInputTime = millis();
  lastPetUpdate = millis();
}

// ==================================================
// 12. MAIN EXECUTION LOOP
// ==================================================

void loop() {
  updateWiFi();
  if (inConfigMode) {
    return;
  }

  handleTouch();
  processInput();

  unsigned long now = millis();

  // Decay Pet Stats slowly (every 1 minute)
  if (now - lastPetUpdate > 60000) {
    lastPetUpdate = now;
    
    // Decrement energy
    petEnergy -= 0.5;
    if (petEnergy < 0.0) petEnergy = 0.0;
    
    // Decrement happiness
    petHappiness -= 0.2;
    if (petHappiness < 0.0) petHappiness = 0.0;
    
    saveConfig();
  }
  
  // Custom framerate scaling
  unsigned long delayVal = 25; // 40 FPS
  if (currentAnimSpeed == SPEED_FAST) delayVal = 12;
  else if (currentAnimSpeed == SPEED_SLOW) delayVal = 50;

  if (now - lastFrameTime >= delayVal) {
    lastFrameTime = now;

    // Check Sleep Mode Screen turn-off timeout
    if (sleepTimeoutMins > 0 && !displayIsOff) {
      if (now - lastDinoInputTime > (unsigned long)sleepTimeoutMins * 60 * 1000) {
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        displayIsOff = true;
        currentState = STATE_CLOCK; // Turn off on clock screen
      }
    }

    // Auto-return to clock after 20 seconds of menu inactivity
    if (currentState == STATE_MENU && (now - lastDinoInputTime > 20000)) {
      currentState = STATE_CLOCK;
    }

    if (!displayIsOff) {
      display.clearDisplay();

      if (isIncomingCall) {
        drawIncomingCallOverlay();
      } else {
        switch (currentState) {
          case STATE_BOOT:
            runBootSequence();
            break;
          case STATE_CLOCK:
            drawClockPage();
            break;
          case STATE_MENU:
            drawMenuPage();
            break;
          case STATE_POMO_SETUP:
            drawPomoSetupPage();
            break;
          case STATE_POMO_RUNNING:
            drawPomoRunningPage();
            break;
          case STATE_POMO_COMPLETE:
            drawPomoCompletePage();
            break;
          case STATE_POMO_BREAK:
            drawPomoBreakPage();
            break;
          case STATE_DINO_START:
            drawDinoStartPage();
            break;
          case STATE_DINO_PLAY:
            drawDinoGamePage();
            break;
          case STATE_DINO_GAMEOVER:
            drawDinoGameOverPage();
            break;
          case STATE_MOOD_CHECKIN:
            drawMoodCheckinPage();
            break;
          case STATE_MOOD_RESPONSE:
            drawMoodResponsePage();
            break;
          case STATE_PET_STATS:
            drawPetStatsPage();
            break;
          case STATE_SETTINGS_MENU:
            drawSettingsMenuPage();
            break;
          case STATE_SETTINGS_BRIGHTNESS:
            drawSettingsBrightnessPage();
            break;
          case STATE_SETTINGS_SOUND:
            drawSettingsSoundPage();
            break;
          case STATE_SETTINGS_SPEED:
            drawSettingsSpeedPage();
            break;
          case STATE_SETTINGS_POMO_TIME:
            drawSettingsPomoPage();
            break;
          case STATE_SETTINGS_BREAK_TIME:
            drawSettingsBreakPage();
            break;
          case STATE_SETTINGS_FORMAT:
            drawSettingsFormatPage();
            break;
          case STATE_SETTINGS_SLEEP:
            drawSettingsSleepPage();
            break;
          case STATE_SETTINGS_PET_RESET:
            drawSettingsResetPage();
            break;
          case STATE_SETTINGS_ABOUT:
            drawSettingsAboutPage();
            break;
          case STATE_ONBOARDING_QR:
            drawOnboardingQRPage();
            break;
          case STATE_IDLE:
            // Unused - direct clock rendering
            break;
        }
      }

      display.display();
    }
  }
}
