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


#include "restream.h"
#include "guide.h"
#include "playlist.h"
#include "infile.h"
#include "reader.h"
#include "writer.h"

int writer_init_video(ctx_restream *restrm, int indx) {

    const AVCodec *encoder;
    AVStream *out_stream;
    int retcd;

    //if (finish == TRUE) return -1;
    snprintf(restrm->function_name, 1024, "%s", "writer_init_video");
    restrm->watchdog_playlist = av_gettime_relative();

    out_stream = avformat_new_stream(restrm->ofmt_ctx, NULL);
    if (!out_stream) {
        fprintf(stderr, "%s: Failed allocating output video stream\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
        fprintf(stderr, "%s: Could not find video encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    restrm->stream_ctx[indx].enc_ctx = avcodec_alloc_context3(encoder);
    if (!restrm->stream_ctx[indx].enc_ctx) {
        fprintf(stderr, "%s: Could not allocate video encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }


    retcd = avcodec_parameters_from_context(out_stream->codecpar, restrm->stream_ctx[indx].dec_ctx);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not copy parms from video decoder\n"
            ,restrm->guide_info->guide_displayname);
        return -1;
    }

    retcd = avcodec_parameters_to_context(restrm->stream_ctx[indx].enc_ctx, out_stream->codecpar);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not copy parms to video encoder\n"
            ,restrm->guide_info->guide_displayname);
        return -1;
    }

    restrm->stream_ctx[indx].enc_ctx->width = restrm->stream_ctx[indx].dec_ctx->width;
    restrm->stream_ctx[indx].enc_ctx->height = restrm->stream_ctx[indx].dec_ctx->height;
    restrm->stream_ctx[indx].enc_ctx->time_base = restrm->stream_ctx[indx].dec_ctx->time_base;
    if (restrm->stream_ctx[indx].dec_ctx->pix_fmt == -1) {
        restrm->stream_ctx[indx].enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    } else {
        restrm->stream_ctx[indx].enc_ctx->pix_fmt = restrm->stream_ctx[indx].dec_ctx->pix_fmt;
    }

    retcd = avcodec_open2(restrm->stream_ctx[indx].enc_ctx, encoder, NULL);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not open video encoder\n", restrm->guide_info->guide_displayname);
        fprintf(stderr, "%s: %dx%d %d %d\n"
            , restrm->guide_info->guide_displayname
            , restrm->stream_ctx[indx].enc_ctx->width, restrm->stream_ctx[indx].enc_ctx->height
            , restrm->stream_ctx[indx].enc_ctx->pix_fmt
            , restrm->stream_ctx[indx].enc_ctx->time_base.den);
        return -1;
    }

    if (restrm->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        restrm->stream_ctx[indx].enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    out_stream->time_base = restrm->stream_ctx[indx].enc_ctx->time_base;

    retcd = avcodec_parameters_from_context(out_stream->codecpar, restrm->stream_ctx[indx].enc_ctx);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not copy parms from decoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    return 0;
}

int writer_init_audio(ctx_restream *restrm, int indx) {

    AVStream *out_stream;
    const AVCodec *encoder;
    int retcd;

    //if (finish == TRUE) return -1;
    snprintf(restrm->function_name, 1024, "%s", "writer_init_audio");
    restrm->watchdog_playlist = av_gettime_relative();

    out_stream = avformat_new_stream(restrm->ofmt_ctx, NULL);
    if (!out_stream) {
        fprintf(stderr, "%s: Failed allocating output stream\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    encoder = avcodec_find_encoder(restrm->stream_ctx[indx].dec_ctx->codec_id);
    if (!encoder) {
        fprintf(stderr, "%s: Could not find audio encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    restrm->stream_ctx[indx].enc_ctx = avcodec_alloc_context3(encoder);
    if (!restrm->stream_ctx[indx].enc_ctx) {
        fprintf(stderr, "%s: Could not allocate audio encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    /*  Copy from / to parms causes a memory leak so we do it manually. */
    restrm->stream_ctx[indx].enc_ctx->sample_rate = restrm->stream_ctx[indx].dec_ctx->sample_rate;
    //restrm->stream_ctx[indx].enc_ctx->ch_layout = restrm->stream_ctx[indx].dec_ctx->ch_layout; /* For ffmpeg 5.0+ only */
    restrm->stream_ctx[indx].enc_ctx->channel_layout = restrm->stream_ctx[indx].dec_ctx->channel_layout;
    restrm->stream_ctx[indx].enc_ctx->channels = restrm->stream_ctx[indx].dec_ctx->channels;
    restrm->stream_ctx[indx].enc_ctx->sample_fmt = restrm->stream_ctx[indx].dec_ctx->sample_fmt;
    restrm->stream_ctx[indx].enc_ctx->time_base = restrm->stream_ctx[indx].dec_ctx->time_base;

    /* Third parameter can be used to pass settings to encoder */
    retcd = avcodec_open2(restrm->stream_ctx[indx].enc_ctx, encoder, NULL);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not open audio encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    retcd = avcodec_parameters_from_context(out_stream->codecpar, restrm->stream_ctx[indx].enc_ctx);
    if (retcd < 0) {
        fprintf(stderr, "%s: Failed to copy audio encoder parameters to stream\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    if (restrm->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        restrm->stream_ctx[indx].enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    out_stream->time_base = restrm->stream_ctx[indx].enc_ctx->time_base;

    return 0;
}

void writer_close_encoder(ctx_restream *restrm) {
    int indx;

    snprintf(restrm->function_name, 1024, "%s", "writer_close_encoder");
    restrm->watchdog_playlist = av_gettime_relative();

    if (restrm->stream_ctx != NULL) {
        indx = 0;
        while (indx < restrm->stream_count) {
            if (restrm->stream_ctx[indx].enc_ctx != NULL) {
                avcodec_free_context(&restrm->stream_ctx[indx].enc_ctx);
                restrm->stream_ctx[indx].enc_ctx = NULL;
            }
            indx++;
        }
    }
    if (restrm->ofmt_ctx != NULL) {
        avformat_free_context(restrm->ofmt_ctx);
        restrm->ofmt_ctx = NULL;
    }

}

void writer_close(ctx_restream *restrm) {

    snprintf(restrm->function_name, 1024, "%s", "writer_close 01");
    restrm->watchdog_playlist = av_gettime_relative();

    reader_start(restrm);

    snprintf(restrm->function_name, 1024, "%s", "writer_close 02");
    restrm->watchdog_playlist = av_gettime_relative();

    if (restrm->ofmt_ctx) {
        if (restrm->ofmt_ctx->pb != NULL) {

            snprintf(restrm->function_name, 1024, "%s", "writer_close 03");
            restrm->watchdog_playlist = av_gettime_relative();

            av_write_trailer(restrm->ofmt_ctx); /* Frees some memory*/

            snprintf(restrm->function_name, 1024, "%s", "writer_close 04");
            restrm->watchdog_playlist = av_gettime_relative();

            avio_closep(&restrm->ofmt_ctx->pb);

            restrm->ofmt_ctx->pb = NULL;
        }
        snprintf(restrm->function_name, 1024, "%s", "writer_close 05");
        restrm->watchdog_playlist = av_gettime_relative();

        writer_close_encoder(restrm);
    }

    snprintf(restrm->function_name, 1024, "%s", "writer_close 06");
    restrm->watchdog_playlist = av_gettime_relative();

    reader_close(restrm);

    restrm->pipe_state = PIPE_IS_CLOSED;

    snprintf(restrm->function_name, 1024, "%s", "writer_close 07");
    restrm->watchdog_playlist = av_gettime_relative();

}

int writer_init(ctx_restream *restrm) {

    int retcd, indx;
    const AVOutputFormat *out_fmt;

    if (finish == TRUE) return -1;
    snprintf(restrm->function_name, 1024, "%s", "writer_init");
    restrm->watchdog_playlist = av_gettime_relative();

    if (restrm->ifmt_ctx == NULL) {
        fprintf(stderr, "%s: no input file provided\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    out_fmt = av_guess_format("matroska", NULL, NULL);
    if (out_fmt == NULL) {
        fprintf(stderr, "%s: av_guess_format failed\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    retcd = avformat_alloc_output_context2(&restrm->ofmt_ctx, out_fmt, NULL, restrm->out_filename);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not create output context\n", restrm->guide_info->guide_displayname);
        return -1;
    }
    restrm->ofmt_ctx->interrupt_callback.callback = restrm_interrupt;
    restrm->ofmt_ctx->interrupt_callback.opaque = restrm;

    restrm->ofmt = restrm->ofmt_ctx->oformat;
    for (indx = 0; indx < restrm->ifmt_ctx->nb_streams; indx++) {
        if (restrm->stream_ctx[indx].dec_ctx != NULL) {
            if (restrm->stream_ctx[indx].dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (writer_init_video(restrm, indx) < 0) return -1; /* Message already reported */
            } else if (restrm->stream_ctx[indx].dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                if (writer_init_audio(restrm, indx) < 0) return -1; /* Message already reported */
            }
        }
        snprintf(restrm->function_name, 1024, "%s", "writer_init");
    }

    av_dump_format(restrm->ofmt_ctx, 0, restrm->out_filename, 1);

    return 0;
}

int writer_init_open(ctx_restream *restrm) {

    int retcd;
    char errstr[128];

    //if (finish == TRUE) return -1;
    snprintf(restrm->function_name, 1024, "%s", "writer_init_open");
    restrm->watchdog_playlist = av_gettime_relative();

    writer_close(restrm);

    reader_start(restrm);
        retcd = writer_init(restrm);
        if (retcd < 0){
            fprintf(stderr,"%s: Failed to open the new connection\n"
                ,restrm->guide_info->guide_displayname);
        }
    reader_close(restrm);

    retcd = avio_open(&restrm->ofmt_ctx->pb, restrm->out_filename, AVIO_FLAG_WRITE);
    if (retcd < 0) {
        fprintf(stderr, "%s: AVIO Open 1 Failed '%s'", restrm->guide_info->guide_displayname, restrm->out_filename);
        return -1;
    }

    retcd = avformat_write_header(restrm->ofmt_ctx, NULL);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        fprintf(stderr, "%s: avformat_write_header error. writer_init_open %s\n"
                    , restrm->guide_info->guide_displayname, errstr);
        //writer_close(restrm);
        reader_start(restrm);
            if (restrm->ofmt_ctx) {
                if (restrm->ofmt_ctx->pb != NULL) {
                    avio_closep(&restrm->ofmt_ctx->pb);
                    restrm->ofmt_ctx->pb = NULL;
                }
                writer_close_encoder(restrm);
            }
            restrm->ofmt_ctx = NULL;
            retcd = writer_init(restrm);
            if (retcd < 0){
                fprintf(stderr,"%s: Failed to open the new connection\n"
                    ,restrm->guide_info->guide_displayname);
            }
        reader_close(restrm);
        restrm->pipe_state = PIPE_NEEDS_RESET;
        return -1;
    }

    return 0;
}

void writer_packet(ctx_restream *restrm) {

    int retcd, indx;
    int64_t new_ts;

    //if (finish == TRUE) return;
    snprintf(restrm->function_name, 1024, "%s", "writer_packet");
    restrm->watchdog_playlist = av_gettime_relative();

    if ((restrm->pkt->stream_index != restrm->video_index) &&
        (restrm->pkt->stream_index != restrm->audio_index)) {
        return;
    }

    if (restrm->pkt->pts != AV_NOPTS_VALUE) {
        if (restrm->pkt->stream_index == restrm->video_index) {
            if (restrm->pkt->pts <= restrm->ts_file.video) {
                new_ts = AV_NOPTS_VALUE;
            } else {
                new_ts = restrm->pkt->pts -
                    restrm->ts_file.video +
                    restrm->ts_base.video;
                restrm->ts_out.video = new_ts;
            }
            restrm->pkt->pts = new_ts;
        } else {
            if (restrm->pkt->pts < restrm->ts_file.audio) {
                new_ts = AV_NOPTS_VALUE;
            } else {
                new_ts = restrm->pkt->pts -
                    restrm->ts_file.audio +
                    restrm->ts_base.audio;
                restrm->ts_out.audio = new_ts;
            }
            restrm->pkt->pts = new_ts;
        }
    }

    if (restrm->pkt->dts != AV_NOPTS_VALUE) {
        if (restrm->pkt->stream_index == restrm->video_index) {
            if (restrm->pkt->dts < restrm->ts_file.video) {
                new_ts = AV_NOPTS_VALUE;
            } else {
                new_ts = restrm->pkt->dts -
                    restrm->ts_file.video +
                    restrm->ts_base.video;
                if (restrm->ts_out.video < new_ts) {
                    restrm->ts_out.video = new_ts;
                }
            }
            restrm->pkt->dts = new_ts;
        } else {
            if (restrm->pkt->dts < restrm->ts_file.audio) {
                new_ts = AV_NOPTS_VALUE;
            } else {
                new_ts = restrm->pkt->dts -
                    restrm->ts_file.audio +
                    restrm->ts_base.audio;
                if (restrm->ts_out.audio < new_ts) {
                    restrm->ts_out.audio = new_ts;
                }
            }
            restrm->pkt->dts = new_ts;
        }
    } else {
        restrm->pkt->dts = restrm->pkt->pts;
    }

/*
    fprintf(stderr, "ts_base %ld %ld ts_file %ld %ld pts %ld %d dts %ld %d\n"
        ,restrm->ts_base.audio,restrm->ts_base.video
        ,restrm->ts_file.audio,restrm->ts_file.video
        ,restrm->pkt->pts,restrm->pkt->stream_index
        ,restrm->pkt->dts,restrm->pkt->stream_index

        );

*/

    //retcd = av_write_frame(restrm->ofmt_ctx, restrm->pkt);
    retcd = av_interleaved_write_frame(restrm->ofmt_ctx, restrm->pkt);

    if (retcd < 0){
        //fprintf(stderr, "%s: write packet: reset \n", restrm->guide_info->guide_displayname);
        restrm->pipe_state = PIPE_NEEDS_RESET;
    }

    return;

}
