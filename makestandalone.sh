#!/bin/bash
if [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
    # Do something under Mac OS X platform        
    SYSTEM="linux"
elif [ "$(expr substr $(uname -s) 1 10)" == "MINGW64_NT" ]; then
    # Do something under 64 bits Windows NT platform
    SYSTEM="windows"
else
exit
fi

echo 'When making a stand alone application, we need to specify the -s'
echo 'option on the hcustom command.'

if [ ! -d build ]; then
    mkdir build
fi

if [ ! -d build/standalone ]; then
    mkdir build/standalone
fi

cd build/standalone
if [ $SYSTEM = "windows" ]; then
	HDK="C:/Program Files/Side Effects Software/Houdini 19.0.498/bin"
	"$HDK/hcustom" -s ../../src/standalone/geoisosurface.c -i ../install/standalone
else
	hcustom -s ../../src/standalone/geoisosurface.c -i ../install/standalone
fi


cd ../../
