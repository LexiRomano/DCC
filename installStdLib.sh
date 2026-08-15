#!/usr/bin/bash

./make.sh

if [ ! -e dcc ]
then
    exit
fi


sudo mkdir -p /usr/lib/dcc/include
sudo mkdir -p /usr/lib/dlib/include
sudo mkdir -p /usr/lib/dcc/objects
sudo mkdir -p /usr/lib/dlib/objects

cd stdlib

for dir in *
do
    echo $dir
    cd $dir
    if ../../dcc -O *.c
    then
        sudo mv *.dob /usr/lib/dcc/objects/.
        sudo cp *.c /usr/lib/dcc/include/.
    fi
    cd ..

done
