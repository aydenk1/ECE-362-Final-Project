#ifndef __EXTRAFUNCTIONS_H__
#define __EXTRAFUNCTIONS_H__


//Game Specific Functions
//=============================================================================
void nano_wait(unsigned int n);
void setupAmmoLEDs();
void updateAmmoLEDs(int ammo);
void setup_tim17();
char check_key();
void updateScore(int score);
void setupOLED();
void setPercentTable(const int fireDly);
void updateOLED(int fireRdy);
void initOLED();

//Display Functions
//=============================================================================
void setup_spi1_displayTFT();

//Keypad and Seven-Segment Functions
//=============================================================================
void enable_keypad_display();
void TIM7_IRQHandler(void);
int getkey();

#endif /* __EXTRAFUNCTIONS_H__ */
