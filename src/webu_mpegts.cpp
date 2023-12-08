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
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_mpegts.hpp"
#include "infile.hpp"


void webu_mpegts_free_context(ctx_webui *webui)
{

    if (webui->wfile.audio.codec_ctx != nullptr) {
        myavcodec_close(webui->wfile.audio.codec_ctx);
        webui->wfile.audio.codec_ctx = nullptr;
    }
    if (webui->wfile.video.codec_ctx != nullptr) {
        myavcodec_close(webui->wfile.video.codec_ctx);
        webui->wfile.video.codec_ctx = nullptr;
    }

    if (webui->wfile.fmt_ctx != nullptr) {
        if (webui->wfile.fmt_ctx->pb != nullptr) {
            if (webui->wfile.fmt_ctx->pb->buffer != nullptr) {
                av_free(webui->wfile.fmt_ctx->pb->buffer);
                webui->wfile.fmt_ctx->pb->buffer = nullptr;
            }
            avio_context_free(&webui->wfile.fmt_ctx->pb);
            webui->wfile.fmt_ctx->pb = nullptr;
        }
        avformat_free_context(webui->wfile.fmt_ctx);
        webui->wfile.audio.strm = nullptr;
        webui->wfile.video.strm = nullptr;
        webui->wfile.fmt_ctx = nullptr;
    }
    LOG_MSG(DBG, NO_ERRNO, "closed");
}

static void webu_mpegts_resetpos(ctx_webui *webui)
{
    webui->stream_pos = 0;
    webui->resp_used = 0;
}

static void webu_mpegts_packet_wait(ctx_webui *webui, AVPacket *pkt)
{
    int64_t tm_diff, pts_diff, tot_diff;

    if (finish == true) {
        return;
    }

    if (pkt->pts == AV_NOPTS_VALUE) {
        return;
    }

    /* How much time the pts wants us to be at since the start of the movie */
    if (pkt->stream_index == webui->wfile.audio.index) {
        pts_diff = av_rescale(pkt->pts - webui->wfile.audio.start_pts
            , 1000000, webui->wfile.audio.strm->time_base.den);
    } else {
        pts_diff = av_rescale(pkt->pts - webui->wfile.video.start_pts
            , 1000000, webui->wfile.video.strm->time_base.den);
    }

    /* How much time has really elapsed since the start of movie*/
    tm_diff = av_gettime_relative() - webui->wfile.time_start;

    /* How much time we need to wait to get in sync*/
    tot_diff = pts_diff - tm_diff;

    if (tot_diff > 0) {
        if (tot_diff < 1000000){
            SLEEP(0, tot_diff * 1000);
        } else {
            if (pkt->stream_index ==webui->wfile.audio.index) {
                LOG_MSG(INF, NO_ERRNO, "sync index %d last %d pktpts %d base %d src %d"
                    , pkt->stream_index
                    , webui->wfile.audio.last_pts
                    , pkt->pts
                    , webui->wfile.audio.base_pdts
                    ,webui->chitm->pktarray[webui->chitm->pktarray_lastwritten].packet->pts
                );
            } else {
                LOG_MSG(INF, NO_ERRNO, "sync index %d last %d pktpts %d base %d src %d"
                    , pkt->stream_index
                    , webui->wfile.video.last_pts
                    , pkt->pts
                    , webui->wfile.video.base_pdts
                    ,webui->chitm->pktarray[webui->chitm->pktarray_lastwritten].packet->pts
                );
            }
            webui->wfile.time_start = av_gettime_relative();
            if (pkt->stream_index == webui->wfile.audio.index) {
                webui->wfile.audio.start_pts = pkt->pts;
                webui->wfile.video.start_pts = av_rescale_q(pkt->pts
                    , webui->wfile.audio.strm->time_base
                    , webui->wfile.video.strm->time_base);
            } else {
                webui->wfile.video.start_pts = pkt->pts;
                webui->wfile.audio.start_pts = av_rescale_q(pkt->pts
                    , webui->wfile.video.strm->time_base
                    , webui->wfile.audio.strm->time_base);
            }
            webui->wfile.audio.last_pts = webui->wfile.audio.start_pts;
            webui->wfile.video.last_pts = webui->wfile.video.start_pts;
        }
    }
}

static void webu_mpegts_packet_pts(ctx_webui *webui, AVPacket *pkt, int indx)
{
    int64_t ts_interval, base_pdts, last_pts, src_pts;
    int64_t strm_st_pts, file_st_pts;
    AVRational tmpdst, tbase;

    src_pts = pkt->pts;
    if (webui->wfile.time_start == -1) {
        webui->wfile.time_start = av_gettime_relative();
        webui->wfile.audio.start_pts = av_rescale_q(
            src_pts - webui->chitm->pktarray[indx].start_pts
            , webui->chitm->pktarray[indx].timebase
            , webui->wfile.audio.strm->time_base);
        webui->wfile.video.start_pts = av_rescale_q(
            src_pts - webui->chitm->pktarray[indx].start_pts
            , webui->chitm->pktarray[indx].timebase
            , webui->wfile.video.strm->time_base);
        if (webui->wfile.audio.start_pts <= 0) {
            webui->wfile.audio.start_pts = 1;
        }
        if (webui->wfile.video.start_pts <= 0) {
            webui->wfile.video.start_pts = 1;
        }
        webui->wfile.audio.last_pts = 0;
        webui->wfile.video.last_pts = 0;
        webui->wfile.audio.base_pdts = 0;
        webui->wfile.video.base_pdts = 0;
        webui->file_cnt = webui->chitm->pktarray[indx].file_cnt;
/*
        LOG_MSG(DBG, NO_ERRNO
            ,"init %d %d pts %d %d %d/%d st-a %d %d/%d st-v %d %d/%d"
            , pkt->stream_index
            , webui->file_cnt
            , src_pts
            , webui->chitm->pktarray[indx].start_pts
            , webui->chitm->pktarray[indx].timebase.num
            , webui->chitm->pktarray[indx].timebase.den
            , webui->wfile.audio.start_pts
            , webui->wfile.audio.strm->time_base.num
            , webui->wfile.audio.strm->time_base.den
            , webui->wfile.video.start_pts
            , webui->wfile.video.strm->time_base.num
            , webui->wfile.video.strm->time_base.den
            );
*/
    }

    if (pkt->stream_index == webui->wfile.audio.index) {
        tmpdst = webui->wfile.audio.codec_ctx->time_base;
        tmpdst = webui->wfile.audio.strm->time_base;
        last_pts = webui->wfile.audio.last_pts;
        base_pdts = webui->wfile.audio.base_pdts;
        strm_st_pts = webui->wfile.audio.start_pts;
    } else {
        tmpdst = webui->wfile.video.strm->time_base;
        last_pts = webui->wfile.video.last_pts;
        base_pdts = webui->wfile.video.base_pdts;
        strm_st_pts = webui->wfile.video.start_pts;
    }
    file_st_pts = webui->chitm->pktarray[indx].start_pts;
    tbase = webui->chitm->pktarray[indx].timebase;

    if (pkt->pts != AV_NOPTS_VALUE) {
        if (webui->file_cnt == webui->chitm->pktarray[indx].file_cnt) {
            pkt->pts = av_rescale_q(pkt->pts - file_st_pts , tbase, tmpdst) -
                strm_st_pts + base_pdts;
        } else {
            webui->file_cnt = webui->chitm->pktarray[indx].file_cnt;
            base_pdts = last_pts + strm_st_pts;
            pkt->pts = av_rescale_q(pkt->pts - file_st_pts , tbase, tmpdst) -
                strm_st_pts + base_pdts;
            if (pkt->pts == last_pts) {
                pkt->pts++;
            }
            if (pkt->stream_index == webui->wfile.audio.index) {
                webui->wfile.audio.base_pdts = base_pdts;
                webui->wfile.video.base_pdts = av_rescale_q(base_pdts
                    , webui->wfile.audio.strm->time_base
                    , webui->wfile.video.strm->time_base);
            } else {
                webui->wfile.video.base_pdts = base_pdts;
                webui->wfile.audio.base_pdts = av_rescale_q(base_pdts
                    , webui->wfile.video.strm->time_base
                    , webui->wfile.audio.strm->time_base);
            }
        }
        if (pkt->pts <= 0) {
            pkt->pts = 1;
        }
    }

    if (pkt->dts != AV_NOPTS_VALUE) {
        if (webui->file_cnt == webui->chitm->pktarray[indx].file_cnt) {
            pkt->dts = av_rescale_q(pkt->dts - file_st_pts , tbase, tmpdst) -
                strm_st_pts + base_pdts;
        } else {
            webui->file_cnt = webui->chitm->pktarray[indx].file_cnt;
            base_pdts = last_pts + strm_st_pts;
            pkt->dts = av_rescale_q(pkt->dts - file_st_pts , tbase, tmpdst) -
                strm_st_pts + base_pdts;
            if (pkt->dts == last_pts) {
                pkt->dts++;
            }
            if (pkt->stream_index == webui->wfile.audio.index) {
                webui->wfile.audio.base_pdts = base_pdts;
                webui->wfile.video.base_pdts = av_rescale_q(base_pdts
                    , webui->wfile.audio.strm->time_base
                    , webui->wfile.video.strm->time_base);
            } else {
                webui->wfile.video.base_pdts = base_pdts;
                webui->wfile.audio.base_pdts = av_rescale_q(base_pdts
                    , webui->wfile.video.strm->time_base
                    , webui->wfile.audio.strm->time_base);
            }
        }
        if (pkt->dts <= 0) {
            pkt->dts = 1;
        }
    }

    ts_interval = pkt->duration;
    pkt->duration = av_rescale_q(ts_interval, tbase, tmpdst);
/*
    LOG_MSG(DBG, NO_ERRNO
        ,"data %d %d src %d  %d/%d newpts %d %d tb-a %d/%d %d %d tb-v %d/%d %d %d"
        , pkt->stream_index
        , webui->file_cnt
        , src_pts
        , webui->chitm->pktarray[indx].timebase.num
        , webui->chitm->pktarray[indx].timebase.den
        , pkt->pts
        , pkt->dts
        , webui->wfile.audio.strm->time_base.num
        , webui->wfile.audio.strm->time_base.den
        , webui->wfile.audio.base_pdts
        , webui->wfile.audio.start_pts
        , webui->wfile.video.strm->time_base.num
        , webui->wfile.video.strm->time_base.den
        , webui->wfile.video.base_pdts
        , webui->wfile.video.start_pts
        );
*/
}

/* Write the packet in the array at indx to the output format context */
static void webu_mpegts_packet_write(ctx_webui *webui, int indx)
{
    char errstr[128];
    int retcd;
    AVPacket *pkt;

    pkt = nullptr;
    pkt = mypacket_alloc(pkt);

    pthread_mutex_lock(&webui->chitm->mtx_pktarray);
        webui->chitm->pktarray[indx].iswritten = true;
        webui->chitm->pktarray_lastwritten = indx;
        retcd = mycopy_packet(pkt, webui->chitm->pktarray[indx].packet);
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            LOG_MSG(INF, NO_ERRNO, "av_copy_packet: %s",errstr);
            mypacket_free(pkt);
            pthread_mutex_unlock(&webui->chitm->mtx_pktarray);
            return;
        }
    pthread_mutex_unlock(&webui->chitm->mtx_pktarray);

    /* The source packets have the index of chitm->ofile, we need to
       make sure they match the index of webui->wfile.  If not fix them
    */
    if ((webui->wfile.audio.index != webui->chitm->ofile.audio.index) ||
        (webui->wfile.video.index != webui->chitm->ofile.video.index)) {
        if (pkt->stream_index == webui->chitm->ofile.audio.index) {
            LOG_MSG(DBG, NO_ERRNO,"Swapping audio");
            pkt->stream_index = webui->wfile.audio.index;
        } else if (pkt->stream_index == webui->chitm->ofile.video.index) {
            LOG_MSG(DBG, NO_ERRNO,"Swapping video");
            pkt->stream_index = webui->wfile.video.index;
        }
    }

    webu_mpegts_packet_pts(webui, pkt, indx);

    if (pkt->stream_index == webui->wfile.audio.index) {
        if (pkt->pts > webui->wfile.audio.last_pts) {
            webui->wfile.audio.last_pts = pkt->pts;
        } else {
            return;
        }
    } else {
        if (pkt->pts > webui->wfile.video.last_pts) {
            webui->wfile.video.last_pts = pkt->pts;
        } else {
            return;
        }
    }

    webu_mpegts_packet_wait(webui, pkt);

/*
    LOG_MSG(DBG, NO_ERRNO,"dx %d strm %d newpts %d srcpts %d newdts %d srcdts %d al %d vl %d"
            ,indx, pkt->stream_index
            , pkt->pts
            ,webui->chitm->pktarray[indx].packet->pts
            , pkt->dts
            ,webui->chitm->pktarray[indx].packet->dts
            ,webui->wfile.audio.last_pts
            ,webui->wfile.video.last_pts);
*/

    retcd = av_interleaved_write_frame(webui->wfile.fmt_ctx, pkt);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Error writing frame index %d id %d err %s"
            , indx, webui->chitm->pktarray[indx].idnbr, errstr);
    }
    mypacket_free(pkt);
}

static void webu_mpegts_getimg(ctx_webui *webui)
{
    int indx_next, chk;
    bool pktready;

    pthread_mutex_lock(&webui->chitm->mtx_pktarray);
        if (webui->chitm->pktarray_count == 0) {
            pthread_mutex_unlock(&webui->chitm->mtx_pktarray);
            return;
        }
    pthread_mutex_unlock(&webui->chitm->mtx_pktarray);

    indx_next = webui->chitm->pktarray_lastwritten;
    indx_next = pktarray_indx_next(indx_next);

    pthread_mutex_lock(&webui->chitm->mtx_pktarray);
        if ((webui->chitm->pktarray[indx_next].iswritten == false) &&
            (webui->chitm->pktarray[indx_next].packet != nullptr) &&
            (webui->chitm->pktarray_index != indx_next) ) {
            pktready = true;
        } else {
            pktready = false;
        }
    pthread_mutex_unlock(&webui->chitm->mtx_pktarray);

    chk = 0;
    while ((chk < 1000) && (pktready == false)) {
        SLEEP(0, 250000000L);
        pthread_mutex_lock(&webui->chitm->mtx_pktarray);
            if ((webui->chitm->pktarray[indx_next].iswritten == false) &&
                (webui->chitm->pktarray[indx_next].packet != nullptr) &&
                (webui->chitm->pktarray_index != indx_next) ) {
                pktready = true;
            } else {
                pktready = false;
            }
        pthread_mutex_unlock(&webui->chitm->mtx_pktarray);
        chk++;
    }
    if (chk == 1000) {
        LOG_MSG(INF, NO_ERRNO,"Excessive wait for new packet");
    } else {
        webu_mpegts_packet_write(webui, indx_next);
    }

}


static int webu_mpegts_avio_buf(void *opaque, uint8_t *buf, int buf_size)
{
    ctx_webui *webui =(ctx_webui *)opaque;

    if (webui->resp_size < (size_t)(buf_size + webui->resp_used)) {
        webui->resp_size = (size_t)(buf_size + webui->resp_used);
        webui->resp_image = (unsigned char*)realloc(
            webui->resp_image, webui->resp_size);
/*
        LOG_MSG(DBG, NO_ERRNO
            ,"resp_image reallocated %d %d %d"
            ,webui->resp_size
            ,webui->resp_used
            ,buf_size);
*/
    }

    memcpy(webui->resp_image + webui->resp_used, buf, buf_size);
    webui->resp_used += buf_size;

    return buf_size;
}

static ssize_t webu_mpegts_response(void *cls, uint64_t pos, char *buf, size_t max)
{
    ctx_webui *webui =(ctx_webui *)cls;
    size_t sent_bytes;
    (void)pos;

    if ((webui->app->webcontrol_finish) || (webui->chitm->ch_finish)) {
        return -1;
    }

    if ((webui->stream_pos == 0) && (webui->resp_used == 0)) {
        webu_mpegts_getimg(webui);
    }

    if (webui->resp_used == 0) {
        webu_mpegts_resetpos(webui);
        return 0;
    }

    if ((webui->resp_used - webui->stream_pos) > max) {
        sent_bytes = max;
    } else {
        sent_bytes = webui->resp_used - webui->stream_pos;
    }

    memcpy(buf, webui->resp_image + webui->stream_pos, sent_bytes);

    webui->stream_pos = webui->stream_pos + sent_bytes;
    if (webui->stream_pos >= webui->resp_used) {
        webui->stream_pos = 0;
        webui->resp_used = 0;
    }
/*
    LOG_MSG(DBG, NO_ERRNO
            ,"pos %d sent %d used %d"
            ,webui->stream_pos
            ,sent_bytes
            ,webui->resp_used);
*/
    return sent_bytes;

}

static int webu_mpegts_streams_video(ctx_webui *webui)
{
    int retcd;
    char errstr[128];
    AVCodecContext  *wfl_ctx, *enc_ctx;
    AVDictionary    *opts;
    const AVCodec   *encoder;
    AVStream        *stream;

    opts = NULL;
    webui->wfile.fmt_ctx->video_codec_id = MY_CODEC_ID_H264;
    webui->wfile.fmt_ctx->video_codec = avcodec_find_encoder(MY_CODEC_ID_H264);
    encoder = webui->wfile.fmt_ctx->video_codec;

    webui->wfile.video.strm = avformat_new_stream(webui->wfile.fmt_ctx
        , encoder);
    if (webui->wfile.video.strm == nullptr) {
        LOG_MSG(ERR, NO_ERRNO, "Could not alloc video stream");
        return -1;
    }
    webui->wfile.video.index = webui->wfile.video.strm->index;
    stream = webui->wfile.video.strm;

    webui->wfile.video.codec_ctx = avcodec_alloc_context3(encoder);
    if (webui->wfile.video.codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not allocate video context"
            , webui->chitm->ch_nbr.c_str());
        return -1;
    }

    enc_ctx = webui->chitm->ofile.video.codec_ctx;
    wfl_ctx = webui->wfile.video.codec_ctx;

    wfl_ctx->gop_size      = 15;
    wfl_ctx->codec_id      = MY_CODEC_ID_H264;
    wfl_ctx->codec_type    = AVMEDIA_TYPE_VIDEO;
    wfl_ctx->bit_rate      = 400000;
    wfl_ctx->width         = enc_ctx->width;
    wfl_ctx->height        = enc_ctx->height;
    wfl_ctx->time_base.num = 1;
    wfl_ctx->time_base.den = 90000;
    wfl_ctx->pix_fmt       = MY_PIX_FMT_YUV420P;
    wfl_ctx->max_b_frames  = 1;
    wfl_ctx->flags         |= MY_CODEC_FLAG_GLOBAL_HEADER;
    wfl_ctx->framerate.num  = 30;
    wfl_ctx->framerate.den  = 1;
    av_opt_set(wfl_ctx->priv_data, "profile", "main", 0);
    av_opt_set(wfl_ctx->priv_data, "crf", "22", 0);
    av_opt_set(wfl_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(wfl_ctx->priv_data, "preset", "superfast",0);
    av_dict_set(&opts, "movflags", "empty_moov", 0);

    retcd = avcodec_open2(wfl_ctx, encoder, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO,"Failed to open codec: %s", errstr);
        av_dict_free(&opts);
        return -1;
    }
    av_dict_free(&opts);

    wfl_ctx->gop_size      = 15;
    wfl_ctx->codec_id      = MY_CODEC_ID_H264;
    wfl_ctx->codec_type    = AVMEDIA_TYPE_VIDEO;
    wfl_ctx->bit_rate      = 400000;
    wfl_ctx->width         = enc_ctx->width;
    wfl_ctx->height        = enc_ctx->height;
    wfl_ctx->time_base.num = 1;
    wfl_ctx->time_base.den = 90000;
    wfl_ctx->pix_fmt       = MY_PIX_FMT_YUV420P;
    wfl_ctx->max_b_frames  = 1;
    wfl_ctx->flags         |= MY_CODEC_FLAG_GLOBAL_HEADER;
    wfl_ctx->framerate.num  = 30;
    wfl_ctx->framerate.den  = 1;

    retcd = avcodec_parameters_from_context(stream->codecpar, wfl_ctx);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to copy decoder parameters!: %s", errstr);
        return -1;
    }

    return 0;
}

static int webu_mpegts_streams_audio(ctx_webui *webui)
{
    int retcd;
    AVCodecContext  *wfl_ctx, *enc_ctx;
    char errstr[128];
    AVDictionary *opts = NULL;
    const AVCodec *encoder;
    AVStream        *stream;

    webui->wfile.audio.codec_ctx = nullptr;
    encoder = avcodec_find_encoder(AV_CODEC_ID_AC3);

    webui->wfile.audio.strm = avformat_new_stream(webui->wfile.fmt_ctx, encoder);
    if (webui->wfile.audio.strm == nullptr) {
        LOG_MSG(ERR, NO_ERRNO, "Could not alloc audio stream");
        return -1;
    }
    stream = webui->wfile.audio.strm;
    webui->wfile.audio.index = webui->wfile.audio.strm->index;

    webui->wfile.audio.codec_ctx = avcodec_alloc_context3(encoder);
    if (webui->wfile.audio.codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not allocate audio context"
            , webui->chitm->ch_nbr.c_str());
        return -1;
    }

    enc_ctx =webui->chitm->ofile.audio.codec_ctx;
    wfl_ctx =webui->wfile.audio.codec_ctx;

    webui->wfile.fmt_ctx->audio_codec_id = AV_CODEC_ID_AC3;
    webui->wfile.fmt_ctx->audio_codec = avcodec_find_encoder(AV_CODEC_ID_AC3);
    stream->codecpar->bit_rate = enc_ctx->bit_rate;;
    stream->codecpar->frame_size = enc_ctx->frame_size;;
    av_channel_layout_copy(&stream->codecpar->ch_layout, &enc_ctx->ch_layout);
    stream->codecpar->codec_id=AV_CODEC_ID_AC3;
    stream->codecpar->format = enc_ctx->sample_fmt;
    stream->codecpar->sample_rate = enc_ctx->sample_rate;
    stream->time_base.den  = enc_ctx->sample_rate;
    stream->time_base.num = 1;

    wfl_ctx->bit_rate = enc_ctx->bit_rate;
    wfl_ctx->sample_fmt = enc_ctx->sample_fmt;
    wfl_ctx->sample_rate = enc_ctx->sample_rate;
    wfl_ctx->time_base = enc_ctx->time_base;
    av_channel_layout_copy(&wfl_ctx->ch_layout, &enc_ctx->ch_layout);
    wfl_ctx->frame_size = enc_ctx->frame_size;
    wfl_ctx->pkt_timebase = enc_ctx->pkt_timebase;

    retcd = avcodec_open2(wfl_ctx, encoder, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO,"Failed to open codec: %s", errstr);
        abort();
        return -1;
    }
    av_dict_free(&opts);

    return 0;
}

void webu_mpegts_open(ctx_webui *webui)
{
    int retcd;
    char errstr[128];
    unsigned char   *buf_image;
    AVDictionary    *opts;

    opts = NULL;
    webui->wfile.fmt_ctx = avformat_alloc_context();
    webui->wfile.fmt_ctx->oformat = av_guess_format("mpegts", NULL, NULL);

    pthread_mutex_lock(&webui->chitm->mtx_ifmt);
        if (webui->chitm->ofile.video.index != -1) {
            retcd = webu_mpegts_streams_video(webui);
            if (retcd < 0) {
                pthread_mutex_unlock(&webui->chitm->mtx_ifmt);
                return;
            }
        }
        if (webui->chitm->ofile.audio.index != -1) {
            retcd = webu_mpegts_streams_audio(webui);
            if (retcd < 0) {
                pthread_mutex_unlock(&webui->chitm->mtx_ifmt);
                return;
            }
        }
    pthread_mutex_unlock(&webui->chitm->mtx_ifmt);

    webui->resp_image  =(unsigned char*) mymalloc(40960);
    memset(webui->resp_image,'\0',4096);
    webui->resp_size = 4096;
    webui->resp_used = 0;

    webui->aviobuf_sz = 4096;
    buf_image = (unsigned char*)av_malloc(webui->aviobuf_sz);
    webui->wfile.fmt_ctx->pb = avio_alloc_context(
        buf_image, (int)webui->aviobuf_sz, 1, webui
        , NULL, &webu_mpegts_avio_buf, NULL);
    webui->wfile.fmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
    av_dict_set(&opts, "movflags", "empty_moov", 0);

    retcd = avformat_write_header(webui->wfile.fmt_ctx, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to write header!: %s", errstr);
        webu_mpegts_free_context(webui);
        return;
    }

    webui->stream_pos = 0;
    webui->resp_used = 0;

}

mhdrslt webu_mpegts_main(ctx_webui *webui)
{
    mhdrslt retcd;
    struct MHD_Response *response;
    std::list<ctx_params_item>::iterator    it;

    webu_mpegts_open(webui);

    clock_gettime(CLOCK_MONOTONIC, &webui->time_last);

    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN
        , 512, &webu_mpegts_response, webui, NULL);
    if (response == nullptr) {
        LOG_MSG(ERR, NO_ERRNO, "Invalid response");
        return MHD_NO;
    }

    for (it  = webui->app->webcontrol_headers.params_array.begin();
         it != webui->app->webcontrol_headers.params_array.end(); it++) {
        MHD_add_response_header (response
            , it->param_name.c_str(), it->param_value.c_str());
    }

    MHD_add_response_header(response, "Content-Transfer-Encoding", "BINARY");
    MHD_add_response_header(response, "Content-Type", "application/octet-stream");

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Increment the counters for the connections to the streams */
static void webu_stream_cnct_cnt(ctx_webui *webui)
{
    int indx;

    webui->chitm->cnct_cnt++;

    /* We set the last written to be behind the index of reader of the file
      so that the infile keeps processing and replacing the packets in the
      array ring until it reaches the lastwritten index.
    */
    indx = pktarray_get_index(webui->chitm);
    if (indx < 0) {
        indx = 0;
    }
    if (webui->chitm->pktarray_count != 0) {
        webui->chitm->pktarray[indx].iswritten = false;
    }
    indx = pktarray_indx_prev(indx);

    webui->chitm->pktarray_lastwritten = indx;

    if (webui->chitm->cnct_cnt == 1) {
        /* This is the first connection so we need to wait a bit
         * so that the loop on the other thread can update image
         */
        SLEEP(0,100000000L);
    }

}

/* Assign the type of stream that is being answered*/
static void webu_stream_type(ctx_webui *webui)
{
    if (webui->uri_cmd1 == "mpegts") {
        if (webui->uri_cmd2 == "stream") {
            webui->cnct_type = WEBUI_CNCT_TS_FULL;
        } else if (webui->uri_cmd2 == "") {
            webui->cnct_type = WEBUI_CNCT_TS_FULL;
        } else {
            webui->cnct_type = WEBUI_CNCT_UNKNOWN;
        }
    }
}

/* Determine whether the user specified a valid URL for the particular port */
static int webu_stream_checks(ctx_webui *webui)
{
    if (webui->channel_indx == -1) {
        LOG_MSG(ERR, NO_ERRNO
            , "Invalid channel specified: %s",webui->url.c_str());
        return -1;
    }
    if (webui->channel_indx < 0) {
        LOG_MSG(ERR, NO_ERRNO
                , "Invalid channel specified: %s",webui->url.c_str());
            return -1;
        }

        if ((webui->app->webcontrol_finish) ||
            (webui->chitm->ch_finish)) {
            return -1;
        }

    return 0;
}

/* Entry point for answering stream*/
mhdrslt webu_stream_main(ctx_webui *webui)
{
    mhdrslt retcd;

    if (webui->chitm == nullptr) {
        return MHD_NO;
    }

    webu_stream_type(webui);

    if (webu_stream_checks(webui) == -1) {
        return MHD_NO;
    }

    if (webui->uri_cmd1 == "mpegts") {
        webu_stream_cnct_cnt(webui);
        retcd = webu_mpegts_main(webui);
        return retcd;
    } else {
        return MHD_NO;
    }

}





/*
static int webu_mpegts_pic_send(ctx_webui *webui, unsigned char *img)
{
    int retcd;
    char errstr[128];
    struct timespec curr_ts;
    int64_t pts_interval;

    if (webui->picture == NULL) {
        webui->picture = myframe_alloc();
        webui->picture->linesize[0] = webui->cdcctx->width;
        webui->picture->linesize[1] = webui->cdcctx->width / 2;
        webui->picture->linesize[2] = webui->cdcctx->width / 2;

        webui->picture->format = webui->cdcctx->pix_fmt;
        webui->picture->width  = webui->cdcctx->width;
        webui->picture->height = webui->cdcctx->height;

        webui->picture->pict_type = AV_PICTURE_TYPE_I;
        webui->picture->key_frame = 1;
        webui->picture->pts = 1;
    }

    webui->picture->data[0] = img;
    webui->picture->data[1] = webui->picture->data[0] +
        (webui->cdcctx->width * webui->cdcctx->height);
    webui->picture->data[2] = webui->picture->data[1] +
        ((webui->cdcctx->width * webui->cdcctx->height) / 4);

    clock_gettime(CLOCK_REALTIME, &curr_ts);
    pts_interval = ((1000000L * (curr_ts.tv_sec - webui->start_time.tv_sec)) +
        (curr_ts.tv_nsec/1000) - (webui->start_time.tv_nsec/1000));
    webui->picture->pts = av_rescale_q(pts_interval
        ,(AVRational){1, 1000000L}, webui->cdcctx->time_base);

    retcd = avcodec_send_frame(webui->cdcctx, webui->picture);
    if (retcd < 0 ) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            , "Error sending frame for encoding:%s", errstr);
        myframe_free(webui->picture);
        webui->picture = NULL;
        return -1;
    }

    return 0;
}



    AVStream        *strm;
    const AVCodec   *codec;
    AVDictionary    *opts;
    ctx_chitm *chitm;
    AVCodecContext  *vidctx;
    AVCodecContext  *audctx;
    AVStream *vid_stream;
    AVStream *aud_stream;

    opts = NULL;
    webui->cdcctx = NULL;
    webui->ofmt_ctx = NULL;
    webui->stream_fps = 10000;
    clock_gettime(CLOCK_REALTIME, &webui->start_time);

    webui->ofmt_ctx = avformat_alloc_context();
    webui->ofmt_ctx->oformat = av_guess_format("mpegts", NULL, NULL);
    chitm = webui->chitm;
    vidctx = chitm->stream_ctx[chitm->video.index_dec].enc_ctx;
    audctx =chitm->stream_ctx[chitm->audio->index].enc_ctx;

    retcd = avcodec_parameters_from_context(vid_stream->codecpar, vidctx);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not copy parms from video decoder"
            , chitm->ch_nbr.c_str());
        return;
    }

    retcd = avcodec_parameters_to_context(webui->ofmt_ctx, vid_stream->codecpar);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not copy parms to video encoder"
            , chitm->ch_nbr.c_str());
        return;
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
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not open video encoder"
            , chitm->ch_nbr.c_str());
        LOG_MSG(NTC, NO_ERRNO
            , "%s: %dx%d %d %d"
            , chitm->ch_nbr.c_str()
            , enc_ctx->width, enc_ctx->height
            , enc_ctx->pix_fmt, enc_ctx->time_base.den);
        return;
    }

    if (chitm->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    out_stream->time_base = enc_ctx->time_base;

    retcd = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not copy parms from decoder"
            , chitm->ch_nbr.c_str());
        return;
    }

    webui->ofmt_ctx->video_codec_id = vidctx->codec_id;
    codec = avcodec_find_encoder(webui->ofmt_ctx->video_codec_id);
    strm = avformat_new_stream(webui->ofmt_ctx, codec);

    webui->cdcctx = avcodec_alloc_context3(codec);
    webui->cdcctx->gop_size      = vidctx->gop_size;
    webui->cdcctx->codec_id      = MY_CODEC_ID_H264;
    webui->cdcctx->codec_type    = AVMEDIA_TYPE_VIDEO;
    webui->cdcctx->bit_rate      = 400000;

    webui->cdcctx->width         = webui->cam->imgs.width;
    webui->cdcctx->height        = webui->cam->imgs.height;

    webui->cdcctx->time_base.num = 1;
    webui->cdcctx->time_base.den = 90000;
    webui->cdcctx->pix_fmt       = MY_PIX_FMT_YUV420P;
    webui->cdcctx->max_b_frames  = 1;
    webui->cdcctx->flags         |= MY_CODEC_FLAG_GLOBAL_HEADER;
    webui->cdcctx->framerate.num  = 1;
    webui->cdcctx->framerate.den  = 1;
    av_opt_set(webui->cdcctx->priv_data, "profile", "main", 0);
    av_opt_set(webui->cdcctx->priv_data, "crf", "22", 0);
    av_opt_set(webui->cdcctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(webui->cdcctx->priv_data, "preset", "superfast",0);
    av_dict_set(&opts, "movflags", "empty_moov", 0);

    retcd = avcodec_open2(webui->cdcctx, codec, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to copy decoder parameters!: %s", errstr);
        webu_mpegts_free_context(webui);
        av_dict_free(&opts);
        return -1;
    }

    retcd = avcodec_parameters_from_context(strm->codecpar, webui->cdcctx);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to copy decoder parameters!: %s", errstr);
        webu_mpegts_free_context(webui);
        av_dict_free(&opts);
        return -1;
    }




*/

/*
    retcd = avcodec_parameters_from_context(webui->strm_audio->codecpar, enc_ctx);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to copy encoder parameters!: %s", errstr);
        return -1;
    }

    retcd = avcodec_parameters_to_context(ctx_codec, enc_ctx);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to copy encoder parameters!: %s", errstr);
        return -1;
    }
    //wfl_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    //wfl_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    //av_dict_set(&opts, "strict", "experimental", 0);

*/

/*
    wfl_ctx->sample_fmt = enc_ctx->sample_fmt;
    wfl_ctx->sample_rate = enc_ctx->sample_rate;
    wfl_ctx->time_base = enc_ctx->time_base;
    wfl_ctx->bit_rate = enc_ctx->bit_rate;
    av_channel_layout_copy(&wfl_ctx->ch_layout, &enc_ctx->ch_layout);
    wfl_ctx->frame_size = enc_ctx->frame_size;
    //wfl_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    //wfl_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    wfl_ctx->pkt_timebase = enc_ctx->pkt_timebase;
    //av_dict_set(&opts, "strict", "experimental", 0);


    stream->codecpar->bit_rate = 448000;
    stream->codecpar->frame_size = 96000;
    av_channel_layout_copy(&stream->codecpar->ch_layout, &enc_ctx->ch_layout);
    stream->codecpar->codec_id=AV_CODEC_ID_AC3;
    stream->codecpar->format = enc_ctx->sample_fmt;
    stream->codecpar->sample_rate = enc_ctx->sample_rate;
    stream->time_base.den  = enc_ctx->sample_rate;
    stream->time_base.num = 1;
*/

/*
    retcd = avcodec_parameters_from_context(webui->wfile.audio.strm->codecpar, wfl_ctx);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to copy encoder parameters!: %s", errstr);
        return -1;
    }
*/

/*
Sleep required time to get to the user requested framerate for the stream
void webu_stream_delay(ctx_webui *webui)
{
    long   stream_rate;
    struct timespec time_curr;
    long   stream_delay;

    if ((webui->app->webcontrol_finish) ||
        (webui->chitm->ch_finish)) {
        webui->resp_used = 0;
        return;
    }

return;

    clock_gettime(CLOCK_MONOTONIC, &time_curr);

    // The stream rate MUST be less than 1000000000 otherwise undefined behaviour
    // will occur with the SLEEP function.
    //
    stream_delay = ((time_curr.tv_nsec - webui->time_last.tv_nsec)) +
        ((time_curr.tv_sec - webui->time_last.tv_sec)*1000000000);
    if (stream_delay < 0)  {
        stream_delay = 0;
    }
    if (stream_delay > 1000000000 ) {
        stream_delay = 1000000000;
    }

    if (webui->stream_fps <= -1) {
        stream_rate = ( (1000000000 / webui->stream_fps) - stream_delay);
        if ((stream_rate > 0) && (stream_rate < 1000000000)) {
            SLEEP(0,stream_rate);
        } else if (stream_rate == 1000000000) {
            SLEEP(1,0);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &webui->time_last);

}
*/

/*
static void webu_mpegts_getimg_old(ctx_webui *webui)
{
    int64_t idnbr_image, idnbr_lastwritten, idnbr_stop, idnbr_firstkey;
    int indx, indx_lastwritten, indx_firstkey, indx_video;

    if ((webui->app->webcontrol_finish) || (webui->chitm->ch_finish)) {
        webu_mpegts_resetpos(webui);
        return;
    }

    pthread_mutex_lock(&webui->chitm->mtx_pktarray);
        if (webui->chitm->pktarray_count == 0) {
            pthread_mutex_unlock(&webui->chitm->mtx_pktarray);
            return;
        }

        indx = webui->chitm->pktarray_index;
        idnbr_image = webui->chitm->pktarray[indx].idnbr;
        idnbr_lastwritten = 0;
        idnbr_firstkey = idnbr_image;
        idnbr_stop = 0;
        indx_lastwritten = -1;
        indx_firstkey = -1;
        indx_video = webui->chitm->ofile.video.index;

        for(indx = 0; indx < webui->chitm->pktarray_count; indx++) {
            if ((webui->chitm->pktarray[indx].iswritten) &&
                (webui->chitm->pktarray[indx].idnbr > idnbr_lastwritten) &&
                (webui->chitm->pktarray[indx].packet->stream_index == indx_video)) {
                idnbr_lastwritten=webui->chitm->pktarray[indx].idnbr;
                indx_lastwritten = indx;
            }
            if ((webui->chitm->pktarray[indx].idnbr >  idnbr_stop) &&
                (webui->chitm->pktarray[indx].idnbr <= idnbr_image)&&
                (webui->chitm->pktarray[indx].packet->stream_index == indx_video)) {
                idnbr_stop=webui->chitm->pktarray[indx].idnbr;
            }
            if ((webui->chitm->pktarray[indx].iskey) &&
                (webui->chitm->pktarray[indx].idnbr <= idnbr_firstkey)&&
                (webui->chitm->pktarray[indx].packet->stream_index == indx_video)) {
                    idnbr_firstkey=webui->chitm->pktarray[indx].idnbr;
                    indx_firstkey = indx;
            }
        }

        if (idnbr_stop == 0) {
            pthread_mutex_unlock(&webui->chitm->mtx_pktarray);
            return;
        }

        if (indx_lastwritten != -1) {
            indx = indx_lastwritten;
        } else if (indx_firstkey != -1) {
            indx = indx_firstkey;
        } else {
            indx = 0;
        }

        while (true){
            if ((webui->chitm->pktarray[indx].iswritten == false) &&
                (webui->chitm->pktarray[indx].packet->size > 0) &&
                (webui->chitm->pktarray[indx].idnbr >  idnbr_lastwritten) &&
                (webui->chitm->pktarray[indx].idnbr <= idnbr_image)) {
                webu_mpegts_packet_write(webui, indx);
            }
            if (webui->chitm->pktarray[indx].idnbr == idnbr_stop) {
                break;
            }
            indx++;
            if (indx == webui->chitm->pktarray_count ) {
                indx = 0;
            }
        }
    pthread_mutex_unlock(&webui->chitm->mtx_pktarray);

}
*/
/*
static void webu_mpegts_packet_minpts(ctx_webui *webui)
{
    int indx, indx_audio, indx_video;
    int64_t vbase, abase;

    // Note that the packets in the array are using the chitm->ofile
    //  index values of streams which could be different than webui->wfile
    //
    pthread_mutex_lock(&webui->chitm->mtx_pktarray);
        indx_audio = webui->chitm->ofile.audio.index;
        indx_video = webui->chitm->ofile.video.index;

        for (indx = 0; indx < webui->chitm->pktarray_count; indx++) {
            if (webui->chitm->pktarray[indx].packet == nullptr) {
                break;
            }
            if ((webui->chitm->pktarray[indx].packet->stream_index == indx_audio) &&
                (webui->chitm->pktarray[indx].packet->pts != AV_NOPTS_VALUE)) {
                abase = webui->chitm->pktarray[indx].packet->pts;
            };
            if ((webui->chitm->pktarray[indx].packet->stream_index == indx_video) &&
                (webui->chitm->pktarray[indx].packet->pts != AV_NOPTS_VALUE)) {
                vbase = webui->chitm->pktarray[indx].packet->pts;
            };
        }
        for (indx = 0; indx < webui->chitm->pktarray_count; indx++) {
            if (webui->chitm->pktarray[indx].packet == nullptr) {
                break;
            }
            if ((webui->chitm->pktarray[indx].packet->stream_index == indx_audio) &&
                (webui->chitm->pktarray[indx].packet->pts != AV_NOPTS_VALUE) &&
                (webui->chitm->pktarray[indx].packet->pts < abase)) {
                abase = webui->chitm->pktarray[indx].packet->pts;
            };
            if ((webui->chitm->pktarray[indx].packet->stream_index == indx_audio) &&
                (webui->chitm->pktarray[indx].packet->pts != AV_NOPTS_VALUE) &&
                (webui->chitm->pktarray[indx].packet->dts < abase)) {
                abase = webui->chitm->pktarray[indx].packet->dts;
            };
            if ((webui->chitm->pktarray[indx].packet->stream_index == indx_video) &&
                (webui->chitm->pktarray[indx].packet->pts != AV_NOPTS_VALUE) &&
                (webui->chitm->pktarray[indx].packet->pts < vbase)) {
                vbase = webui->chitm->pktarray[indx].packet->pts;
            };
            if ((webui->chitm->pktarray[indx].packet->stream_index == indx_video) &&
                (webui->chitm->pktarray[indx].packet->pts != AV_NOPTS_VALUE) &&
                (webui->chitm->pktarray[indx].packet->dts < vbase)) {
                vbase = webui->chitm->pktarray[indx].packet->dts;
            };
        }
    pthread_mutex_unlock(&webui->chitm->mtx_pktarray);

    if (abase < 0) {
        abase = 0;
    }

    if (vbase < 0) {
        vbase = 0;
    }

    webui->wfile.audio.base_pdts = abase;
    webui->wfile.video.base_pdts = vbase;
}

*/