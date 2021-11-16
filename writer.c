

#include "restream.h"
#include "guide.h"
#include "playlist.h"
#include "infile.h"
#include "reader.h"
#include "writer.h"

int writer_init_video(ctx_restream *restrm, int indx) {

    AVCodecContext *enc_ctx, *dec_ctx;
    AVCodec *encoder;
    AVStream *out_stream;
    int retcd;

    snprintf(restrm->function_name, 1024, "%s", "writer_init_video");
    restrm->watchdog_playlist = av_gettime_relative();

    dec_ctx = restrm->stream_ctx[indx].dec_ctx;

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

    enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) {
        fprintf(stderr, "%s: Could not allocate video encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    (void)dec_ctx;

    retcd = avcodec_parameters_from_context(out_stream->codecpar, dec_ctx);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not copy parms from video decoder\n"
            ,restrm->guide_info->guide_displayname);
        return -1;
    }

    retcd = avcodec_parameters_to_context(enc_ctx, out_stream->codecpar);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not copy parms to video encoder\n"
            ,restrm->guide_info->guide_displayname);
        return -1;
    }

    enc_ctx->width = dec_ctx->width;
    enc_ctx->height = dec_ctx->height;
    enc_ctx->time_base = dec_ctx->time_base;
    if (dec_ctx->pix_fmt == -1) {
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    } else {
        enc_ctx->pix_fmt = dec_ctx->pix_fmt;
    }

    retcd = avcodec_open2(enc_ctx, encoder, NULL);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not open video encoder\n", restrm->guide_info->guide_displayname);
        fprintf(stderr, "%s: %dx%d %d %d\n"
            , restrm->guide_info->guide_displayname
            , enc_ctx->width, enc_ctx->height
            , enc_ctx->pix_fmt
            , enc_ctx->time_base.den);
        return -1;
    }

    if (restrm->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    out_stream->time_base = enc_ctx->time_base;

    retcd = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not copy parms from decoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    restrm->stream_ctx[indx].enc_ctx = enc_ctx;

    return 0;
}

int writer_init_audio(ctx_restream *restrm, int indx) {

    AVStream *out_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int retcd;

    snprintf(restrm->function_name, 1024, "%s", "writer_init_audio");
    restrm->watchdog_playlist = av_gettime_relative();

    out_stream = avformat_new_stream(restrm->ofmt_ctx, NULL);
    if (!out_stream) {
        fprintf(stderr, "%s: Failed allocating output stream\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    dec_ctx = restrm->stream_ctx[indx].dec_ctx;

    encoder = avcodec_find_encoder(dec_ctx->codec_id);
    if (!encoder) {
        fprintf(stderr, "%s: Could not find audio encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) {
        fprintf(stderr, "%s: Could not allocate audio encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    retcd = avcodec_parameters_from_context(out_stream->codecpar, dec_ctx);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not copy parms from audio decoder\n"
            ,restrm->guide_info->guide_displayname);
        return -1;
    }

    retcd = avcodec_parameters_to_context(enc_ctx, out_stream->codecpar);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not copy parms to audio encoder\n"
            ,restrm->guide_info->guide_displayname);
        return -1;
    }


    enc_ctx->sample_rate = dec_ctx->sample_rate;
    enc_ctx->channel_layout = dec_ctx->channel_layout;
    enc_ctx->channels = dec_ctx->channels;
    enc_ctx->sample_fmt = dec_ctx->sample_fmt;
    enc_ctx->time_base = dec_ctx->time_base;

    /* Third parameter can be used to pass settings to encoder */
    retcd = avcodec_open2(enc_ctx, encoder, NULL);
    if (retcd < 0) {
        fprintf(stderr, "%s: Could not open audio encoder\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    retcd = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (retcd < 0) {
        fprintf(stderr, "%s: Failed to copy audio encoder parameters to stream\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    if (restrm->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    out_stream->time_base = enc_ctx->time_base;
    restrm->stream_ctx[indx].enc_ctx = enc_ctx;

    return 0;
}

void writer_close_encoder(ctx_restream *restrm) {
    int indx;

    snprintf(restrm->function_name, 1024, "%s", "writer_close_encoder");
    restrm->watchdog_playlist = av_gettime_relative();

    indx = 0;
    while (indx < restrm->stream_count) {
        if (restrm->stream_ctx[indx].enc_ctx != NULL) {
            avcodec_free_context(&restrm->stream_ctx[indx].enc_ctx);
            restrm->stream_ctx[indx].enc_ctx = NULL;
        }
        indx++;
    }
    avformat_free_context(restrm->ofmt_ctx);
    restrm->ofmt_ctx = NULL;

}

void writer_close(ctx_restream *restrm) {

    snprintf(restrm->function_name, 1024, "%s", "writer_close");
    restrm->watchdog_playlist = av_gettime_relative();

    /*
    fprintf(stderr,"%s: writer_close enter\n"
        , restrm->guide_info->guide_displayname
        );
    */

    reader_start(restrm);

    if (restrm->ofmt_ctx) {
        if (restrm->ofmt_ctx->pb != NULL) {
            av_write_trailer(restrm->ofmt_ctx); /* Frees some memory*/
            avio_closep(&restrm->ofmt_ctx->pb);
        }
        writer_close_encoder(restrm);
    }
    restrm->ofmt_ctx = NULL;

    reader_close(restrm);

    restrm->pipe_state = PIPE_IS_CLOSED;

    /*
    fprintf(stderr,"%s: writer_close exit\n"
        , restrm->guide_info->guide_displayname
        );
    */
}

int writer_init(ctx_restream *restrm) {

    int retcd, indx;
    AVOutputFormat *out_fmt;

    snprintf(restrm->function_name, 1024, "%s", "writer_init");
    restrm->watchdog_playlist = av_gettime_relative();

    /*
    fprintf(stderr,"%s: writer_init enter\n"
        , restrm->guide_info->guide_displayname
        );
    */

    if (restrm->ifmt_ctx == NULL) {
        fprintf(stderr, "%s: no input file provided\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    out_fmt = av_guess_format("matroska", NULL, NULL);
    out_fmt->flags |= ~AVFMT_GLOBALHEADER;
    if (!out_fmt) {
        fprintf(stderr, "%s: av_guess_format failed\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    avformat_alloc_output_context2(&restrm->ofmt_ctx, out_fmt, NULL, restrm->out_filename);
    if (!restrm->ofmt_ctx) {
        fprintf(stderr, "%s: Could not create output context\n", restrm->guide_info->guide_displayname);
        return -1;
    }

    restrm->ofmt_ctx->interrupt_callback.callback = restrm_interrupt;
    restrm->ofmt_ctx->interrupt_callback.opaque = restrm;

    restrm->ofmt = restrm->ofmt_ctx->oformat;
    for (indx = 0; indx < restrm->ifmt_ctx->nb_streams; indx++) {
        if (restrm->stream_ctx[indx].dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            retcd = writer_init_video(restrm, indx);
            if (retcd < 0)
                return -1; /* Message already reported */
        } else if (restrm->stream_ctx[indx].dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            retcd = writer_init_audio(restrm, indx);
            if (retcd < 0)
                return -1; /* Message already reported */
        }
        snprintf(restrm->function_name, 1024, "%s", "writer_init");
    }

    av_dump_format(restrm->ofmt_ctx, 0, restrm->out_filename, 1);

    /*
    fprintf(stderr,"%s: writer_init exit\n"
        , restrm->guide_info->guide_displayname
        );
    */

    return 0;
}

int writer_init_open(ctx_restream *restrm) {

    int retcd;
    char errstr[128];

    snprintf(restrm->function_name, 1024, "%s", "writer_init_open");
    restrm->watchdog_playlist = av_gettime_relative();

    /*
    fprintf(stderr,"%s: writer_init_open enter\n"
        , restrm->guide_info->guide_displayname
        );
    */

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
        writer_close(restrm);
        reader_start(restrm);
            retcd = writer_init(restrm);
            if (retcd < 0){
                fprintf(stderr,"%s: Failed to open the new connection\n"
                    ,restrm->guide_info->guide_displayname);
            }
        reader_close(restrm);
        restrm->pipe_state = PIPE_NEEDS_RESET;
        return -1;
    }

    /*
    fprintf(stderr,"%s: writer_init_open exit\n"
        , restrm->guide_info->guide_displayname
        );
    */

    return 0;
}

void writer_packet(ctx_restream *restrm) {

    int retcd, indx;
    int64_t new_ts;

    snprintf(restrm->function_name, 1024, "%s", "writer_packet");
    restrm->watchdog_playlist = av_gettime_relative();

    if ((restrm->pkt.stream_index != restrm->video_index) &&
        (restrm->pkt.stream_index != restrm->audio_index)) {
        return;
    }

    if (restrm->pkt.pts != AV_NOPTS_VALUE) {
        if (restrm->pkt.stream_index == restrm->video_index) {
            new_ts = av_rescale(restrm->pkt.pts, 1000
                , restrm->ifmt_ctx->streams[restrm->pkt.stream_index]->time_base.den)
                - av_rescale(restrm->dts_strtin.video, 1000, 1000000)
                + restrm->dts_out.video;
            restrm->pkt.pts = new_ts;
        } else {
            new_ts = av_rescale(restrm->pkt.pts, 1000
                , restrm->ifmt_ctx->streams[restrm->pkt.stream_index]->time_base.den)
                - av_rescale(restrm->dts_strtin.video, 1000, 1000000)
                + restrm->dts_out.audio;
            restrm->pkt.pts = new_ts;
        }
    }

    if (restrm->pkt.dts != AV_NOPTS_VALUE) {
        if (restrm->pkt.stream_index == restrm->video_index) {
            new_ts = av_rescale(restrm->pkt.dts, 1000
                , restrm->ifmt_ctx->streams[restrm->pkt.stream_index]->time_base.den)
                - av_rescale(restrm->dts_strtin.video, 1000, 1000000)
                + restrm->dts_out.video;
            restrm->dts_lstout.video = new_ts;
            restrm->pkt.dts = new_ts;
        } else {
            new_ts = av_rescale(restrm->pkt.dts, 1000
                , restrm->ifmt_ctx->streams[restrm->pkt.stream_index]->time_base.den)
                - av_rescale(restrm->dts_strtin.video, 1000, 1000000)
                + restrm->dts_out.audio;
            restrm->dts_lstout.audio = new_ts;
            restrm->pkt.dts = new_ts;
        }
    }

    //retcd = av_write_frame(restrm->ofmt_ctx, &restrm->pkt);
    retcd = av_interleaved_write_frame(restrm->ofmt_ctx, &restrm->pkt);

    if (retcd < 0){
        //fprintf(stderr, "%s: write packet: reset \n", restrm->guide_info->guide_displayname);
        restrm->pipe_state = PIPE_NEEDS_RESET;
    }

    return;

}
