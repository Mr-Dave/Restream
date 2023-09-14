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


#include "restream.hpp"
#include "guide.hpp"
#include "playlist.hpp"
#include "infile.hpp"
#include "reader.hpp"
#include "writer.hpp"


int infile_init(ctx_restream *restrm){

    int retcd, stream_index, indx;
    char errstr[128];
    int64_t incr_st;

    //if (finish == TRUE) return -1;
    snprintf(restrm->function_name,1024,"%s","infile_init");
    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    guide_process(restrm);

    snprintf(restrm->function_name,1024,"%s","infile_init 01");
    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    restrm->ifmt_ctx=NULL;

    retcd = avformat_open_input(&restrm->ifmt_ctx, restrm->in_filename, NULL, NULL);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        fprintf(stderr, "%s: Could not open input file '%s': %s  \n"
            ,restrm->guide_info->guide_displayname, restrm->in_filename, errstr);
        return -1;
    }

    snprintf(restrm->function_name,1024,"%s","infile_init 02");
    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    retcd = avformat_find_stream_info(restrm->ifmt_ctx, NULL);
    if (retcd < 0) {
        fprintf(stderr, "%s: Failed to retrieve input stream information\n"
            ,restrm->guide_info->guide_displayname);
        return -1;
    }

    restrm->stream_index = 0;
    restrm->stream_ctx = NULL;
    restrm->stream_count = 0;
    restrm->stream_count = restrm->ifmt_ctx->nb_streams;
    restrm->stream_ctx =(StreamContext*)malloc(restrm->stream_count * sizeof(StreamContext));
    if (!restrm->stream_ctx) {
        fprintf(stderr, "%s:  Failed to allocate space for streams\n"
            ,restrm->guide_info->guide_displayname);
        return -1;
    }

    snprintf(restrm->function_name,1024,"%s","infile_init 03");
    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    stream_index = 0;
    while (stream_index < restrm->ifmt_ctx->nb_streams){
        restrm->stream_ctx[stream_index].dec_ctx = NULL;
        restrm->stream_ctx[stream_index].enc_ctx = NULL;
        stream_index++;
    }

    stream_index = 0;
    while (stream_index < restrm->ifmt_ctx->nb_streams){
        AVStream *stream = restrm->ifmt_ctx->streams[stream_index];
        const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;

        if (!dec) {
            fprintf(stderr, "%s: Failed to find decoder for stream #%d\n"
                ,restrm->guide_info->guide_displayname, stream_index);
            return -1;
        }

        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            fprintf(stderr, "%s: Failed to allocate decoder for stream #%d\n"
                ,restrm->guide_info->guide_displayname, stream_index);
            return -1;
        }

        retcd = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (retcd < 0) {
            fprintf(stderr, "%s: Failed to copy decoder parameters for stream #%d\n"
                ,restrm->guide_info->guide_displayname, stream_index);
            return -1;
        }

        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
            codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {

            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
                codec_ctx->framerate = av_guess_frame_rate(restrm->ifmt_ctx, stream, NULL);
                restrm->video_index = stream_index;
            }
            if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO){
                restrm->audio_index = stream_index;
            }
            retcd = avcodec_open2(codec_ctx, dec, NULL);
            if (retcd < 0) {
                fprintf(stderr, "%s: Failed to open decoder for stream #%d\n"
                    ,restrm->guide_info->guide_displayname, stream_index);
                return -1;
            }

            restrm->stream_ctx[stream_index].dec_ctx = codec_ctx;
        }

        stream_index++;
    }

    /* Read some pkts to get correct start times */
    indx = 0;
    restrm->pts_strtin.video = 0;
    restrm->pts_strtin.audio = 0;
    restrm->ts_file.video = 0;
    restrm->ts_file.audio = 0;
    while (indx < 100) {
        av_packet_free(&restrm->pkt);
        restrm->pkt = av_packet_alloc();
        restrm->pkt->data = NULL;
        restrm->pkt->size = 0;

        snprintf(restrm->function_name,1024,"%s","infile_init 04");
        restrm->watchdog_playlist = av_gettime_relative() + 5000000;

        retcd = av_read_frame(restrm->ifmt_ctx, restrm->pkt);
        if (retcd < 0){
            fprintf(stderr, "%s: Failed to read first packets %d\n"
                ,restrm->guide_info->guide_displayname, stream_index);
            return -1;
        }

        if (restrm->pkt->pts != AV_NOPTS_VALUE) {
            if (restrm->pkt->stream_index == restrm->video_index) {
                restrm->pts_strtin.video = av_rescale(restrm->pkt->pts, 1000000
                    ,restrm->ifmt_ctx->streams[restrm->pkt->stream_index]->time_base.den);
                if (restrm->ts_file.video < restrm->pkt->pts) {
                    restrm->ts_file.video = restrm->pkt->pts;
                }
            }
            if (restrm->pkt->stream_index == restrm->audio_index) {
                restrm->pts_strtin.audio = av_rescale(restrm->pkt->pts, 1000000
                    ,restrm->ifmt_ctx->streams[restrm->pkt->stream_index]->time_base.den);
                if (restrm->ts_file.audio < restrm->pkt->pts) {
                    restrm->ts_file.audio = restrm->pkt->pts;
                }
            }
        }

        if ((restrm->pts_strtin.video > 0) &&
            (restrm->pts_strtin.audio > 0)) {
            break;
        }

        indx++;
    }

    restrm->time_start = av_gettime_relative();

    if (restrm->ts_out.audio > restrm->ts_out.video) {
        restrm->ts_base.audio = restrm->ts_out.audio;
        restrm->ts_base.video = restrm->ts_out.audio;
    } else {
        restrm->ts_base.audio = restrm->ts_out.video;
        restrm->ts_base.video = restrm->ts_out.video;
    }
    restrm->pts_lstin.video = restrm->pts_strtin.video;
    restrm->pts_lstin.audio = restrm->pts_strtin.audio;

    snprintf(restrm->function_name, 1024,"%s","infile_init 05");
    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    /*
    fprintf(stderr, "%s: ts_base %ld %ld ts_file %ld %ld \n"
            ,restrm->guide_info->guide_displayname
            ,restrm->ts_base.audio,restrm->ts_base.video
            ,restrm->ts_file.audio,restrm->ts_file.video
            );

    */

    return 0;

}

void infile_close(ctx_restream *restrm){
    int indx;

    snprintf(restrm->function_name,1024,"%s","infile_close");
    restrm->watchdog_playlist = av_gettime_relative();

    if (restrm->stream_ctx != NULL) {
        indx = 0;
        while (indx < restrm->stream_count){
            if (restrm->stream_ctx[indx].dec_ctx !=  NULL){
                avcodec_free_context(&restrm->stream_ctx[indx].dec_ctx);
                restrm->stream_ctx[indx].dec_ctx =  NULL;
            }
            if (restrm->stream_ctx[indx].enc_ctx !=  NULL){
                avcodec_free_context(&restrm->stream_ctx[indx].enc_ctx);
                restrm->stream_ctx[indx].enc_ctx =  NULL;
            }
            indx++;
        }
        free(restrm->stream_ctx);
        restrm->stream_ctx = NULL;
    }

    if (restrm->ifmt_ctx != NULL) {
        avformat_close_input(&restrm->ifmt_ctx);
        restrm->ifmt_ctx = NULL;
    }
}

void infile_wait(ctx_restream *restrm){

    int64_t tm_diff, pts_diff, tot_diff, pts;

    if (finish == TRUE) return;
    snprintf(restrm->function_name,1024,"%s","infile_wait");

    restrm->soft_restart = 1;

    if (restrm->pkt->stream_index != restrm->video_index) return;

    if (restrm->pkt->pts != AV_NOPTS_VALUE) {
        pts = av_rescale(restrm->pkt->pts, 1000000
            ,restrm->ifmt_ctx->streams[restrm->pkt->stream_index]->time_base.den);
        if (pts < restrm->pts_lstin.video) return;
        restrm->pts_lstin.video = pts;
    }

    restrm->watchdog_playlist = av_gettime_relative();

    /* How much time has really elapsed since the start of movie*/
    tm_diff = av_gettime_relative() - restrm->time_start;

    /* How much time the pts wants us to be at since the start of the movie */
    pts_diff = pts - restrm->pts_strtin.video;

    /* How much time we need to wait to get in sync*/
    tot_diff = pts_diff - tm_diff;

        /*
        fprintf(stderr
            ,"%s: Return "
            " dts_lstin.video: %ld pkt->dts:%ld"
            " dts_strtin.video: %ld totdiff:%ld \n"
            , restrm->guide_info->guide_displayname
            , restrm->dts_lstin.video, restrm->pkt->dts
            , restrm->dts_strtin.video, tot_diff
            );
        */

    if (tot_diff > 0){
        if (tot_diff < 1000000){
            snprintf(restrm->function_name,1024,"%s %ld","infile_wait",tot_diff);
            SLEEP(0, tot_diff * 1000);
        } else {
            /*
            if (restrm->pipe_state == PIPE_NEEDS_RESET ){
                fprintf(stderr
                    ,"%s: Excessive wait time "
                    " dts_in_last: %ld pkt->dts:%ld"
                    " dts_strtin.video: %ld dts_base:%ld"
                    " tm_diff: %ld dts_diff: %ld tot_diff: %ld\n"
                    , restrm->guide_info->guide_displayname
                    , restrm->dts_in_last, restrm->pkt->dts
                    , restrm->dts_strtin.video, restrm->dts_base
                    , tm_diff, dts_diff, tot_diff
                    );
            }
            */
            /* reset all our times to see if we can get in sync*/
            restrm->time_start = av_gettime_relative();
            restrm->pts_strtin.video = av_rescale(restrm->pkt->pts, 1000000
                ,restrm->ifmt_ctx->streams[restrm->pkt->stream_index]->time_base.den);
            restrm->pts_lstin.video = restrm->pts_strtin.video;
        }
    }
}
