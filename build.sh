#!/bin/sh

_CC=gcc
_CXX=g++

$_CXX -g -fpermissive -I external/webview/ -I external/httpserver.h/ \
	aria2web-host.c \
	`pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` \
	-o aria2web-host \
	|| exit 1
$_CXX -g -fpermissive -I external/webview/ -I external/httpserver.h/ \
	-I external/tiny-process-library \
	external/tiny-process-library/process.cpp \
	external/tiny-process-library/process_unix.cpp \
	aria2web-lv2ui.c \
	`pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` \
	-o aria2web-lv2ui.so -shared -fPIC -Wl,--no-undefined \
	|| exit 1

if [ ! -f a2w-sfizz.stamp ] ; then
  cd external/sfizz ;
  patch -i ../../sfizz-to-a2w.patch -p1 ;
  cd ../.. ;
  touch a2w-sfizz.stamp ;
fi

if [ ! -d sfizz-aria2web/dist ] ; then
  echo "building sfizz..." ;
  mkdir sfizz-aria2web ;
  cd sfizz-aria2web ;
  CXX=$_CXX CC=$_CC cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=`pwd`/dist ../external/sfizz ;
  make ;
  make install ;
  cd .. ;
fi

DSTLV2=sfizz-aria2web/dist/lib/lv2/sfizz.lv2/
cp aria2web-lv2ui.so $DSTLV2
cp aria2web.ttl $DSTLV2
cp manifest.ttl $DSTLV2
cp aria2web-host $DSTLV2
cp -R web $DSTLV2
echo "build successfully completed."
