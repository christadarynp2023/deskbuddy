#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// StorageManager encapsulates non-volatile storage via the Preferences API.
// Note: Assumes variables are declared in the global scope before this file is included.

namespace Storage {

Preferences prefs;

void loadConfig() {
  prefs.begin("deskbuddy", true);
  wifiSsid      = prefs.getString("ssid", "");
  wifiPass      = prefs.getString("pass", "");
  tzString      = prefs.getString("tz", "");
  dinoHighScore = prefs.getUInt("dinohi", 0);
  
  currentEyeStyle      = (EyeStyle)prefs.getInt("eyestyle", (int)STYLE_CLASSIC);
  currentAnimSpeed     = (AnimSpeed)prefs.getInt("animspeed", (int)SPEED_NORMAL);
  screenHighBrightness = prefs.getBool("brightness", true);
  soundOn              = prefs.getBool("sound", true);
  clockFormat24h       = prefs.getBool("format24h", false);
  sleepTimeoutMins     = prefs.getInt("sleep", 5);
  pomoDuration         = prefs.getInt("pomotime", 25);
  pomoBreakDuration    = prefs.getInt("breaktime", 5);
  
  // Pet loading
  petXP        = prefs.getInt("petxp", 0);
  petHappiness = prefs.getFloat("pethappy", 50.0);
  petEnergy    = prefs.getFloat("petenergy", 50.0);

  // Mood history loading
  moodHistorySize = prefs.getInt("hsize", 0);
  for (int i = 0; i < 5; i++) {
    char key[6];
    sprintf(key, "mh%d", i);
    moodHistory[i] = prefs.getInt(key, -1);
  }
  prefs.end();

  // Fallback defaults
  if (wifiSsid.isEmpty()) {
    wifiSsid    = "edison science corner";
    wifiPass    = "eeeeeeee";
    tzString    = "IST-5:30";
  } else {
    if (tzString.isEmpty()) tzString = "IST-5:30";
  }
}

void saveConfig() {
  prefs.begin("deskbuddy", false);
  prefs.putInt("eyestyle", (int)currentEyeStyle);
  prefs.putInt("animspeed", (int)currentAnimSpeed);
  prefs.putBool("brightness", screenHighBrightness);
  prefs.putBool("sound", soundOn);
  prefs.putBool("format24h", clockFormat24h);
  prefs.putInt("sleep", sleepTimeoutMins);
  prefs.putInt("pomotime", pomoDuration);
  prefs.putInt("breaktime", pomoBreakDuration);
  
  prefs.putInt("petxp", petXP);
  prefs.putFloat("pethappy", petHappiness);
  prefs.putFloat("petenergy", petEnergy);
  
  prefs.putInt("hsize", moodHistorySize);
  for (int i = 0; i < 5; i++) {
    char key[6];
    sprintf(key, "mh%d", i);
    prefs.putInt(key, moodHistory[i]);
  }
  prefs.end();
}

void saveWiFiConfig(const String& s, const String& p, const String& tz) {
  prefs.begin("deskbuddy", false);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.putString("tz", tz);
  prefs.end();
}

void saveDinoHighScore() {
  prefs.begin("deskbuddy", false);
  prefs.putUInt("dinohi", dinoHighScore);
  prefs.end();
}

void factoryReset() {
  prefs.begin("deskbuddy", false);
  prefs.clear();
  prefs.end();
  ESP.restart();
}

} // namespace Storage

#endif
