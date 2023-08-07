#!/bin/bash

PIPE="/home/dave/source/restream/pipes/channel"$1".mkv"

function SendToPipe
{
#/usr/bin/ffmpeg -loglevel fatal -follow 1 -fflags +genpts -analyzeduration 64 -i $PIPE -c:v libx264 -ar 48000 -ac 2 -c:a libfdk_aac -f mpegts pipe:1
#/usr/bin/ffmpeg -loglevel fatal -i $PIPE -vf scale=360:240 -c:v libx264 -ac 2 -c:a ac3 -f mpegts pipe:1
#/usr/bin/ffmpeg -i $PIPE -c:v libx264 -c:a libfdk_aac -f mpegts pipe:1
#/usr/bin/ffmpeg -loglevel fatal -i $PIPE -c:v libx264 -ac 2 -c:a aac -f mpegts pipe:1
#/usr/bin/ffmpeg -loglevel fatal -i $PIPE -c:v copy -ac 2 -c:a ac3 -f mpegts pipe:1
#/usr/bin/ffmpeg -loglevel fatal -i $PIPE -c:v libx264 -ac 2 -c:a ac3 -f mpegts pipe:1
/usr/bin/ffmpeg -loglevel fatal -i $PIPE -fflags +genpts -c:v copy -ac 2 -c:a aac -f mpegts pipe:1
}


while true
do
  SendToPipe
  sleep 1
done

exit 0
