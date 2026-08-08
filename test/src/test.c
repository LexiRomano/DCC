#include "test.h"

uint8_t something;
unsigned short int myVar;

short unsigned int emptyFunc();

void myFunc(int param1, unsigned char param2, uint8_t param3)
{
    something = ~param1 + (myVar << 3);
}

uint8_t anotherOne;
int anotherTwo;
