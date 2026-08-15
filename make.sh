#!/usr/bin/bash

gcc -Iinclude include/*.h src/*.c -o dcc -Wall -Werror -g || rm dcc
