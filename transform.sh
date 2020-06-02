#!/bin/sh

mkdir -p ui-metal-gtx/gen/GUI
cp -R ui-metal-gtx/source/GUI/ ui-metal-gtx/gen/
for i in ui-metal-gtx/source/GUI/*.xml ; do
  xsltproc aria2web.xsl "$i" > ui-metal-gtx/gen/GUI/`basename "$i" .xml`.html
done

mkdir -p ui-standard-guitar/gen/GUI
cp -R ui-standard-guitar/source/GUI/ ui-standard-guitar/gen/
for i in ui-standard-guitar/source/GUI/*.xml ; do
  xsltproc aria2web.xsl "$i" > ui-standard-guitar/gen/GUI/`basename "$i" .xml`.html
done

mkdir -p ui-1912/gen/GUI
cp -R ui-1912/source/GUI/ ui-1912/gen/
for i in ui-1912/source/GUI/*.xml ; do
  xsltproc aria2web.xsl "$i" > ui-1912/gen/GUI/`basename "$i" .xml`.html
done

mkdir -p karoryfer-bigcat.cello/gen/GUI
cp -R karoryfer-bigcat.cello/source/GUI/ karoryfer-bigcat.cello/gen/
for i in karoryfer-bigcat.cello/source/GUI/*.xml ; do
  xsltproc aria2web.xsl "$i" > karoryfer-bigcat.cello/gen/GUI/`basename "$i" .xml`.html
done
