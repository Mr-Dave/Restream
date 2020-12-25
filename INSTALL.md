

Setup Configuration 
In a text file add entries such as this:
"/mnt/FDrive/Series/Nature/" "/home/dave/restream/pipes/channel77.mkv" "r"

Where
"/mnt/FDrive/Series/Nature/" is the directory of recordings
"/home/dave/restream/pipes/channel77.mkv" is the named pipe
"r" indicates randomize the entries, "a" would indicate alpha sorted

Add as many rows as desired, each will be a channel. 


Setup named pipes:

mkfifo /home/dave/source/restream/pipes/channel60.mkv
mkfifo /home/dave/source/restream/pipes/channel61.mkv
mkfifo /home/dave/source/restream/pipes/channel62.mkv
mkfifo /home/dave/source/restream/pipes/channel63.mkv
mkfifo /home/dave/source/restream/pipes/channel64.mkv
mkfifo /home/dave/source/restream/pipes/channel65.mkv
mkfifo /home/dave/source/restream/pipes/channel66.mkv
mkfifo /home/dave/source/restream/pipes/channel67.mkv
mkfifo /home/dave/source/restream/pipes/channel68.mkv
mkfifo /home/dave/source/restream/pipes/channel69.mkv
mkfifo /home/dave/source/restream/pipes/channel70.mkv
mkfifo /home/dave/source/restream/pipes/channel71.mkv
mkfifo /home/dave/source/restream/pipes/channel72.mkv
mkfifo /home/dave/source/restream/pipes/channel73.mkv
mkfifo /home/dave/source/restream/pipes/channel74.mkv
mkfifo /home/dave/source/restream/pipes/channel75.mkv
mkfifo /home/dave/source/restream/pipes/channel76.mkv
mkfifo /home/dave/source/restream/pipes/channel77.mkv
mkfifo /home/dave/source/restream/pipes/channel78.mkv
mkfifo /home/dave/source/restream/pipes/channel79.mkv


Setup TVHeadend

Create new network, IP

Add a mux to the IP network with the following sample for channel 67:
pipe:///bin/bash /home/dave/source/restream/execpipe.sh 67

In the execpipe script put the folllowing
#!/bin/bash

PIPE="/home/dave/source/restream/pipes/channel"$1".mkv"

function SendToPipe
{
#/usr/bin/ffmpeg -loglevel fatal -follow 1 -fflags +genpts -analyzeduration 64 -i $PIPE -c:v libx264 -ar 48000 -ac 2 -c:a libfdk_aac -f mpegts pipe:1
/usr/bin/ffmpeg -loglevel fatal -i $PIPE -c:v libx264 -ar 48000 -ac 2 -c:a libfdk_aac -f mpegts pipe:1

}


while true
do
  SendToPipe
  sleep 1
done

exit 0

Make sure the scaling in this matches any scaling that may occur in the stream.
Scaling, video format, audio format conversions make the transitions between
movies occur "better"

Next, specify the xmltv entries to each channel.  
(Restream may need to be started/stopped so that it will create some xmltv entries in TVHeadend.
sudo chmod 755 /home/hts/.hts/tvheadend/epggrab


When debugging restream program using gdb, specify:

handle SIGPIPE nostop noprint pass

Start program as:

restream configurationfile

or if no configuration file is specified, it will open the file "testall.txt" in current directory.
 