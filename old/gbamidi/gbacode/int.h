#ifndef INT_H
#define INT_H

#include "gba.h"
void InterruptProcess()  __attribute__((externally_visible));
void interruptsInit();

#define interruptsEnable() REG_IME=1;
#define interruptsDisable() REG_IME=0;

#endif
