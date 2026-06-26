#ifndef COMPANION_PET_H
#define COMPANION_PET_H

#include <Arduino.h>
#include "DisplayManager.h"

// CompanionPet manages the Virtual Pet simulation stats, physics coordinates, and stage drawings.
namespace Pet {

// Renders the pet bitmap based on energy/happiness levels and evolution stages
void drawPetSprite(int px, int py, bool isWalking, bool isSleeping, bool isWaving) {
  // Determine evolution stage based on XP
  int stage = 0;
  if (petXP > 800) stage = 3;
  else if (petXP > 400) stage = 2;
  else if (petXP > 150) stage = 1;

  const unsigned char* sprite = bmp_baby_idle;

  if (isSleeping || petEnergy < 20.0) {
    sprite = bmp_pet_sleep;
  } else if (isWaving) {
    sprite = bmp_pet_wave;
  } else {
    bool walkTick = (millis() / 250) % 2 == 0;
    switch (stage) {
      case 0:
        sprite = (isWalking && walkTick) ? bmp_baby_move : bmp_baby_idle;
        break;
      case 1:
        sprite = (isWalking && walkTick) ? bmp_child_walk : bmp_child_idle;
        break;
      case 2:
        sprite = (isWalking && walkTick) ? bmp_teen_walk : bmp_teen_idle;
        break;
      case 3:
        sprite = (isWalking && walkTick) ? bmp_adult_walk : bmp_adult_idle;
        break;
    }
  }
  display.drawBitmap(px, py, sprite, 16, 16, SSD1306_WHITE);
}

// Strolling updates (moves back and forth at the bottom-left of the clock screen)
void updatePetPosition() {
  if (petEnergy < 20.0) {
    return; // Stay sleeping, don't walk
  }

  float walkSpeed = 0.25;
  if (currentAnimSpeed == SPEED_FAST) walkSpeed = 0.45;
  else if (currentAnimSpeed == SPEED_SLOW) walkSpeed = 0.12;

  if (petWalkingLeft) {
    petX -= walkSpeed;
    if (petX <= 2.0) {
      petX = 2.0;
      petWalkingLeft = false;
    }
  } else {
    petX += walkSpeed;
    if (petX >= 30.0) { // Keep within left half (0 to 50) of the clock screen
      petX = 30.0;
      petWalkingLeft = true;
    }
  }
}

} // namespace Pet

#endif
