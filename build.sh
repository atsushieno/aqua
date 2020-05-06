#!/bin/sh

g++ -g -fpermissive -I external/webview/ -I external/httpserver.h/ aria2web-host.c `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o aria2web-host  || exit 1
g++ -g -fpermissive -I external/webview/ -I external/httpserver.h/ aria2web-lv2ui.c `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o aria2web-lv2ui.so -shared -fPIC -Wl,--no-undefined || exit 1

if [ ! -d sfizz-aria2web ] ; then
  echo "building sfizz..." ;
  mkdir sfizz-aria2web ;
  cd sfizz-aria2web ;
  cmake -DCMAKE_INSTALL_PREFIX=`pwd`/dist ../external/sfizz ;
  make install ;
  cd .. ;
fi
cp aria2web-lv2ui.so sfizz-aria2web/dist/lib/lv2/sfizz.lv2/
cp sfizz-aria2web.ttl sfizz-aria2web/dist/lib/lv2/sfizz.lv2/
echo "build successfully completed."
