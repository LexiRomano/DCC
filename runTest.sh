#!/usr/bin/bash

./make.sh && cd test && ../dcc src/* -I include/* -k
