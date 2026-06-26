#ifndef QR_GENERATOR_H
#define QR_GENERATOR_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "qrcode.h"

namespace QR {

// Draws the QR code onto the display
void draw(Adafruit_SSD1306& display, const char* text, int scale = 1) {
  QRCode qrcode;
  
  // Decide QR version based on length of input text
  int len = strlen(text);
  int version = 3; // Default version 3 (29x29)
  
  if (len > 55) {
    version = 4; // Version 4 (33x33)
  }
  if (len > 80) {
    version = 5; // Version 5 (37x37)
  }

  // Adjust scale if version 4/5 is too large for the screen
  if (version >= 4 && scale > 1) {
    // A scale of 2 with version 4 takes 66px, which exceeds screen height of 64px.
    // So we limit scale to 1 for version >= 4 to keep it within screen boundary.
    scale = 1;
  }

  uint8_t qrcodeData[qrcode_getBufferSize(version)];
  qrcode_initText(&qrcode, qrcodeData, version, ECC_LOW, text);

  int qrSize = qrcode.size;
  int offset = (64 - (qrSize * scale)) / 2;
  int startX = 48 + (80 - (qrSize * scale)) / 2; // Center on right side of split

  // Draw background white frame
  display.fillRect(startX - 2, offset - 2, (qrSize * scale) + 4, (qrSize * scale) + 4, SSD1306_WHITE);

  for (uint8_t y = 0; y < qrSize; y++) {
    for (uint8_t x = 0; x < qrSize; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        display.fillRect(startX + (x * scale), offset + (y * scale), scale, scale, SSD1306_BLACK);
      }
    }
  }
}

} // namespace QR

#endif
