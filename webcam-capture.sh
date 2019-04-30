#! /bin/bash
#
HOME="/home/pi"
DIR=`date +"%Y%m%d"`
IMGNUM=`date +"%H%M"`
#
if [ ! -d "$HOME/Images/$DIR" ]; then
  mkdir $HOME/Images/$DIR
fi
#
fswebcam -S 30 -r 640x480 --jpeg 95 --timestamp "%Y-%m-%d %H:%M MST" $HOME/Images/latest-image.jpg
SIZE=`wc -c $HOME/Images/latest-image.jpg |awk '{print $1}'`
if [ "$SIZE" -gt 20000 ]; then
  cp $HOME/Images/latest-image.jpg $HOME/Images/$DIR/img$IMGNUM.jpg
fi
