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
    while ((chk < 1000) && (pktready == false) && (finish == false)) {
        SLEEP(0, 25000000L);
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

    enc_ctx =webui->chitm->ifile.audio.codec_ctx;
    wfl_ctx =webui->wfile.audio.codec_ctx;

    webui->wfile.fmt_ctx->audio_codec_id = AV_CODEC_ID_AC3;
    webui->wfile.fmt_ctx->audio_codec = avcodec_find_encoder(AV_CODEC_ID_AC3);
    stream->codecpar->codec_id=AV_CODEC_ID_AC3;
    stream->codecpar->bit_rate = enc_ctx->bit_rate;;
    stream->codecpar->frame_size = enc_ctx->frame_size;;
    av_channel_layout_default(&stream->codecpar->ch_layout
        , enc_ctx->ch_layout.nb_channels);

    stream->codecpar->format = enc_ctx->sample_fmt;
    stream->codecpar->sample_rate = enc_ctx->sample_rate;
    stream->time_base.den  = enc_ctx->sample_rate;
    stream->time_base.num = 1;

    wfl_ctx->bit_rate = enc_ctx->bit_rate;
    wfl_ctx->sample_fmt = enc_ctx->sample_fmt;
    wfl_ctx->sample_rate = enc_ctx->sample_rate;
    wfl_ctx->time_base = enc_ctx->time_base;
    av_channel_layout_default(&wfl_ctx->ch_layout
        , enc_ctx->ch_layout.nb_channels);
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
        av_dict_free(&opts);
        return;
    }
    av_dict_free(&opts);

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

    //if (webui->chitm->cnct_cnt == 1) {
        /* This is the first connection so we need to wait a bit
         * so that the loop on the other thread can update image
         */
    //    SLEEP(0,100000000L);
    //}

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

