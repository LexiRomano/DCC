#include "test.h"

uint8_t something;
unsigned short int myVar;

short unsigned int emptyFunc();

void anotherFunc(int a, int b)
{
    a + b;
}

int myFunc(short int param1, unsigned char param2, uint8_t param3)
{
    int a;
    int b;

    anotherFunc(a + b, b);

    return 3;
}

void main()
{
    myFunc(1, 2, 3);

    return;
}

uint8_t anotherOne;
int anotherTwo;
