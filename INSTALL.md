

Setup Configuration 
In a text file add entries such as this:
"/mnt/FDrive/Series/Nature/" "/home/dave/restream/pipes/channel70.ts" "r"

Where
"/mnt/FDrive/Series/Nature/" is the directory of recordings
"/home/dave/restream/pipes/channel70.ts" is the named pipe
"r" indicates randomize the entries, "a" would indicate alpha sorted

Add as many rows as desired, each will be a channel. 


Setup named pipes:

mkfifo /home/dave/restream/pipes/channel70.ts


Setup TVHeadend

Create new network, IP

Add a mux to the IP network with the following:
pipe:///usr/bin/ffmpeg -loglevel fatal -follow 1 -i file:/home/dave/restream/pipes/channel70.ts  -c:v libx264 -ar 48000 -ac 2 -c:a libfdk_aac -vf scale=320:-1 -f mpegts -tune zerolatency pipe:1

Make sure the scaling in this matches any scaling that may occur in the stream.
Scaling, video format, audio format conversions make the transitions between
movies occur "better"

Next, specify the xmltv entries to each channel.  
(Restream may need to be started/stopped so that it will create some xmltv entries in TVHeadend.


When debugging restream program using gdb, specify:

handle SIGPIPE nostop noprint pass

Start program as:

restream configurationfile

or if no configuration file is specified, it will open the file "testall.txt" in current directory.
 