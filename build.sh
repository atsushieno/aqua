#!/bin/sh

g++ -g -fpermissive -I external/webview/ -I external/httpserver.h/ aria2web-host.c `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o aria2web-host 
