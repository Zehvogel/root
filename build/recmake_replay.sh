#!/bin/sh
rm -f CMakeCache.txt
/usr/local/bin/cmake  -Dcocoa="OFF" -Dminimal="ON" -Dx11="OFF" /home/runner/work/root/root 
