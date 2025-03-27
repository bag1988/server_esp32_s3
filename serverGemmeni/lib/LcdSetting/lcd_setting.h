#ifndef LCD_SETTING_H
#define LCD_SETTING_H

#define BUTTON_DEBOUNCE_DELAY 50  // 50 миллисекунд

void initLCD();
void initButtons();
void updateScrollText();
void updateLCD();
void handleButtons();

#endif
