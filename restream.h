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

#ifndef _INCLUDE_RESTREAM_H_
    #define _INCLUDE_RESTREAM_H_

    #define _GNU_SOURCE

    #include <pthread.h>
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

    #define SLEEP(seconds, nanoseconds) {              \
                    struct timespec tv;                \
                    tv.tv_sec = (seconds);             \
                    tv.tv_nsec = (nanoseconds);        \
                    while (nanosleep(&tv, &tv) == -1); \
            }

    /* values of boolean */
    #ifndef FALSE
        #define FALSE   0
    #endif
    #ifndef TRUE
        #define TRUE    1
    #endif

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

    typedef struct StreamContext {
        AVCodecContext      *dec_ctx;
        AVCodecContext      *enc_ctx;
    } StreamContext;

    struct ts_item {
        int64_t audio;
        int64_t video;
    };

    struct guide_item {
        char   *movie1_filename;
        char   *movie2_filename;

        char   *movie1_displayname;
        char   *movie2_displayname;
        char   *guide_filename;
        char   *guide_displayname;

        char   time1_st[25];
        char   time1_en[25];

        char   time2_st[25];
        char   time2_en[25];

    };

    typedef struct ctx_restream {
        char                    *in_filename;
        char                    *out_filename;
        AVFormatContext         *ifmt_ctx;
        AVFormatContext         *ofmt_ctx;
        const AVOutputFormat    *ofmt;
        AVPacket                *pkt;

        int                     stream_index;
        int                     audio_index;
        int                     video_index;
        StreamContext           *stream_ctx;
        int                     stream_count;

        int64_t                 time_start;
        struct ts_item          pts_strtin;
        struct ts_item          pts_lstin;

        struct ts_item          ts_out;    /* Last ts written to output file*/
        struct ts_item          ts_base;   /* Starting ts for the output file*/
        struct ts_item          ts_file;

        //struct ts_item          ts_lstout;

        unsigned int            rand_seed;

        struct playlist_item    *playlist;
        char                    *playlist_dir;
        char                    *function_name;
        volatile int            finish_thread;
        int                     playlist_count;
        int                     playlist_index;
        char                    *playlist_sort_method;   //a =alpha, r = random

        struct guide_item       *guide_info;

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


    } ctx_restream;
    /**********************************************************/

    struct channel_context {
        struct channel_item   *channel_info;
        int                    channel_count;
    };
    extern volatile int finish;
    int restrm_interrupt(void *ctx);

#endif

