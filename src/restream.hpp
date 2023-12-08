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
        #include <libswscale/swscale.h>
    }
    #include <microhttpd.h>
    #include <stdio.h>
    #include <stdlib.h>
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

    struct ctx_config;
    struct ctx_app;

    extern bool finish;

    #define MYFFVER (LIBAVFORMAT_VERSION_MAJOR * 1000)+LIBAVFORMAT_VERSION_MINOR

    #define SLEEP(seconds, nanoseconds) {              \
                struct timespec ts1;                \
                ts1.tv_sec = (seconds);             \
                ts1.tv_nsec = (nanoseconds);        \
                while (nanosleep(&ts1, &ts1) == -1); \
        }

    enum PIPE_STATUS{
        PIPE_IS_OPEN,
        PIPE_IS_CLOSED,
        PIPE_NEEDS_RESET
    };
    enum READER_STATUS{
        READER_STATUS_CLOSED,
        READER_STATUS_PAUSED,
        READER_STATUS_OPEN,
        READER_STATUS_INACTIVE,
        READER_STATUS_READING,
        READER_STATUS_READBYTE
    };
    enum READER_ACTION{
        READER_ACTION_START,
        READER_ACTION_OPEN,
        READER_ACTION_CLOSE,
        READER_ACTION_END,
        READER_ACTION_BYTE
    };

    enum WEBUI_LEVEL{
        WEBUI_LEVEL_ALWAYS     = 0,
        WEBUI_LEVEL_LIMITED    = 1,
        WEBUI_LEVEL_ADVANCED   = 2,
        WEBUI_LEVEL_RESTRICTED = 3,
        WEBUI_LEVEL_NEVER      = 99
    };

    struct ctx_codec {
        AVCodecContext      *dec_ctx;
        AVCodecContext      *enc_ctx;
    };

    struct ts_item {
        int64_t audio;
        int64_t video;
    };

    struct ctx_params_item {
        std::string     param_name;       /* The name or description of the ID as requested by user*/
        std::string     param_value;      /* The value that the user wants the control set to*/
    };

    struct ctx_params {
        std::list<ctx_params_item> params_array;  /*List of the controls the user specified*/
        int params_count;               /*Count of the controls the user specified*/
        bool update_params;             /*Bool for whether to update the parameters on the device*/
        std::string params_desc;        /* Description of params*/
    };

    struct ctx_webu_clients {
        std::string                 clientip;
        bool                        authenticated;
        int                         conn_nbr;
        struct timespec             conn_time;
        int                         userid_fail_nbr;
    };

    struct ctx_restream {
        std::string             in_filename;
        std::string             out_filename;
        AVFormatContext         *ofmt_ctx;
        const AVOutputFormat    *ofmt;

        unsigned int            rand_seed;
        std::string             playlist_dir;
        std::string             function_name;
        volatile bool           finish_thread;
        int                     playlist_count;
        int                     playlist_index;
        std::string             playlist_sort_method;   //a =alpha, r = random

        volatile enum PIPE_STATUS       pipe_state;
        volatile enum READER_STATUS     reader_status;
        volatile enum READER_ACTION     reader_action;

        int64_t                watchdog_reader;
        int64_t                watchdog_playlist;
        int64_t                connect_start;
        int                    soft_restart;
        pthread_t              reader_thread;
        pthread_t              process_playlist_thread;
        pthread_mutex_t        mutex_reader;   /* mutex used with the output reader */

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

    struct ctx_stream_info {
        int index_dec;
        int index_enc;
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
        ctx_app         *app;
        ctx_params      ch_params;
        std::string     ch_conf;
        std::string     ch_dir;
        std::string     ch_nbr;
        std::string     ch_sort;

        int     ch_index;
        bool    ch_status;
        bool    ch_finish;
        bool    ch_tvhguide;

        std::vector<ctx_playlist_item>    playlist;
        int     playlist_count;
        int     playlist_index;

        ctx_file_info   ifile;
        ctx_file_info   ofile;
        int64_t         file_cnt;
        AVPacket        *pkt;
        AVFrame         *frame;

        int cnct_cnt;

        std::vector<ctx_packet_item> pktarray;
        int         pktarray_count;
        int         pktarray_index;
        int64_t     pktnbr;
        int         pktarray_lastwritten;

        pthread_mutex_t    mtx_pktarray;
        pthread_mutex_t    mtx_ifmt;

    };

    struct ctx_app {
        int         argc;
        char        **argv;
        std::string conf_file;
        ctx_config  *conf;

        std::vector<ctx_channel_item>    channels;
        int         ch_count;

        int         webcontrol_running;
        int         webcontrol_finish;
        ctx_params  webcontrol_headers;        /* parameters for header */
        char        webcontrol_digest_rand[12];
        struct MHD_Daemon               *webcontrol_daemon;
        std::list<ctx_webu_clients>      webcontrol_clients;         /* list of client ips */
    };

#endif

