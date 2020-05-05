#!/bin/sh

g++ -g -fpermissive -I external/webview/ -I external/httpserver.h/ aria2web-host.c `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o aria2web-host  || exit 1
g++ -g -fpermissive -I external/webview/ -I external/httpserver.h/ aria2web-lv2ui.c `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o aria2web-lv2ui.so -shared -fPIC || exit 1
echo "build successfully completed."
