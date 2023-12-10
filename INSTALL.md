Build:

sudo apt install autoconf automake autopoint build-essential pkgconf libtool git libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libmicrohttpd-dev

autoreconf -fiv
./configure
make

In the ect/restream directory, edit/create a restream.conf file with entries similar to the following:

;*************************************************
;*****   Web Control
;*************************************************
log_level 8
webcontrol_port 8080
webcontrol_localhost off
;*************************************************
;*****   Channel Setup
;*************************************************
channel ch=87,dir=/home/dave/test/PBS/, sort=random,tvhguide=on
channel ch=187,dir=/home/dave/test/Nova/, sort=alpha,tvhguide=off
channel ch=8,dir=/home/dave/test/Nature/, sort=alpha,tvhguide=off
channel ch=7,dir=/home/dave/test/Misc/, sort=random,tvhguide=on
;*************************************************
;*************************************************


Setup TVHeadend

Create new network, IP

Add a mux to the IP network with the following sample for channel 7:
http://localhost:8080/7/mpegts


Next, specify the xmltv entries to each channel.
(Restream may need to be started/stopped so that it will create some xmltv entries in TVHeadend.

sudo chmod 755 /home/hts/.hts/tvheadend/epggrab


