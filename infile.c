

#include "restream.h"
#include "guide.h"
#include "playlist.h"
#include "infile.h"
#include "reader.h"
#include "writer.h"


int infile_init_decoder(ctx_restream *restrm, int stream_index){
    /* Simple routine to fill decoder and get a video packet */
    int retcd;

    if (finish) return -1;

    snprintf(restrm->function_name,1024,"%s: infile_init_decoder %d"
        ,restrm->guide_info->guide_displayname, stream_index);
    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    av_packet_unref(&restrm->pkt);
    av_init_packet(&restrm->pkt);
    restrm->pkt.data = NULL;
    restrm->pkt.size = 0;

    retcd = av_read_frame(restrm->ifmt_ctx, &restrm->pkt);
    if (retcd == AVERROR_EOF) return retcd;

    if (retcd == 0 ) {
        retcd = avcodec_send_packet(
            restrm->stream_ctx[stream_index].dec_ctx
            , &restrm->pkt);
    }
    restrm->frame_in = av_frame_alloc();
    if (retcd == 0 ) {
        retcd = avcodec_receive_frame(
            restrm->stream_ctx[stream_index].dec_ctx
            , restrm->frame_in);
    }
    if (retcd == AVERROR(EAGAIN) || retcd == AVERROR_EOF) {
        /* The frame is not ready to be consumed.  Return normal*/
        av_frame_free(&restrm->frame_in);
        return 0;
    }
    restrm->dts_frame = restrm->frame_in->pkt_dts;
    av_frame_free(&restrm->frame_in);

    return 0;

}

int infile_init(ctx_restream *restrm){

    int retcd;
    int stream_index;
    char errstr[128];

    snprintf(restrm->function_name,1024,"%s","infile_init");
    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    //fprintf(stderr,"%s: Initializing : >%s< \n"
    //    ,restrm->guide_info->guide_displayname, restrm->in_filename);

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
    //restrm->stream_ctx = av_mallocz_array(restrm->stream_count, sizeof(*restrm->stream_ctx));
    restrm->stream_ctx = malloc(restrm->stream_count * sizeof(StreamContext));
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
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
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

        /* Reencode video & audio and remux subtitles etc. */
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
        }

        restrm->stream_ctx[stream_index].dec_ctx = codec_ctx;

        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
            codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {

            /* Read pkts to get correct start time */
            av_packet_unref(&restrm->pkt);
            av_init_packet(&restrm->pkt);
            restrm->pkt.data = NULL;
            restrm->pkt.size = 0;

            snprintf(restrm->function_name,1024,"%s","infile_init 04");
            restrm->watchdog_playlist = av_gettime_relative() + 5000000;

            retcd = av_read_frame(restrm->ifmt_ctx, &restrm->pkt);
            while (((restrm->pkt.stream_index != stream_index) ||
                    (restrm->pkt.dts == AV_NOPTS_VALUE)) &&
                   (retcd == 0)) {
                retcd = infile_init_decoder(restrm,stream_index);
            }
            if (retcd != 0){
                fprintf(stderr, "%s: Failed to find dts #%d\n"
                    ,restrm->guide_info->guide_displayname, stream_index);
                return -1;
            }

            snprintf(restrm->function_name,1024,"%s","infile_init 05");
            restrm->watchdog_playlist = av_gettime_relative() + 5000000;

            /* Now get a valid DTS from the decoder
             * We need to initialize the decoder so that
             * in later steps we can get a pixel format.
            */
            restrm->dts_start = restrm->pkt.dts;
            while (((restrm->pkt.stream_index != stream_index) ||
                    (restrm->dts_start < restrm->dts_frame)) &&
                   (retcd == 0)) {
                retcd = infile_init_decoder(restrm,stream_index);
            }
            if (retcd != 0){
                fprintf(stderr, "%s: Failed to find dts #%d\n"
                    ,restrm->guide_info->guide_displayname, stream_index);
                FILE *f_nodts;
                f_nodts = fopen("no_dts.log","a");
                    fputs(restrm->in_filename, f_nodts);
                    fputs("\n", f_nodts);
                fclose(f_nodts);
                return -1;
            }

            snprintf(restrm->function_name,1024,"%s","infile_init 06");
            restrm->watchdog_playlist = av_gettime_relative() + 5000000;

            restrm->time_start = av_gettime_relative();
            restrm->dts_start = av_rescale(restrm->pkt.dts, 1000000
                ,restrm->ifmt_ctx->streams[restrm->pkt.stream_index]->time_base.den);
            restrm->dts_last = restrm->dts_start;

            snprintf(restrm->function_name,1024,"%s","infile_init 07");
            restrm->watchdog_playlist = av_gettime_relative() + 5000000;

            /* If we still do not have a pix fmt, read more to fill decoder*/
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
                snprintf(restrm->function_name,1024,"%s","infile_init 08");
                restrm->watchdog_playlist = av_gettime_relative() + 5000000;

                while (((restrm->pkt.stream_index != stream_index) ||
                        (restrm->stream_ctx[stream_index].dec_ctx->pix_fmt == -1)) &&
                       (retcd == 0)) {
                    retcd = infile_init_decoder(restrm,stream_index);
                }
                if (retcd != 0){
                    fprintf(stderr, "%s: Failed to find pixel format #%d\n"
                        ,restrm->guide_info->guide_displayname, stream_index);
                    return -1;
                }

            }
        }

        stream_index ++;
    }

    return 0;

}

void infile_close(ctx_restream *restrm){
    int indx;

    snprintf(restrm->function_name,1024,"%s","infile_close");
    restrm->watchdog_playlist = av_gettime_relative();

    if (restrm->ifmt_ctx == NULL) return;

    indx = 0;
    while (indx < restrm->stream_count){
        if (restrm->stream_ctx[indx].dec_ctx !=  NULL){
            avcodec_free_context(&restrm->stream_ctx[indx].dec_ctx);
            restrm->stream_ctx[indx].dec_ctx =  NULL;
        }
        indx++;
    }
    if (restrm->stream_ctx != NULL) free(restrm->stream_ctx);
    avformat_close_input(&restrm->ifmt_ctx);
    restrm->ifmt_ctx = NULL;
    //fprintf(stderr, "%s: Input closed \n"
    //        ,restrm->guide_info->guide_displayname);
}

void infile_wait(ctx_restream *restrm){

    int64_t tm_diff, dts_diff, tot_diff, dts;

    snprintf(restrm->function_name,1024,"%s","infile_wait");

    restrm->soft_restart = 1;

    if (finish) return;

    if (restrm->pkt.stream_index != restrm->video_index) return;

    int64_t tmnow;
    tmnow  = av_gettime_relative();
    if (((tmnow - restrm->connect_start)<2500000) &&
        (restrm->pipe_state == PIPE_IS_OPEN)){
        return;
    }

    restrm->watchdog_playlist = av_gettime_relative();

    if (restrm->pkt.dts != AV_NOPTS_VALUE) {
        dts = av_rescale(restrm->pkt.dts, 1000000
            ,restrm->ifmt_ctx->streams[restrm->pkt.stream_index]->time_base.den);

        if (dts < restrm->dts_last) return;
        restrm->dts_last = dts;

        /* How much time has really elapsed since the start of movie*/
        tm_diff = av_gettime_relative() - restrm->time_start;

        /* How much time the dts wants us to be at since the start of the movie */
        dts_diff = dts - restrm->dts_start;

        /* How much time we need to wait to get in sync*/
        tot_diff = dts_diff - tm_diff;
        if (tot_diff > 0){
            if (tot_diff < 1000000){
                snprintf(restrm->function_name,1024,"%s %ld","infile_wait",tot_diff);
                SLEEP(0, tot_diff * 1000);
            } else {
                if (restrm->pipe_state == PIPE_NEEDS_RESET ){
                fprintf(stderr
                    ,"%s: Excessive wait time "
                     " dtsdiff: %ld dts:%ld pktdts:%ld"
                     " tdiff: %ld totdif:%ld last: %ld\n"
                    , restrm->guide_info->guide_displayname
                    , dts_diff, dts ,restrm->pkt.dts
                    , tm_diff, tot_diff, restrm->dts_last
                    );
                }
                /* reset all our times to see if we can get in sync*/
                restrm->time_start = av_gettime_relative();
                restrm->dts_start = av_rescale(restrm->pkt.dts, 1000000
                    ,restrm->ifmt_ctx->streams[restrm->pkt.stream_index]->time_base.den);
                restrm->dts_last = restrm->dts_start;
            }
        }
    }
}
