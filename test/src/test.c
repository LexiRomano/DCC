#include "test.h"

uint8_t something;
unsigned short int myVar;

short unsigned int emptyFunc();

void anotherFunc(int a, int b)
{
    a + b;
}

void myFunc(short int param1, unsigned char param2, uint8_t param3)
{
    int a;
    int b;

    anotherFunc(a + b, b);

    //myFunc(param1, param2, param3);
}

uint8_t anotherOne;
int anotherTwo;
