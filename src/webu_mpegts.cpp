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

    LOG_MSG(DBG, NO_ERRNO, "Ch%s: Completed"
        , webui->chitm->ch_nbr.c_str());
}

static void webu_mpegts_resetpos(ctx_webui *webui)
{
    webui->stream_pos = 0;
    webui->resp_used = 0;
}

static void webu_mpegts_packet_wait(ctx_webui *webui)
{
    int64_t tm_diff, pts_diff, tot_diff;
    int64_t sec_full, sec_msec;

    if (webui->app->webcontrol_finish == true) {
        return;
    }

    if (webui->pkt->pts == AV_NOPTS_VALUE) {
        return;
    }


    return;

    SLEEP(0, webui->msec_cnt * 1000);
    return;

    /* How much time the pts wants us to be at since the start of the movie */
    if (webui->pkt->stream_index == webui->wfile.audio.index) {
        pts_diff = av_rescale_q(
            webui->pkt->pts - webui->wfile.audio.start_pts
            , AVRational{1,1}
            ,webui->wfile.audio.strm->time_base);
    } else {
        pts_diff = av_rescale_q(
            webui->pkt->pts - webui->wfile.video.start_pts
            , AVRational{1,1}
            ,webui->wfile.video.strm->time_base);
    }

    /* How much time has really elapsed since the start of movie*/
    tm_diff = av_gettime_relative() - webui->wfile.time_start;

    /* How much time we need to wait to get in sync*/
    tot_diff = pts_diff - tm_diff;

    if (tot_diff > 0) {
        sec_full = int(tot_diff / 1000000L);
        sec_msec = (tot_diff % 1000000L);
        if (sec_full < 100){
            SLEEP(sec_full, sec_msec * 1000);
        } else {
            if (webui->pkt->stream_index ==webui->wfile.audio.index) {
                LOG_MSG(INF, NO_ERRNO, "sync index %d last %d pktpts %d base %d "
                    , webui->pkt->stream_index
                    , webui->wfile.audio.last_pts
                    , webui->pkt->pts
                    , webui->wfile.audio.base_pdts
                );
            } else {
                LOG_MSG(INF, NO_ERRNO, "sync index %d last %d pktpts %d base %d"
                    , webui->pkt->stream_index
                    , webui->wfile.video.last_pts
                    , webui->pkt->pts
                    , webui->wfile.video.base_pdts
                );
            }
        }
    }
}

static void webu_mpegts_packet_pts(ctx_webui *webui)
{
    int64_t ts_interval, base_pdts, last_pts;
    int64_t strm_st_pts, src_pts, temp_pts;
    AVRational tmpdst;

    src_pts = webui->pkt->pts;
    if (webui->wfile.time_start == -1) {
        webui->wfile.time_start = av_gettime_relative();
        webui->file_cnt = webui->pkt_file_cnt;
    }
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


    if (webui->pkt->stream_index == webui->wfile.audio.index) {
        if (webui->wfile.audio.start_pts == -1 ) {
            webui->wfile.audio.start_pts = av_rescale_q(
                src_pts - webui->pkt_start_pts
                , webui->pkt_timebase
                , webui->wfile.audio.strm->time_base);
            if (webui->wfile.audio.start_pts <= 0) {
                webui->wfile.audio.start_pts = 1;
            }
            if (webui->wfile.video.start_pts != -1 ) {
                temp_pts = av_rescale_q(
                    webui->wfile.video.start_pts
                    , webui->wfile.video.strm->time_base
                    , webui->wfile.audio.strm->time_base);
                if (temp_pts < webui->wfile.audio.start_pts) {
                    webui->wfile.audio.start_pts = temp_pts;
                } else {
                    webui->wfile.video.start_pts = av_rescale_q(
                        webui->wfile.audio.start_pts
                        , webui->wfile.audio.strm->time_base
                        , webui->wfile.video.strm->time_base);
                }
            }
            webui->wfile.audio.last_pts = 0;
            webui->wfile.audio.base_pdts = 0;
        }
        tmpdst = webui->wfile.audio.strm->time_base;
        last_pts = webui->wfile.audio.last_pts;
        base_pdts = webui->wfile.audio.base_pdts;
        strm_st_pts = webui->wfile.audio.start_pts;
    } else {
        if (webui->wfile.video.start_pts == -1 ) {
            webui->wfile.video.start_pts = av_rescale_q(
                src_pts - webui->pkt_start_pts
                , webui->pkt_timebase
                , webui->wfile.video.strm->time_base);
            if (webui->wfile.video.start_pts <= 0) {
                webui->wfile.video.start_pts = 1;
            }
            if (webui->wfile.audio.start_pts != -1 ) {
                temp_pts = av_rescale_q(
                    webui->wfile.audio.start_pts
                    , webui->wfile.audio.strm->time_base
                    , webui->wfile.video.strm->time_base);
                if (temp_pts < webui->wfile.video.start_pts) {
                    webui->wfile.video.start_pts = temp_pts;
                } else {
                    webui->wfile.audio.start_pts = av_rescale_q(
                        webui->wfile.video.start_pts
                        , webui->wfile.video.strm->time_base
                        , webui->wfile.audio.strm->time_base);
                }
            }
            webui->wfile.video.last_pts = 0;
            webui->wfile.video.base_pdts = 0;
        }
        tmpdst = webui->wfile.video.strm->time_base;
        last_pts = webui->wfile.video.last_pts;
        base_pdts = webui->wfile.video.base_pdts;
        strm_st_pts = webui->wfile.video.start_pts;
    }


    if (webui->pkt->pts != AV_NOPTS_VALUE) {
        if (webui->file_cnt == webui->pkt_file_cnt) {
            webui->pkt->pts = av_rescale_q(webui->pkt->pts - webui->pkt_start_pts
                ,webui->pkt_timebase, tmpdst) - strm_st_pts + base_pdts;
        } else {
            webui->file_cnt = webui->pkt_file_cnt;
            base_pdts = last_pts + strm_st_pts;
            webui->pkt->pts = av_rescale_q(webui->pkt->pts - webui->pkt_start_pts
                ,webui->pkt_timebase, tmpdst) - strm_st_pts + base_pdts;
            if (webui->pkt->pts == last_pts) {
                webui->pkt->pts++;
            }
            if (webui->pkt->stream_index == webui->wfile.audio.index) {
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
        if (webui->pkt->pts <= 0) {
            if (webui->pkt->pts < 0) {
                LOG_MSG(DBG, NO_ERRNO
                    ,"Skipping %d",webui->pkt->pts);

                //abort();
                return;
            }
            webui->pkt->pts = 1;
        }
    }

    if (webui->pkt->dts != AV_NOPTS_VALUE) {
        if (webui->file_cnt == webui->pkt_file_cnt) {
            webui->pkt->dts = av_rescale_q(webui->pkt->dts - webui->pkt_start_pts
                ,webui->pkt_timebase, tmpdst) - strm_st_pts + base_pdts;
        } else {
            webui->file_cnt = webui->pkt_file_cnt;
            base_pdts = last_pts + strm_st_pts;
            webui->pkt->dts = av_rescale_q(webui->pkt->dts - webui->pkt_start_pts
                , webui->pkt_timebase, tmpdst) - strm_st_pts + base_pdts;
            if (webui->pkt->dts == last_pts) {
                webui->pkt->dts++;
            }
            if (webui->pkt->stream_index == webui->wfile.audio.index) {
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
        if (webui->pkt->dts <= 0) {
            webui->pkt->dts = 1;
        }
    }

    ts_interval = webui->pkt->duration;
    webui->pkt->duration = av_rescale_q(ts_interval,webui->pkt_timebase, tmpdst);
    bool chk;
    if (webui->pkt->flags & AV_PKT_FLAG_KEY) {
        chk = true;
    } else {
        chk = false;
    }
    if (chk) {
        chk = false;
    }
/*
    LOG_MSG(DBG, NO_ERRNO
        ,"data %d %d src %d  %d/%d newpts %d %d tb-a %d/%d %d %d tb-v %d/%d %d %d"
        , webui->pkt->stream_index
        , webui->file_cnt
        , src_pts
        , webui->pkt_timebase.num
        , webui->pkt_timebase.den
        , webui->pkt->pts
        , chk
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
static void webu_mpegts_packet_write(ctx_webui *webui)
{
    int retcd;
    char errstr[128];

    if (webui->app->webcontrol_finish == true) {
        return;
    }

    if ((webui->wfile.audio.index != webui->chitm->ofile.audio.index) ||
        (webui->wfile.video.index != webui->chitm->ofile.video.index)) {
        if (webui->pkt->stream_index == webui->chitm->ofile.audio.index) {
            LOG_MSG(DBG, NO_ERRNO,"Swapping audio");
            webui->pkt->stream_index = webui->wfile.audio.index;
        } else if (webui->pkt->stream_index == webui->chitm->ofile.video.index) {
            LOG_MSG(DBG, NO_ERRNO,"Swapping video");
            webui->pkt->stream_index = webui->wfile.video.index;
        }
    }

    webu_mpegts_packet_pts(webui);

    if (webui->pkt->stream_index == webui->wfile.audio.index) {
        if (webui->pkt->pts > webui->wfile.audio.last_pts) {
            webui->wfile.audio.last_pts = webui->pkt->pts;
        } else {
            return;
        }
    } else {
        if (webui->pkt->pts > webui->wfile.video.last_pts) {
            webui->wfile.video.last_pts = webui->pkt->pts;
        } else {
            return;
        }
    }

    webu_mpegts_packet_wait(webui);
/*
    LOG_MSG(NTC, NO_ERRNO
        ,"%s: Writing frame index %d id %d"
        , webui->chitm->ch_nbr.c_str()
        , webui->pkt_index, webui->pkt_idnbr);
*/
    if ((webui->start_cnt == 1) &&
        (webui->pkt_key == false) &&
        (webui->pkt->stream_index = webui->chitm->ofile.video.index)) {
        return;
    }
    webui->start_cnt = 0;

    retcd = av_interleaved_write_frame(webui->wfile.fmt_ctx, webui->pkt);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Error writing frame index %d id %d err %s"
            , webui->pkt_index, webui->pkt_idnbr, errstr);
    }

}

static void webu_mpegts_pkt_copy(ctx_webui *webui, int indx)
{
    int retcd;
    char errstr[128];

    retcd = mycopy_packet(webui->pkt, webui->chitm->pktarray[indx].packet);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(INF, NO_ERRNO, "av_copy_packet: %s",errstr);
        mypacket_free(webui->pkt);
        webui->pkt = nullptr;
        return;
    }
    webui->pkt_index     = indx;
    webui->pkt_idnbr     = webui->chitm->pktarray[indx].idnbr;
    webui->pkt_start_pts = webui->chitm->pktarray[indx].start_pts;
    webui->pkt_timebase  = webui->chitm->pktarray[indx].timebase;
    webui->pkt_file_cnt  = webui->chitm->pktarray[indx].file_cnt;
    webui->pkt_key       = webui->chitm->pktarray[indx].iskey;
}

static bool webu_mpegts_pkt_get(ctx_webui *webui, int indx)
{
    bool pktready;
    pthread_mutex_lock(&webui->chitm->mtx_pktarray);
        if ((webui->chitm->pktarray[indx].packet != nullptr) &&
            (webui->chitm->pktarray[indx].idnbr > webui->pkt_idnbr) ) {
            webu_mpegts_pkt_copy(webui, indx);
            if (webui->pkt == nullptr) {
                pktready = false;
            } else {
                pktready = true;
            }
        } else {
            pktready = false;
        }
    pthread_mutex_unlock(&webui->chitm->mtx_pktarray);
    return pktready;
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

    indx_next = webui->pkt_index;
    indx_next = pktarray_indx_next(indx_next);

    webui->pkt = mypacket_alloc(webui->pkt);

    pktready = webu_mpegts_pkt_get(webui, indx_next);

    chk = 0;
    while (
        (chk < 1000000) &&
        (pktready == false) &&
        (webui->app->webcontrol_finish == false)) {

        SLEEP(0, webui->msec_cnt * 1000);
        pktready = webu_mpegts_pkt_get(webui, indx_next);
        chk++;
    }

    if (chk == 1000000) {
        LOG_MSG(INF, NO_ERRNO,"Excessive wait for new packet");
        webui->msec_cnt++;
    } else {
        webu_mpegts_packet_write(webui);
    }
    mypacket_free(webui->pkt);
    webui->pkt = nullptr;
}

static int webu_mpegts_avio_buf(void *opaque, uint8_t *buf, int buf_size)
{
    ctx_webui *webui =(ctx_webui *)opaque;

    if (webui->resp_size < (size_t)(buf_size + webui->resp_used)) {
        webui->resp_size = (size_t)(buf_size + webui->resp_used);
        webui->resp_image = (unsigned char*)realloc(
            webui->resp_image, webui->resp_size);
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

    if (webui->app->webcontrol_finish == true) {
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
    return sent_bytes;
}

static int webu_mpegts_streams_video_h264(ctx_webui *webui)
{
    int retcd;
    char errstr[128];
    AVCodecContext  *wfl_ctx, *enc_ctx;
    AVDictionary    *opts;
    const AVCodec   *encoder;
    AVStream        *stream;

    opts = NULL;
    webui->wfile.fmt_ctx->video_codec_id = AV_CODEC_ID_H264;
    webui->wfile.fmt_ctx->video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    encoder = webui->wfile.fmt_ctx->video_codec;

    webui->wfile.video.strm = avformat_new_stream(
        webui->wfile.fmt_ctx, encoder);
    if (webui->wfile.video.strm == nullptr) {
        LOG_MSG(ERR, NO_ERRNO, "Could not alloc video stream");
        webu_mpegts_free_context(webui);
        return -1;
    }
    webui->wfile.video.index = webui->wfile.video.strm->index;
    stream = webui->wfile.video.strm;

    webui->wfile.video.codec_ctx = avcodec_alloc_context3(encoder);
    if (webui->wfile.video.codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not allocate video context"
            , webui->chitm->ch_nbr.c_str());
        webu_mpegts_free_context(webui);
        return -1;
    }

    enc_ctx = webui->chitm->ofile.video.codec_ctx;
    wfl_ctx = webui->wfile.video.codec_ctx;

    wfl_ctx->gop_size      = enc_ctx->gop_size;
    wfl_ctx->codec_id      = enc_ctx->codec_id;
    wfl_ctx->codec_type    = enc_ctx->codec_type;
    wfl_ctx->bit_rate      = enc_ctx->bit_rate;
    wfl_ctx->width         = enc_ctx->width;
    wfl_ctx->height        = enc_ctx->height;
    wfl_ctx->time_base     = enc_ctx->time_base;
    wfl_ctx->pix_fmt       = enc_ctx->pix_fmt;
    wfl_ctx->max_b_frames  = enc_ctx->max_b_frames;
    wfl_ctx->framerate     = enc_ctx->framerate;
    wfl_ctx->keyint_min    = enc_ctx->keyint_min;

    av_dict_set( &opts, "profile", "baseline", 0 );
    av_dict_set( &opts, "crf", "17", 0 );
    av_dict_set( &opts, "tune", "zerolatency", 0 );
    av_dict_set( &opts, "preset", "superfast", 0 );
    av_dict_set( &opts, "keyint", "5", 0 );
    av_dict_set( &opts, "scenecut", "200", 0 );

    if (webui->wfile.fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    retcd = avcodec_open2(wfl_ctx, encoder, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO,"Failed to open codec: %s", errstr);
        av_dict_free(&opts);
        webu_mpegts_free_context(webui);
        return -1;
    }
    av_dict_free(&opts);

    retcd = avcodec_parameters_from_context(stream->codecpar, wfl_ctx);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to copy decoder parameters!: %s", errstr);
        webu_mpegts_free_context(webui);
        return -1;
    }
    stream->time_base = webui->chitm->ofile.video.strm->time_base;

    return 0;
}

static int webu_mpegts_streams_video_mpeg(ctx_webui *webui)
{
    int retcd;
    char errstr[128];
    AVCodecContext  *wfl_ctx, *enc_ctx;
    AVDictionary    *opts;
    const AVCodec   *encoder;
    AVStream        *stream;

    opts = NULL;
    webui->wfile.fmt_ctx->video_codec_id = AV_CODEC_ID_MPEG2VIDEO;
    webui->wfile.fmt_ctx->video_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);

    encoder = webui->wfile.fmt_ctx->video_codec;

    webui->wfile.video.strm = avformat_new_stream(webui->wfile.fmt_ctx, encoder);
    if (webui->wfile.video.strm == nullptr) {
        LOG_MSG(ERR, NO_ERRNO, "Could not alloc video stream");
        webu_mpegts_free_context(webui);
        return -1;
    }
    webui->wfile.video.index = webui->wfile.video.strm->index;
    stream = webui->wfile.video.strm;

    webui->wfile.video.codec_ctx = avcodec_alloc_context3(encoder);
    if (webui->wfile.video.codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not allocate video context"
            , webui->chitm->ch_nbr.c_str());
        webu_mpegts_free_context(webui);
        return -1;
    }

    enc_ctx = webui->chitm->ofile.video.codec_ctx;
    wfl_ctx = webui->wfile.video.codec_ctx;

    wfl_ctx->codec_id      = enc_ctx->codec_id;
    wfl_ctx->codec_type    = enc_ctx->codec_type;
    wfl_ctx->width         = enc_ctx->width;
    wfl_ctx->height        = enc_ctx->height;
    wfl_ctx->time_base     = enc_ctx->time_base;
    wfl_ctx->max_b_frames  = enc_ctx->max_b_frames;
    wfl_ctx->framerate     = enc_ctx->framerate;
    wfl_ctx->bit_rate      = enc_ctx->bit_rate;
    wfl_ctx->gop_size      = enc_ctx->gop_size;
    wfl_ctx->pix_fmt       = enc_ctx->pix_fmt;
    wfl_ctx->sw_pix_fmt    = enc_ctx->sw_pix_fmt;
    if (webui->wfile.fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        wfl_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    retcd = avcodec_open2(wfl_ctx, encoder, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not open video encoder: %s %dx%d %d %d %d/%d"
            , webui->chitm->ch_nbr.c_str(), errstr
            , wfl_ctx->width, wfl_ctx->height
            , wfl_ctx->pix_fmt, wfl_ctx->time_base.den
            , wfl_ctx->framerate.num, wfl_ctx->framerate.den);
        av_dict_free(&opts);
        webu_mpegts_free_context(webui);
        abort();
        return -1;
    }
    av_dict_free(&opts);

    retcd = avcodec_parameters_from_context(stream->codecpar, wfl_ctx);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(ERR, NO_ERRNO
            ,"Failed to copy decoder parameters!: %s", errstr);
        webu_mpegts_free_context(webui);
        return -1;
    }
    stream->time_base = webui->chitm->ofile.video.strm->time_base;
    stream->r_frame_rate = enc_ctx->framerate;
    stream->avg_frame_rate= enc_ctx->framerate;

/*
    LOG_MSG(NTC, NO_ERRNO
        , "%s: Opened video encoder: %dx%d %d %d %d/%d %d/%d %d/%d"
        , webui->chitm->ch_nbr.c_str()
        , wfl_ctx->width, wfl_ctx->height
        , wfl_ctx->pix_fmt, wfl_ctx->time_base.den
        , wfl_ctx->framerate.num, wfl_ctx->framerate.den
        , webui->wfile.video.strm->avg_frame_rate.num,webui->wfile.video.strm->avg_frame_rate.den
        , webui->wfile.video.strm->r_frame_rate.num, webui->wfile.video.strm->r_frame_rate.den
        );
*/
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
        webu_mpegts_free_context(webui);
        return -1;
    }
    stream = webui->wfile.audio.strm;
    webui->wfile.audio.index = webui->wfile.audio.strm->index;

    webui->wfile.audio.codec_ctx = avcodec_alloc_context3(encoder);
    if (webui->wfile.audio.codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Could not allocate audio context"
            , webui->chitm->ch_nbr.c_str());
        webu_mpegts_free_context(webui);
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
        webu_mpegts_free_context(webui);
        abort();
        return -1;
    }
    av_dict_free(&opts);

    return 0;
}

static int webu_mpegts_open(ctx_webui *webui)
{
    int retcd, indx_curr;
    char errstr[128];
    unsigned char   *buf_image;
    AVDictionary    *opts;

    if (webui->app->webcontrol_finish == true) {
        return -1;
    }

    opts = NULL;
    webui->wfile.fmt_ctx = avformat_alloc_context();
    webui->wfile.fmt_ctx->oformat = av_guess_format("mpegts", NULL, NULL);

    pthread_mutex_lock(&webui->chitm->mtx_ifmt);
        if (webui->chitm->ofile.video.index != -1) {
            if (webui->chitm->ch_encode == "h264") {
                retcd = webu_mpegts_streams_video_h264(webui);
            } else {
                retcd = webu_mpegts_streams_video_mpeg(webui);
            }
            if (retcd < 0) {
                pthread_mutex_unlock(&webui->chitm->mtx_ifmt);
                webu_mpegts_free_context(webui);
                return -1;
            }
        }
        if (webui->chitm->ofile.audio.index != -1) {
            retcd = webu_mpegts_streams_audio(webui);
            if (retcd < 0) {
                pthread_mutex_unlock(&webui->chitm->mtx_ifmt);
                webu_mpegts_free_context(webui);
                return -1;
            }
        }
    pthread_mutex_unlock(&webui->chitm->mtx_ifmt);

    webui->resp_image  =(unsigned char*) mymalloc(WEBUI_LEN_RESP * 10);
    memset(webui->resp_image,'\0',WEBUI_LEN_RESP);
    webui->resp_size = WEBUI_LEN_RESP;
    webui->resp_used = 0;

    webui->aviobuf_sz = WEBUI_LEN_RESP;
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
        return -1;
    }
    av_dict_free(&opts);

    webui->stream_pos = 0;
    webui->resp_used = 0;

    indx_curr = pktarray_get_index(webui->chitm);
    if (indx_curr < 0) {
        indx_curr = 0;
    }

    webui->pkt_index = (int)(webui->chitm->pktarray_count / 2) + indx_curr;
    if (webui->pkt_index > webui->chitm->pktarray_count) {
        webui->pkt_index -= webui->chitm->pktarray_count;
    }
    webui->pkt_idnbr = 1;
    webui->start_cnt = 1;

    LOG_MSG(NTC, NO_ERRNO
        , "%s: Setting start %d %d"
        , webui->chitm->ch_nbr.c_str()
        , webui->pkt_index, indx_curr);

    return 0;
}

mhdrslt webu_mpegts_main(ctx_webui *webui)
{
    mhdrslt retcd;
    struct MHD_Response *response;
    std::list<ctx_params_item>::iterator    it;

    if (webui->app->webcontrol_finish == true) {
        return MHD_NO;
    }

    if (webu_mpegts_open(webui) == -1) {
        return MHD_NO;
    }

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
    int indx, chk;

    if (webui->chitm->cnct_cnt == 0) {
        for (indx=0; indx < (int)webui->chitm->pktarray.size(); indx++) {
            if (webui->chitm->pktarray[indx].packet != nullptr) {
                mypacket_free(webui->chitm->pktarray[indx].packet);
                webui->chitm->pktarray[indx].packet = nullptr;
            }
        }
        webui->chitm->cnct_cnt++;
        webui->chitm->pktarray_start = webui->chitm->pktarray_count;
        chk = 0;
        while ((webui->chitm->pktarray_start > 0) && (chk <100000)) {
            SLEEP(0,10000L);
            chk++;
        }
    } else {
        webui->chitm->cnct_cnt++;
    }
}

/* Assign the type of stream that is being answered*/
static int webu_stream_type(ctx_webui *webui)
{
    if (webui->uri_cmd1 == "mpegts") {
        if (webui->uri_cmd2 == "stream") {
            webui->cnct_type = WEBUI_CNCT_TS_FULL;
        } else if (webui->uri_cmd2 == "") {
            webui->cnct_type = WEBUI_CNCT_TS_FULL;
        } else {
            webui->cnct_type = WEBUI_CNCT_UNKNOWN;
            return -1;
        }
    } else {
        webui->cnct_type = WEBUI_CNCT_UNKNOWN;
        return -1;
    }
    return 0;
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

    if (webui->app->webcontrol_finish == true) {
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

    if (webu_stream_type(webui) == -1) {
        return MHD_NO;
    }

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

