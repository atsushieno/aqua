#!/bin/sh

g++ -fpermissive -I external/webview/ -I external/httpserver.h/ -I external/uriparser2/ external/uriparser2/uriparser2.c external/uriparser2/uriparser/*.c aria2web-host.c `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o aria2web-host 
