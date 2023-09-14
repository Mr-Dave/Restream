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
#include "guide.hpp"
#include "playlist.hpp"
#include "infile.hpp"
#include "reader.hpp"
#include "writer.hpp"

void guide_names_movie(ctx_restream *restrm){

    int indx_list;
    int    index;
    int    index_slash = 0;
    int    index_dot = 0;
    char   slash = '/';
    char   dot = '.';
    char   tmp_str[4096];

    snprintf(restrm->function_name,1024,"%s","guide_names_movie");
    if (finish == TRUE) {
        return;
    }

    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    /* Names for first movie */
    indx_list = restrm->playlist_index;

    snprintf(restrm->guide_info->movie1_filename,4096,"%s"
        ,restrm->playlist[indx_list].movie_path);
    snprintf(restrm->guide_info->movie1_displayname,4096,"%s"
        ,restrm->guide_info->movie1_filename);

    index = 0;
    index_slash = -1;
    index_dot = strlen(restrm->guide_info->movie1_displayname);
    while (index < strlen(restrm->guide_info->movie1_displayname)){
      if (restrm->guide_info->movie1_displayname[index] == slash) index_slash = index;
      if (restrm->guide_info->movie1_displayname[index] == dot) index_dot = index;
      index ++;
    }
    snprintf(tmp_str,4096,"%s",&restrm->guide_info->movie1_displayname[index_slash + 1]);
    snprintf(restrm->guide_info->movie1_displayname,4096,"%s",tmp_str);
    restrm->guide_info->movie1_displayname[index_dot - index_slash -1 ] = '\0';

    /* Names for second movie */
    indx_list++;
    if (indx_list >= restrm->playlist_count) indx_list = 0;

    snprintf(restrm->guide_info->movie2_filename,4096,"%s",restrm->playlist[indx_list].movie_path);
    snprintf(restrm->guide_info->movie2_displayname,4096,"%s",restrm->guide_info->movie2_filename);

    index = 0;
    index_slash = -1;
    index_dot = strlen(restrm->guide_info->movie2_displayname);
    while (index < strlen(restrm->guide_info->movie2_displayname)){
      if (restrm->guide_info->movie2_displayname[index] == slash) index_slash = index;
      if (restrm->guide_info->movie2_displayname[index] == dot) index_dot = index;
      index ++;
    }
    snprintf(tmp_str,4096,"%s",&restrm->guide_info->movie2_displayname[index_slash + 1]);
    snprintf(restrm->guide_info->movie2_displayname,4096,"%s",tmp_str);
    restrm->guide_info->movie2_displayname[index_dot - index_slash -1 ] = '\0';


}

void guide_times(ctx_restream *restrm){

    AVFormatContext *guidefmt_ctx;
    int64_t         dur_time;
    int64_t         ctx_duration;
    int             retcd;
    time_t          timenow;
    struct tm       *time_info;

    snprintf(restrm->function_name,1024,"%s","guide_times");
    if (finish == TRUE) {
        return;
    }

    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    dur_time = 0;
    guidefmt_ctx = NULL;
    if ((retcd = avformat_open_input(&guidefmt_ctx, restrm->guide_info->movie1_filename, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'\n", restrm->guide_info->movie1_filename);
        return;
    }
    ctx_duration =guidefmt_ctx->duration;
    dur_time = av_rescale(ctx_duration, 1 , AV_TIME_BASE);
    avformat_close_input(&guidefmt_ctx);
    guidefmt_ctx = NULL;

    time(&timenow);
    time_info = localtime(&timenow);
    strftime(restrm->guide_info->time1_st, sizeof(restrm->guide_info->time1_st), "%Y%m%d%H%M%S %z", time_info);

    timenow = timenow + (int16_t)(dur_time);
    time_info = localtime(&timenow);
    strftime(restrm->guide_info->time1_en, sizeof(restrm->guide_info->time1_en), "%Y%m%d%H%M%S %z", time_info);
    //fprintf(stderr,"duration1 %ld %ld %s %s\n",  ctx_duration,dur_time,restrm->guide_info->time1_st,restrm->guide_info->time1_en);

    /*  Get times for second movie */
    dur_time = 0;
    guidefmt_ctx = NULL;
    if ((retcd = avformat_open_input(&guidefmt_ctx, restrm->guide_info->movie2_filename, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'\n", restrm->guide_info->movie2_filename);
        return;
    }
    ctx_duration =guidefmt_ctx->duration;
    dur_time = av_rescale(ctx_duration, 1 , AV_TIME_BASE);
    avformat_close_input(&guidefmt_ctx);
    guidefmt_ctx = NULL;

    /* Recall that we set this to the end time of the first movie above */
    timenow = timenow ;
    time_info = localtime(&timenow);
    strftime(restrm->guide_info->time2_st, sizeof(restrm->guide_info->time2_st), "%Y%m%d%H%M%S %z", time_info);

    timenow = timenow + (int16_t)(dur_time);
    time_info = localtime(&timenow);
    strftime(restrm->guide_info->time2_en, sizeof(restrm->guide_info->time2_en), "%Y%m%d%H%M%S %z", time_info);
    //fprintf(stderr,"duration2 %ld %ld %s %s\n",  ctx_duration,dur_time,restrm->guide_info->time2_st,restrm->guide_info->time2_en);

}

void guide_write(ctx_restream *restrm){

    struct  sockaddr_un addr;
    char    buf[4096];
    ssize_t retcnt;
    int fd,rc;

    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    snprintf(restrm->function_name,1024,"%s","guide_write");
    if (finish == TRUE) {
        return;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if ( fd == -1) {
        fprintf(stderr,"Error creating socket for guide\n");
        return;
    }

    memset(&addr,'\0', sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path,108,"%s","/home/hts/.hts/tvheadend/epggrab/xmltv.sock");
    rc = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (rc == -1) {
        fprintf(stderr,"Error connecting socket for guide\n");
        close(fd);
        return;
    }

    memset(&buf,'\0',4096);
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
        ,restrm->guide_info->guide_displayname
        ,restrm->guide_info->guide_displayname
        ,restrm->guide_info->time1_st
        ,restrm->guide_info->time1_en
        ,restrm->guide_info->guide_displayname
        ,restrm->guide_info->movie1_displayname
        ,restrm->guide_info->guide_displayname
        ,restrm->guide_info->guide_displayname
        ,restrm->guide_info->time2_st
        ,restrm->guide_info->time2_en
        ,restrm->guide_info->guide_displayname
        ,restrm->guide_info->movie2_displayname
    );

    rc = strlen(buf);
    retcnt = write(fd, buf, rc);
    if (retcnt != rc){
        fprintf(stderr,"Error writing socket tried %d wrote %ld\n", rc, retcnt);
        close(fd);
        return;
    }
    close(fd);

}

void guide_free(ctx_restream *restrm){

    snprintf(restrm->function_name,1024,"%s","guide_free");

    /* Remove any previous allocations for the names */
    free(restrm->guide_info->movie1_filename);
    free(restrm->guide_info->movie2_filename);
    free(restrm->guide_info->movie1_displayname);
    free(restrm->guide_info->movie2_displayname);
    free(restrm->guide_info->guide_filename);
    free(restrm->guide_info->guide_displayname);
}

void guide_init(ctx_restream *restrm){

    int buff_max = 4096;
    snprintf(restrm->function_name,1024,"%s","guide_init");
    if (finish == TRUE) {
        return;
    }

    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    restrm->guide_info =(guide_item*)malloc(sizeof(struct guide_item));
    memset(restrm->guide_info,'\0',sizeof(struct guide_item));

    restrm->guide_info->movie1_filename  =(char*) malloc(buff_max);
    memset(restrm->guide_info->movie1_filename,'\0',buff_max);

    restrm->guide_info->movie1_displayname  = (char*) malloc(buff_max);
    memset(restrm->guide_info->movie1_displayname,'\0',buff_max);

    restrm->guide_info->movie2_filename  =(char*) malloc(buff_max);
    memset(restrm->guide_info->movie2_filename,'\0',buff_max);

    restrm->guide_info->movie2_displayname  = (char*)malloc(buff_max);
    memset(restrm->guide_info->movie2_displayname,'\0',buff_max);

    restrm->guide_info->guide_filename  =(char*) malloc(buff_max);
    memset(restrm->guide_info->guide_filename,'\0',buff_max);

    restrm->guide_info->guide_displayname  =(char*) malloc(buff_max);
    memset(restrm->guide_info->guide_displayname,'\0',buff_max);

}

void guide_names_guide(ctx_restream *restrm){

    int    indx, indx_en;
    int    index_slash = 0;
    int    index_slash2 = 0;
    int    index_dot = 0;
    char   slash = '/';
    char   dot = '.';

    snprintf(restrm->function_name,1024,"%s","guide_names_guide");
    if (finish == TRUE) {
        return;
    }

    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    index_slash = -1;
    index_dot = strlen(restrm->out_filename);
    indx_en =strlen(restrm->out_filename);
    for (indx=0; indx<=indx_en; indx++){
      if (restrm->out_filename[indx] == slash) index_slash = indx;
      if (restrm->out_filename[indx] == dot) index_dot = indx;
    }
    snprintf(restrm->guide_info->guide_displayname,index_dot - index_slash
        ,"%s", &restrm->out_filename[index_slash + 1]);


    /* Now we need the directory one level up */
    index_slash2 = -1;
    for (indx=0; indx<index_slash;indx++){
      if (restrm->out_filename[indx] == slash) index_slash2 = indx;
    }
    snprintf(restrm->guide_info->guide_filename,4096,"%.*s/guide/%s.xml"
        ,index_slash2, restrm->out_filename
        ,restrm->guide_info->guide_displayname);

    //fprintf(stderr,"%s:  Guide file  >%s<\n",restrm->guide_info->guide_displayname,restrm->guide_info->guide_filename);

}

void guide_process(ctx_restream *restrm){

    struct stat sdir;
    int retcd;

    /* Determine if we are on the test machine */
    retcd = stat("/home/hts/.hts/tvheadend", &sdir);
    if(retcd < 0) {
        return;
    }

    snprintf(restrm->function_name,1024,"%s","guide_process");
    if (finish == TRUE) {
        return;
    }

    guide_names_movie(restrm);

    guide_times(restrm);

    guide_write(restrm);

}
