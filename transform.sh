#!/bin/sh

mkdir -p ui-metal-gtx/gen/GUI
for i in ui-metal-gtx/source/GUI/*.xml ; do
  xsltproc aria2web.xsl "$i" > ui-metal-gtx/gen/GUI/`basename "$i" .xml`.html
done

mkdir -p ui-standard-guitar/gen/GUI
for i in ui-standard-guitar/source/GUI/*.xml ; do
  xsltproc aria2web.xsl "$i" > ui-standard-guitar/gen/GUI/`basename "$i" .xml`.html
done
