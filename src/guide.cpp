/*
 *    This file is part of Restream.
 *
 *    Restream is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Restream is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Restream.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "restream.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "guide.hpp"
#include "infile.hpp"
#include "webu.hpp"

void guide_times(
    std::string f1, std::string &st1,std::string &en1,
    std::string f2, std::string &st2,std::string &en2)
{
    AVFormatContext *guidefmt_ctx;
    int64_t         dur_time;
    int             retcd;
    time_t          timenow;
    struct tm       *time_info;
    char            timebuf[1024];

    if (finish == true) {
        return;
    }

    dur_time = 0;

    time(&timenow);
    time_info = localtime(&timenow);
    strftime(timebuf, sizeof(timebuf), "%Y%m%d%H%M%S %z", time_info);
    st1 = timebuf;

    guidefmt_ctx = nullptr;
    retcd = avformat_open_input(&guidefmt_ctx,f1.c_str(), 0, 0);
    if (retcd < 0) {
        en1 = st1;
        LOG_MSG(NTC, NO_ERRNO, "Could not open file %s",f1.c_str());
        return;
    }
    dur_time = av_rescale(guidefmt_ctx->duration, 1 , AV_TIME_BASE);
    avformat_close_input(&guidefmt_ctx);
    guidefmt_ctx = nullptr;

    timenow = timenow + (int16_t)(dur_time);
    time_info = localtime(&timenow);
    strftime(timebuf, sizeof(timebuf), "%Y%m%d%H%M%S %z", time_info);
    en1 = timebuf;

    timenow++;
    time_info = localtime(&timenow);
    strftime(timebuf, sizeof(timebuf), "%Y%m%d%H%M%S %z", time_info);
    st2 = timebuf;

    retcd = avformat_open_input(&guidefmt_ctx, f2.c_str(), 0, 0);
    if (retcd < 0) {
        en2 = st2;
        LOG_MSG(NTC, NO_ERRNO, "Could not open file %s",f2.c_str());
        return;
    }
    dur_time = av_rescale(guidefmt_ctx->duration, 1 , AV_TIME_BASE);
    avformat_close_input(&guidefmt_ctx);
    guidefmt_ctx = nullptr;

    timenow = timenow + (int16_t)(dur_time);
    time_info = localtime(&timenow);
    strftime(timebuf, sizeof(timebuf), "%Y%m%d%H%M%S %z", time_info);
    en2 = timebuf;

}

void guide_write_xml(ctx_channel_item *chitm, std::string &xml)
{
    std::string st1,en1,fl1,dn1;
    std::string st2,en2,fl2,dn2;
    std::string gnm;
    char    buf[4096];

    fl1 = chitm->playlist[chitm->playlist_index].fullnm;
    dn1 = chitm->playlist[chitm->playlist_index].displaynm;

    if ((chitm->playlist_index+1) > chitm->playlist_count) {
        fl2 = chitm->playlist[0].fullnm;
        dn2 = chitm->playlist[0].displaynm;
    } else {
        fl2 = chitm->playlist[chitm->playlist_index].fullnm;
        dn2 = chitm->playlist[chitm->playlist_index].displaynm;
    }
    guide_times(fl1, st1, en1, fl2, st2, en2);

    gnm = "channel"+chitm->ch_nbr;

    snprintf(buf, 4096,
        "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
        "<!DOCTYPE tv SYSTEM \"xmlv.dtd\">\n"
        "<tv>\n"
        "  <channel id=\"%s\">\n"
        "    <display-name>%s</display-name>\n"
        "  </channel>\n"
        "  <programme start=\"%s \" stop=\"%s \" channel=\"%s\">\n"
        "    <title lang=\"en\">%s</title>\n"
        "  </programme>\n"
        "  <channel id=\"%s\">\n"
        "    <display-name>%s</display-name>\n"
        "  </channel>\n"
        "  <programme start=\"%s \" stop=\"%s \" channel=\"%s\">\n"
        "    <title lang=\"en\">%s</title>\n"
        "  </programme>\n"
        "</tv>\n"

        ,gnm.c_str(),gnm.c_str()
        ,st1.c_str(),en1.c_str(),gnm.c_str(),dn1.c_str()

        ,gnm.c_str(),gnm.c_str()
        ,st2.c_str(),en2.c_str(),gnm.c_str(),dn2.c_str()
    );

    xml = buf;
}

void guide_write(ctx_channel_item *chitm)
{
    struct  sockaddr_un addr;
    size_t retcd;
    int fd, rc;
    std::string xml;

    if (finish == true) {
        return;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if ( fd == -1) {
        LOG_MSG(NTC, NO_ERRNO, "Error creating socket for the guide");
        return;
    }

    memset(&addr,'\0', sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path,108,"%s","/home/hts/.hts/tvheadend/epggrab/xmltv.sock");
    rc = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (rc == -1) {
        LOG_MSG(NTC, NO_ERRNO, "Error connecting socket for the guide");
        close(fd);
        return;
    }

    guide_write_xml(chitm, xml);

    retcd = (size_t)write(fd, xml.c_str(), xml.length());
    if (retcd != xml.length()) {
        LOG_MSG(NTC, NO_ERRNO
            , "Error writing socket tried %d wrote %ld"
            , xml.length(), retcd);
    }
    close(fd);

}

void guide_process(ctx_channel_item *chitm)
{
    struct stat sdir;
    int retcd;
    std::string xml;

    if (finish == true) {
        return;
    }

    /* Determine if we are on the test machine */
    retcd = stat("/home/hts/.hts/tvheadend", &sdir);
    if(retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Requested tvh guide but the required directory does not exist.");
        LOG_MSG(NTC, NO_ERRNO
            , "Printing tvh guide xml to log.");
        guide_write_xml(chitm, xml);
        LOG_MSG(NTC, NO_ERRNO, "%s",xml.c_str());
        return;
    }

    guide_write(chitm);

}
