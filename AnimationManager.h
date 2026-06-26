#ifndef ANIMATION_MANAGER_H
#define ANIMATION_MANAGER_H

#include <Arduino.h>
#include "DisplayManager.h"

// Spring physics Eye structure


// Global companion eyes
Eye leftEye, rightEye;
unsigned long lastSaccade = 0;
unsigned long saccadeInterval = 3000;
float breathVal = 0;

// Updates the gaze direction and Spring dynamics of eyes
void updatePhysicsAndMood() {
  unsigned long now = millis();
  breathVal = sin(now / 800.0) * 1.5;

  if (currentState != STATE_BOOT) {
    if (now > leftEye.nextBlinkTime) {
      leftEye.blinking = true;
      leftEye.lastBlink = now;
      rightEye.blinking = true;
      leftEye.nextBlinkTime = now + random(10000, 25000); 
    }
    if (leftEye.blinking) {
      leftEye.targetH = 2;
      rightEye.targetH = 2;
      if (now - leftEye.lastBlink > 120) {
        leftEye.blinking = false;
        rightEye.blinking = false;
      }
    }
  }

  // Base layout scale factors
  float targetEyeY = 12;
  float targetEyeW = 36;
  float targetEyeH = 24; 
  int moodToUse = currentMood;

  if (currentState == STATE_POMO_RUNNING || currentState == STATE_PET_STATS || currentState == STATE_CLOCK) {
    targetEyeY = 6;
    targetEyeW = 22;
    targetEyeH = 16;
    moodToUse = MOOD_NORMAL;
  } else if (currentState == STATE_POMO_BREAK) {
    targetEyeY = 6;
    targetEyeW = 22;
    targetEyeH = 16;
    moodToUse = MOOD_HAPPY;
  }

  if (!leftEye.blinking) {
    targetEyeH += breathVal;

    switch (moodToUse) {
      case MOOD_NORMAL:
        leftEye.targetW = targetEyeW;
        leftEye.targetH = targetEyeH;
        rightEye.targetW = targetEyeW;
        rightEye.targetH = targetEyeH;
        break;
      case MOOD_HAPPY:
      case MOOD_LOVE:
        leftEye.targetW = (currentState == STATE_CLOCK || currentState == STATE_PET_STATS) ? 26 : 40;
        leftEye.targetH = (currentState == STATE_CLOCK || currentState == STATE_PET_STATS) ? 22 : 32;
        rightEye.targetW = (currentState == STATE_CLOCK || currentState == STATE_PET_STATS) ? 26 : 40;
        rightEye.targetH = (currentState == STATE_CLOCK || currentState == STATE_PET_STATS) ? 22 : 32;
        break;
      case MOOD_SURPRISED:
        leftEye.targetW = targetEyeW - 4;
        leftEye.targetH = targetEyeH + 6;
        rightEye.targetW = targetEyeW - 4;
        rightEye.targetH = targetEyeH + 6;
        break;
      case MOOD_SLEEPY:
        leftEye.targetW = targetEyeW + 2;
        leftEye.targetH = targetEyeH - 4;
        rightEye.targetW = targetEyeW + 2;
        rightEye.targetH = targetEyeH - 4;
        break;
      case MOOD_ANGRY:
        leftEye.targetW = targetEyeW - 2;
        leftEye.targetH = targetEyeH - 4;
        rightEye.targetW = targetEyeW - 2;
        rightEye.targetH = targetEyeH - 4;
        break;
      case MOOD_SAD:
        leftEye.targetW = targetEyeW - 2;
        leftEye.targetH = targetEyeH + 2;
        rightEye.targetW = targetEyeW - 2;
        rightEye.targetH = targetEyeH + 2;
        break;
      case MOOD_SUSPICIOUS:
        leftEye.targetW = targetEyeW;
        leftEye.targetH = targetEyeH - 8;
        rightEye.targetW = targetEyeW;
        rightEye.targetH = targetEyeH + 4;
        break;
      case MOOD_COOL:
        leftEye.targetW = targetEyeW;
        leftEye.targetH = targetEyeH;
        rightEye.targetW = targetEyeW;
        rightEye.targetH = targetEyeH;
        break;
    }
  }

  // Gaze saccades updates
  if (!leftEye.blinking && now - lastSaccade > saccadeInterval && currentState != STATE_BOOT) {
    lastSaccade = now;
    saccadeInterval = random(1200, 5000);

    int dir = random(0, 10);
    float lx = 0, ly = 0;

    if (currentState == STATE_POMO_RUNNING || currentState == STATE_CLOCK) {
      ly = 3.0; // Gaze downwards slightly
    } else {
      if (dir < 4) { lx = 0; ly = 0; }
      else if (dir == 4) { lx = -5; ly = -3; }
      else if (dir == 5) { lx = 5; ly = -3; }
      else if (dir == 6) { lx = -5; ly = 3; }
      else if (dir == 7) { lx = 5; ly = 3; }
      else if (dir == 8) { lx = 7; ly = 0; }
      else if (dir == 9) { lx = -7; ly = 0; }
    }

    leftEye.targetPupilX = lx;
    leftEye.targetPupilY = ly;
    rightEye.targetPupilX = lx;
    rightEye.targetPupilY = ly;

    if (currentState == STATE_POMO_RUNNING || currentState == STATE_POMO_BREAK || currentState == STATE_PET_STATS || currentState == STATE_CLOCK) {
      leftEye.targetX = 10 + (lx * 0.15);
      leftEye.targetY = targetEyeY + (ly * 0.15);
      rightEye.targetX = 40 + (lx * 0.15);
      rightEye.targetY = targetEyeY + (ly * 0.15);
    } else {
      leftEye.targetX = 18 + (lx * 0.3);
      leftEye.targetY = targetEyeY + (ly * 0.3);
      rightEye.targetX = 74 + (lx * 0.3);
      rightEye.targetY = targetEyeY + (ly * 0.3);
    }
  }

  leftEye.update();
  rightEye.update();
}

// Draws the face shifted to the side for split layouts (Clock/Pet Stats/Pomo)
void drawFaceSide(int xOffset, int yOffset, int mood) {
  float lx = leftEye.x; float ly = leftEye.y;
  float rx = rightEye.x; float ry = rightEye.y;
  
  leftEye.x += xOffset; leftEye.y += yOffset;
  rightEye.x += xOffset; rightEye.y += yOffset;

  drawUltraProEye(leftEye, true, mood);
  drawUltraProEye(rightEye, false, mood);
  drawMouth(38 + xOffset, 22 + yOffset, mood);

  leftEye.x = lx; leftEye.y = ly;
  rightEye.x = rx; rightEye.y = ry;
}

// Renders the ringing screen overlay on top of any active state
void drawIncomingCallOverlay() {
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(24, 2);
  display.print("Incoming Call");

  display.setFont(&FreeSansBold9pt7b);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(callerInfo.c_str(), 0, 0, &x1, &y1, &w, &h);
  int textX = (128 - w) / 2;
  if (textX < 0) textX = 0;
  display.setCursor(textX, 26);
  display.print(callerInfo);

  static unsigned long lastToggle = 0;
  static bool eyeState = false;
  if (millis() - lastToggle > 300) {
    lastToggle = millis();
    eyeState = !eyeState;
  }

  int savedMood = currentMood;
  currentMood = eyeState ? MOOD_SURPRISED : MOOD_NORMAL;

  leftEye.targetW = 20; leftEye.targetH = 16;
  rightEye.targetW = 20; rightEye.targetH = 16;
  if (currentMood == MOOD_SURPRISED) {
    leftEye.targetW = 16; leftEye.targetH = 22;
    rightEye.targetW = 16; rightEye.targetH = 22;
  }
  leftEye.targetX = 38; leftEye.targetY = 36;
  rightEye.targetX = 70; rightEye.targetY = 36;

  leftEye.update(); rightEye.update();
  drawUltraProEye(leftEye, true, currentMood);
  drawUltraProEye(rightEye, false, currentMood);
  drawMouth(64, 48, currentMood);

  currentMood = savedMood;
}

#endif
