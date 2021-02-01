#!/bin/sh

_CC=gcc
_CXX=g++

if ! [ -f external/tiny-process-library/patch.stamp ] ; then
cd external/tiny-process-library && patch -i ../../tiny-process-library-vfork.patch -p1 && touch patch.stamp && cd ../.. || exit 1
fi

$_CXX -g -fpermissive -I external/webview/ -I external/httpserver.h/ \
	aqua-host.c \
	`pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` \
	-o aqua-host \
	|| exit 1
$_CXX -g -fpermissive -I external/webview/ -I external/httpserver.h/ \
	-I external/tiny-process-library \
	external/tiny-process-library/process.cpp \
	external/tiny-process-library/process_unix.cpp \
	aqua-lv2ui.c \
	`pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` \
	-o aqua-lv2ui.so -shared -fPIC -Wl,--no-undefined \
	|| exit 1

if [ ! -f aqua-sfizz.stamp ] ; then
  cd external/sfizz ;
  patch -i ../../sfizz-to-aqua.patch -p1 ;
  cd ../.. ;
  touch aqua-sfizz.stamp ;
fi

if [ ! -d sfizz-aqua/dist ] ; then
  echo "building sfizz..." ;
  mkdir sfizz-aqua ;
  cd sfizz-aqua ;
  CXX=$_CXX CC=$_CC cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=`pwd`/dist ../external/sfizz ;
  make || exit 1 ;
  make install || exit 1 ;
  cd .. ;
fi

mkdir dist
cp -R sfizz-aqua/dist/lib/lv2/sfizz.lv2/ dist/aqua.lv2
DSTLV2=dist/aqua.lv2
cp aqua-lv2ui.so $DSTLV2
cp aqua.ttl $DSTLV2
cp manifest.ttl $DSTLV2
cp aqua-host $DSTLV2
cp -R web $DSTLV2
echo "build successfully completed."
