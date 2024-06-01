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

#ifndef _INCLUDE_RESTREAM_HPP_
#define _INCLUDE_RESTREAM_HPP_
    #include "config.hpp"
    #include <pthread.h>
    extern "C" {
        #include <libavcodec/avcodec.h>
        #include <libavformat/avformat.h>
        #include <libavfilter/buffersink.h>
        #include <libavfilter/buffersrc.h>
        #include <libavutil/opt.h>
        #include <libavutil/imgutils.h>
        #include <libavutil/pixdesc.h>
        #include <libavutil/timestamp.h>
        #include <libavutil/time.h>
        #include <libavutil/mem.h>
        #include "libavutil/audio_fifo.h"
        #include <libswscale/swscale.h>
    }
    #include <microhttpd.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdarg.h>
    #include <syslog.h>
    #include <dirent.h>
    #include <errno.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <signal.h>
    #include <sys/time.h>
    #include <time.h>
    #include <string>
    #include <list>
    #include <vector>
    #include <iostream>
    #include <fstream>
    #include <thread>
    #include <algorithm>
    #include <mutex>

    class cls_config;
    class cls_log;
    class cls_app;

    extern cls_app *app;

    #define MYFFVER (LIBAVFORMAT_VERSION_MAJOR * 1000)+LIBAVFORMAT_VERSION_MINOR

    #define SLEEP(seconds, nanoseconds) {              \
                struct timespec ts1;                \
                ts1.tv_sec = (seconds);             \
                ts1.tv_nsec = (nanoseconds);        \
                while (nanosleep(&ts1, &ts1) == -1); \
        }

    enum WEBUI_LEVEL{
        WEBUI_LEVEL_ALWAYS     = 0,
        WEBUI_LEVEL_LIMITED    = 1,
        WEBUI_LEVEL_ADVANCED   = 2,
        WEBUI_LEVEL_RESTRICTED = 3,
        WEBUI_LEVEL_NEVER      = 99
    };
    enum PARM_CAT{
        PARM_CAT_00
        ,PARM_CAT_01
        ,PARM_CAT_02
        ,PARM_CAT_MAX
    };
    enum PARM_TYP{
        PARM_TYP_STRING
        , PARM_TYP_INT
        , PARM_TYP_LIST
        , PARM_TYP_BOOL
        , PARM_TYP_ARRAY
    };
    enum PARM_ACT{
        PARM_ACT_DFLT
        , PARM_ACT_SET
        , PARM_ACT_GET
        , PARM_ACT_LIST
    };
    struct ctx_config_item {
        std::string         parm_name;      /* name for this parameter                  */
        enum PARM_TYP       parm_type;      /* enum of parm_typ for bool,int or string. */
        enum PARM_CAT       parm_cat;       /* enum of parm_cat for grouping. */
        enum WEBUI_LEVEL    webui_level;    /* Enum to display in webui: 0,1,2,3,99(always to never)*/
    };

    struct ctx_params_item {
        std::string     param_name;       /* The name or description of the ID as requested by user*/
        std::string     param_value;      /* The value that the user wants the control set to*/
    };

    typedef std::list<ctx_params_item> p_lst;
    typedef p_lst::iterator p_it;

    struct ctx_params {
        p_lst params_array;         /*List of the controls the user specified*/
        int params_count;           /*Count of the controls the user specified*/
        bool update_params;         /*Bool for whether to update the parameters on the device*/
        std::string params_desc;    /* Description of params*/
    };

    struct ctx_webu_clients {
        std::string                 clientip;
        bool                        authenticated;
        int                         conn_nbr;
        struct timespec             conn_time;
        int                         userid_fail_nbr;
    };

    struct ctx_playlist_item {
        std::string   fullnm;
        std::string   filenm;
        std::string   displaynm;
    };

    struct ctx_packet_item{
        AVPacket    *packet;
        int64_t     idnbr;
        bool        iskey;
        bool        iswritten;
        AVRational  timebase;
        int64_t     start_pts;
        int64_t     file_cnt;
    };

    struct ctx_av_info {
        int             index;
        AVCodecContext  *codec_ctx;
        AVStream        *strm;
        int64_t         base_pdts;
        int64_t         start_pts;
        int64_t         last_pts;
    };

    struct ctx_file_info {
        AVFormatContext *fmt_ctx;
        ctx_av_info     audio;
        ctx_av_info     video;
        int64_t         time_start;
    };

    struct ctx_channel_item {
        cls_app         *app;
        ctx_params      ch_params;
        std::string     ch_conf;
        std::string     ch_dir;
        std::string     ch_nbr;
        std::string     ch_sort;
        std::string     ch_encode;

        int     ch_index;
        bool    ch_running;
        bool    ch_finish;
        bool    ch_tvhguide;

        std::vector<ctx_playlist_item>    playlist;
        int     playlist_count;
        int     playlist_index;

        ctx_file_info   ifile;
        ctx_file_info   ofile;
        int64_t         file_cnt;
        AVPacket        *pkt_in;
        AVFrame         *frame;
        AVAudioFifo     *fifo;
        int64_t         audio_last_pts;
        int64_t         audio_last_dts;

        int cnct_cnt;

        std::vector<ctx_packet_item> pktarray;
        int         pktarray_count;
        int         pktarray_index;
        int         pktarray_start;
        int64_t     pktnbr;

        pthread_mutex_t    mtx_pktarray;
        pthread_mutex_t    mtx_ifmt;

    };

    class cls_app {
        public:
            cls_app(int argc, char **argv);
            ~cls_app();

            int         argc;
            char        **argv;

            bool finish;

            cls_config  *conf;
            cls_log     *log;

            std::string     conf_file;

            std::vector<ctx_channel_item>    channels;
            int         ch_count;
            int         webcontrol_running;
            int         webcontrol_finish;
            ctx_params  webcontrol_headers;
            char        webcontrol_digest_rand[12];
            struct MHD_Daemon               *webcontrol_daemon;
            std::list<ctx_webu_clients>      webcontrol_clients;
        private:
            void signal_setup();

    };

#endif

