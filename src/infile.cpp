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
#include "util.hpp"
#include "logger.hpp"
#include "guide.hpp"
#include "infile.hpp"
#include "webu.hpp"

static int decoder_init_video(ctx_channel_item *chitm)
{
    int retcd;
    AVStream *stream;
    const AVCodec *dec;
    AVCodecContext *codec_ctx;

    chitm->ifile.video.codec_ctx = nullptr;
    stream = chitm->ifile.fmt_ctx->streams[chitm->ifile.video.index];
    chitm->ifile.video.strm = stream;
    dec = avcodec_find_decoder(stream->codecpar->codec_id);

    if (dec == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed to find video decoder for input stream"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(dec);
    if (codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed to allocate video decoder for input stream"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    retcd = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed to copy decoder parameters for stream"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    codec_ctx->framerate = av_guess_frame_rate(chitm->ifile.fmt_ctx, stream, NULL);
    codec_ctx->pkt_timebase = stream->time_base;
    codec_ctx->gop_size = 2;
    codec_ctx->keyint_min = 5;
    av_opt_set(codec_ctx->priv_data, "profile", "main", 0);
    av_opt_set(codec_ctx->priv_data, "crf", "22", 0);
    av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(codec_ctx->priv_data, "preset", "superfast",0);
    av_opt_set(codec_ctx->priv_data, "keyint", "5",0);

    retcd = avcodec_open2(codec_ctx, dec, NULL);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed to open video codec for input stream "
            , chitm->ch_nbr.c_str());
        return -1;
    }

    chitm->ifile.video.codec_ctx = codec_ctx;
    return 0;
}

static int decoder_init_audio(ctx_channel_item *chitm)
{
    int retcd;
    AVStream *stream;
    const AVCodec *dec;
    AVCodecContext *codec_ctx;

    chitm->ifile.audio.codec_ctx = nullptr;
    stream = chitm->ifile.fmt_ctx->streams[chitm->ifile.audio.index];
    chitm->ifile.audio.strm = stream;
    dec = avcodec_find_decoder(stream->codecpar->codec_id);

    if (dec == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "%s: Failed to find decoder for stream"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(dec);
    if (codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed to allocate decoder for stream "
            , chitm->ch_nbr.c_str());
        return -1;
    }

    retcd = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed to copy decoder parameters for stream "
            , chitm->ch_nbr.c_str());
        return -1;
    }
    codec_ctx->pkt_timebase = stream->time_base;

    retcd = avcodec_open2(codec_ctx, dec, NULL);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed to open decoder for stream "
            , chitm->ch_nbr.c_str());
        return -1;
    }

    chitm->ifile.audio.codec_ctx = codec_ctx;

    return 0;
}

int decoder_init(ctx_channel_item *chitm)
{
    int retcd, indx;
    char errstr[128];
    std::string fnm;
    AVMediaType strm_typ;

    fnm = chitm->playlist[chitm->playlist_index].fullnm;

    LOG_MSG(NTC, NO_ERRNO
        , "Ch%s: Opening file '%s'"
        , chitm->ch_nbr.c_str(), fnm.c_str());

    retcd = avformat_open_input(&chitm->ifile.fmt_ctx
        , fnm.c_str(), NULL, NULL);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not open input file '%s': %s"
            , chitm->ch_nbr.c_str(), fnm.c_str(), errstr);
        return -1;
    }

    retcd = avformat_find_stream_info(chitm->ifile.fmt_ctx, NULL);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed to retrieve input stream information"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    for (indx = 0; indx < (int)chitm->ifile.fmt_ctx->nb_streams; indx++) {
        strm_typ = chitm->ifile.fmt_ctx->streams[indx]->codecpar->codec_type;
        if ((strm_typ == AVMEDIA_TYPE_VIDEO) &&
            (chitm->ifile.video.index == -1)) {
            chitm->ifile.video.index = indx;
            if (decoder_init_video(chitm) != 0) {
                return -1;
            }
        } else if ((strm_typ == AVMEDIA_TYPE_AUDIO) &&
            (chitm->ifile.audio.index == -1)) {
            chitm->ifile.audio.index = indx;
            if (decoder_init_audio(chitm) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int decoder_get_ts(ctx_channel_item *chitm)
{
    int retcd, indx;
    int64_t temp_pts;

    /* Read some pkts to get correct start times */
    indx = 0;
    chitm->ifile.video.start_pts = -1;
    chitm->ifile.audio.start_pts = -1;
    while (indx < 100) {
        av_packet_free(&chitm->pkt_in);
        chitm->pkt_in = av_packet_alloc();
        chitm->pkt_in->data = nullptr;
        chitm->pkt_in->size = 0;

        retcd = av_read_frame(chitm->ifile.fmt_ctx, chitm->pkt_in);
        if (retcd < 0) {
            LOG_MSG(NTC, NO_ERRNO
                , "Ch%s: Failed to read first packets for stream %d"
                , chitm->ch_nbr.c_str(), indx);
            return -1;
        }

        if (chitm->pkt_in->pts != AV_NOPTS_VALUE) {
            if (chitm->pkt_in->stream_index == chitm->ifile.video.index) {
                if (chitm->ifile.video.start_pts == -1) {
                    chitm->ifile.video.start_pts = chitm->pkt_in->pts;
                }
            }
            if (chitm->pkt_in->stream_index == chitm->ifile.audio.index) {
                if (chitm->ifile.audio.start_pts == -1) {
                    chitm->ifile.audio.start_pts = chitm->pkt_in->pts;
                }
            }
        }

        if ((chitm->ifile.video.start_pts >= 0) &&
            (chitm->ifile.audio.start_pts >= 0)) {
            break;
        }

        indx++;
    }

    chitm->ifile.time_start = av_gettime_relative();
    if ((chitm->ifile.video.strm != nullptr) &&
        (chitm->ifile.audio.strm != nullptr)) {
        temp_pts = av_rescale_q(
            chitm->ifile.audio.start_pts
            , chitm->ifile.audio.strm->time_base
            , chitm->ifile.video.strm->time_base);
        if (temp_pts > chitm->ifile.video.start_pts ) {
            chitm->ifile.video.start_pts = temp_pts;
        } else {
            chitm->ifile.audio.start_pts = av_rescale_q(
            chitm->ifile.video.start_pts
            , chitm->ifile.video.strm->time_base
            , chitm->ifile.audio.strm->time_base);
        }
    }

    chitm->ifile.audio.last_pts = chitm->ifile.audio.start_pts;
    chitm->ifile.video.last_pts = chitm->ifile.video.start_pts;

    chitm->file_cnt++;
    return 0;
}

static void infile_wait(ctx_channel_item *chitm)
{
    int64_t tm_diff, pts_diff, tot_diff;
    int64_t sec_full, sec_msec;

    if (chitm->ch_finish == true) {
        return;
    }

    if (chitm->pktarray_start > 0)  {
        chitm->pktarray_start--;
/*
        LOG_MSG(NTC, NO_ERRNO
            , "%s: skipping first packets %d id %d index %d"
            , chitm->ch_nbr.c_str()
            , chitm->pktarray_start
            , chitm->pktarray[chitm->pktarray_index].idnbr
            , chitm->pktarray_index);
*/
        /* Slightly adjust time start to consider these skipped packets*/
        if ((chitm->pkt_in->stream_index == chitm->ifile.video.index) &&
            (chitm->pkt_in->pts != AV_NOPTS_VALUE)) {
            tm_diff = av_gettime_relative() - chitm->ifile.time_start;
            pts_diff = av_rescale_q(
                chitm->pkt_in->pts - chitm->ifile.video.start_pts
                , AVRational{1,1}
                , chitm->ifile.video.strm->time_base);
            tot_diff = pts_diff - tm_diff;
            chitm->ifile.time_start -= tot_diff;
        }
        return;
    }


    if ((chitm->pkt_in->stream_index != chitm->ifile.video.index) ||
        (chitm->pkt_in->pts == AV_NOPTS_VALUE)  ||
        (chitm->pkt_in->pts < chitm->ifile.video.last_pts)) {
        return;
    }

    chitm->ifile.video.last_pts = chitm->pkt_in->pts;

    /* How much time has really elapsed since the start of movie*/
    tm_diff = av_gettime_relative() - chitm->ifile.time_start;

    /* How much time the pts wants us to be at since the start of the movie */
    /* At this point the pkt pts is in ifile timebase.*/
    pts_diff = av_rescale_q(
        chitm->pkt_in->pts - chitm->ifile.video.start_pts
        , AVRational{1,1}
        , chitm->ifile.video.strm->time_base);

    /* How much time we need to wait to get in sync*/
    tot_diff = pts_diff - tm_diff;

    if (tot_diff > 0) {
        sec_full = int(tot_diff / 1000000L);
        sec_msec = (tot_diff % 1000000L);
        if (sec_full < 100){
            SLEEP(sec_full, sec_msec * 1000);
        }
    }
}

static void decoder_send(ctx_channel_item *chitm)
{
    int retcd;
    char errstr[128];

    if (chitm->pkt_in->stream_index == chitm->ifile.video.index) {
        retcd = avcodec_send_packet(chitm->ifile.video.codec_ctx, chitm->pkt_in);
    } else {
        retcd = avcodec_send_packet(chitm->ifile.audio.codec_ctx, chitm->pkt_in);
    }
    if (retcd == AVERROR_INVALIDDATA) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Send ignoring packet stream %d with invalid data"
            , chitm->ch_nbr.c_str(), chitm->pkt_in->stream_index);
            return;
    } else if (retcd < 0 && retcd != AVERROR_EOF) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Error sending packet to codec: %s"
            , chitm->ch_nbr.c_str(), errstr);
        return;
    }
}

static void decoder_receive(ctx_channel_item *chitm)
{
    int retcd;
    char errstr[128];

    if (chitm->frame != nullptr) {
        myframe_free(chitm->frame);
        chitm->frame = nullptr;
    }
    chitm->frame = myframe_alloc();

    if (chitm->pkt_in->stream_index == chitm->ifile.video.index) {
       retcd = avcodec_receive_frame(chitm->ifile.video.codec_ctx, chitm->frame);
    } else {
       retcd = avcodec_receive_frame(chitm->ifile.audio.codec_ctx, chitm->frame);
    }
    if (retcd < 0) {
        if (retcd == AVERROR_INVALIDDATA) {
            LOG_MSG(NTC, NO_ERRNO
                , "Ch%s: Ignoring packet with invalid data"
                , chitm->ch_nbr.c_str());
        } else if (retcd != AVERROR(EAGAIN)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            LOG_MSG(NTC, NO_ERRNO
                , "Ch%s: Error receiving frame from decoder: %s"
                , chitm->ch_nbr.c_str(), errstr);
        }
        if (chitm->frame != nullptr) {
            myframe_free(chitm->frame);
            chitm->frame = nullptr;
        }
    }

}

static int encode_buffer_audio(ctx_channel_item *chitm)
{
    int frame_size, retcd, bufspc;
    int frmsz_src, frmsz_dst;
    int64_t  pts, dts, ptsadj, dtsadj;
    char errstr[128];

    frmsz_src = chitm->ifile.audio.codec_ctx->frame_size;
    frmsz_dst = chitm->ofile.audio.codec_ctx->frame_size;

    retcd = av_audio_fifo_realloc(chitm->fifo, frmsz_src+frmsz_dst);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not allocate FIFO"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    retcd = av_audio_fifo_write(
        chitm->fifo, (void **)chitm->frame->data,frmsz_src);
    if (retcd < frmsz_src) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not write data to FIFO"
            , chitm->ch_nbr.c_str());
        return -1;
    }
    pts = chitm->frame->pts;
    dts = chitm->frame->pkt_dts;

    myframe_free(chitm->frame);
    chitm->frame = nullptr;

    retcd = av_audio_fifo_size(chitm->fifo);
    if (retcd < frmsz_dst) {
        chitm->audio_last_pts = pts;
        chitm->audio_last_dts = dts;
        return -1;
    }

    frame_size = FFMIN(av_audio_fifo_size(chitm->fifo), frmsz_dst);

    bufspc = av_audio_fifo_space(chitm->fifo) - frmsz_dst;
    ptsadj =((float)((pts - chitm->audio_last_pts) / frmsz_src) * bufspc);
    dtsadj =((float)((dts - chitm->audio_last_dts) / frmsz_src) * bufspc);

    chitm->audio_last_pts = pts;
    chitm->audio_last_dts = dts;

    chitm->frame = av_frame_alloc();
    if (chitm->frame == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not allocate output frame"
            , chitm->ch_nbr.c_str());
        return -1;
    }
    chitm->frame->nb_samples     = frame_size;
    av_channel_layout_copy(&chitm->frame->ch_layout
        , &chitm->ofile.audio.codec_ctx->ch_layout);
    chitm->frame->format         = chitm->ofile.audio.codec_ctx->sample_fmt;
    chitm->frame->sample_rate    = chitm->ofile.audio.codec_ctx->sample_rate;
    chitm->frame->pts = pts - ptsadj;
    chitm->frame->pkt_dts = dts-dtsadj;

    retcd = av_frame_get_buffer(chitm->frame, 0);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not allocate output frame buffer %s"
            , chitm->ch_nbr.c_str(), errstr);
        av_frame_free(&chitm->frame);
        chitm->frame = nullptr;
        return -1;
    }

    retcd = av_audio_fifo_read(chitm->fifo
        , (void **)chitm->frame->data, frame_size);
    if (retcd < frame_size) {
        fprintf(stderr, "Could not read data from FIFO\n");
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not read data from fifo"
            , chitm->ch_nbr.c_str());
        av_frame_free(&chitm->frame);
        chitm->frame = nullptr;
        return -1;
    }

    return 0;
}

static void encoder_send(ctx_channel_item *chitm)
{
    int retcd;
    char errstr[128];

    if (chitm->frame == nullptr) {
        return;
    }

    if (chitm->pkt_in->stream_index == chitm->ifile.video.index) {
        if  (chitm->frame->pts != AV_NOPTS_VALUE) {
            if (chitm->frame->pts <= chitm->ofile.video.last_pts) {
                LOG_MSG(NTC, NO_ERRNO
                    , "Ch%s: PTS Problem %d %d"
                    , chitm->ch_nbr.c_str()
                    ,chitm->frame->pts
                    ,chitm->ofile.video.last_pts);
                myframe_free(chitm->frame);
                chitm->frame = nullptr;
                return;
            }
            chitm->ofile.video.last_pts = chitm->frame->pts;
        }
        chitm->frame->quality = chitm->ofile.video.codec_ctx->global_quality;
        retcd = avcodec_send_frame(chitm->ofile.video.codec_ctx, chitm->frame);
    } else {
        if (chitm->ifile.audio.codec_ctx->codec_id == AV_CODEC_ID_AAC) {
            retcd = encode_buffer_audio(chitm);
            if (retcd < 0) {
                return;
            }
        }
        retcd = avcodec_send_frame(chitm->ofile.audio.codec_ctx, chitm->frame);
    }
    if (retcd < 0 ) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Error sending %d frame for encoding: %s"
            , chitm->ch_nbr.c_str(), chitm->pkt_in->stream_index, errstr);
        int indx;
        for (indx=0;indx<chitm->pktarray_count;indx++) {
            if (chitm->pktarray[indx].packet != nullptr) {
                if (chitm->pktarray[indx].packet->stream_index == 0) {
                    LOG_MSG(NTC, NO_ERRNO
                        , "Ch%s: index %d stream %d frame %d pts %d arrayindex %d"
                        , chitm->ch_nbr.c_str(), indx
                        , chitm->pktarray[indx].packet->stream_index
                        , chitm->frame->pts
                        , chitm->pktarray[indx].packet->pts
                        , chitm->pktarray_index);
                }
            }
        }
        abort();
    }

    myframe_free(chitm->frame);
    chitm->frame = nullptr;

}

static void packetarray_resize(ctx_channel_item *chitm)
{
    ctx_packet_item pktitm;
    int indx;

    pktitm.idnbr = -1;
    pktitm.iskey = false;
    pktitm.iswritten = false;
    pktitm.packet = nullptr;
    pktitm.file_cnt = 0;
    pktitm.start_pts = 0;
    pktitm.timebase={0,0};

    pthread_mutex_lock(&chitm->mtx_pktarray);
        /* 600 is arbitrary */
        for (indx=1; indx <= 600; indx++) {
            chitm->pktarray.push_back(pktitm);
        }
        chitm->pktarray_count = 600;
    pthread_mutex_unlock(&chitm->mtx_pktarray);
}

int pktarray_get_index(ctx_channel_item *chitm)
{
    int retval;
    pthread_mutex_lock(&chitm->mtx_pktarray);
        retval = chitm->pktarray_index;
    pthread_mutex_unlock(&chitm->mtx_pktarray);
    return retval;
}

int pktarray_indx_next(int index)
{
    if (index == 599) {
        return 0;
    } else {
        return ++index;
    }
}

int pktarray_indx_prev(int index)
{
    if (index == 0) {
        return 599;
    } else {
        return --index;
    }
}

static void packetarray_add(ctx_channel_item *chitm, AVPacket *pkt)
{
    int indx_next, retcd;
    static int keycnt;
    char errstr[128];

    indx_next = pktarray_indx_next(chitm->pktarray_index);

    pthread_mutex_lock(&chitm->mtx_pktarray);

        chitm->pktnbr++;
        chitm->pktarray[indx_next].idnbr = chitm->pktnbr;

        if (chitm->pktarray[indx_next].packet != nullptr) {
            mypacket_free(chitm->pktarray[indx_next].packet);
            chitm->pktarray[indx_next].packet = nullptr;
        }
        chitm->pktarray[indx_next].packet = mypacket_alloc(chitm->pktarray[indx_next].packet);

        retcd = mycopy_packet(chitm->pktarray[indx_next].packet, pkt);
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            LOG_MSG(NTC, NO_ERRNO
                , "Ch%s: Error copying packet: %s"
                , chitm->ch_nbr.c_str(), errstr);
            mypacket_free(chitm->pktarray[indx_next].packet);
            chitm->pktarray[indx_next].packet = NULL;
        }

        if (chitm->pktarray[indx_next].packet->flags & AV_PKT_FLAG_KEY) {
            chitm->pktarray[indx_next].iskey = true;
            if (chitm->pktarray[indx_next].packet->stream_index == 0) {
//                LOG_MSG(NTC, NO_ERRNO
//                    , "%s: key interval  %d"
//                    , chitm->ch_nbr.c_str(),keycnt);
                keycnt = 0;
            }
        } else {
            chitm->pktarray[indx_next].iskey = false;
            if (chitm->pktarray[indx_next].packet->stream_index == 0) {
                keycnt++;
            }
        }
        chitm->pktarray[indx_next].iswritten = false;
        chitm->pktarray[indx_next].file_cnt = chitm->file_cnt;
        if (chitm->pkt_in->stream_index == chitm->ifile.video.index) {
            chitm->pktarray[indx_next].timebase = chitm->ifile.video.strm->time_base;
            chitm->pktarray[indx_next].start_pts= chitm->ifile.video.start_pts;
        } else {
            chitm->pktarray[indx_next].timebase = chitm->ifile.audio.strm->time_base;
            chitm->pktarray[indx_next].start_pts= chitm->ifile.audio.start_pts;
        }

        chitm->pktarray_index = indx_next;

    pthread_mutex_unlock(&chitm->mtx_pktarray);

/*
    LOG_MSG(ERR, NO_ERRNO," %d %d/%d %d/%d %d/%d %d/%d "
        ,chitm->ifile.video.codec_ctx->framerate
        ,chitm->ifile.video.strm->time_base.num
        ,chitm->ifile.video.strm->time_base.den
        ,chitm->ifile.video.codec_ctx->time_base.num
        ,chitm->ifile.video.codec_ctx->time_base.den
        ,chitm->ofile.video.strm->time_base.num
        ,chitm->ofile.video.strm->time_base.den
        ,chitm->ofile.video.codec_ctx->time_base.num
        ,chitm->ofile.video.codec_ctx->time_base.den
    );
*/

}

static void encoder_receive(ctx_channel_item *chitm)
{
    int retcd;
    char errstr[128];
    AVPacket *pkt;

    retcd = 0;
    while (retcd == 0) {
        pkt = NULL;
        pkt = mypacket_alloc(pkt);
        if (chitm->pkt_in->stream_index == chitm->ifile.video.index) {
            retcd = avcodec_receive_packet(chitm->ofile.video.codec_ctx, pkt);
            pkt->stream_index =chitm->ofile.video.index;
        } else {
            retcd = avcodec_receive_packet(chitm->ofile.audio.codec_ctx, pkt);
            pkt->stream_index =chitm->ofile.audio.index;
        }
        if (retcd == AVERROR(EAGAIN)) {
            mypacket_free(pkt);
            pkt = NULL;
            return;
        } else if (retcd < 0 ) {
            av_strerror(retcd, errstr, sizeof(errstr));
            LOG_MSG(ERR, NO_ERRNO
                ,"Error receiving encoded packet video:%s", errstr);
            //Packet is freed upon failure of encoding
            return;
        } else {
            /* Some files start with negative pts values. */
            //LOG_MSG(NTC, NO_ERRNO, "%s: adding pkt sz %d"
            //    , chitm->ch_nbr.c_str(), pkt->size);
            if (pkt->pts > 0) {
                packetarray_add(chitm, pkt);
            }
            mypacket_free(pkt);
            pkt = NULL;
        }
    }
}

void infile_read(ctx_channel_item *chitm)
{
    int retcd;

    while (chitm->ch_finish == false) {
        av_packet_free(&chitm->pkt_in);
        chitm->pkt_in = av_packet_alloc();

        chitm->pkt_in->data = NULL;
        chitm->pkt_in->size = 0;

        retcd = av_read_frame(chitm->ifile.fmt_ctx, chitm->pkt_in);
        if (retcd < 0) {
            break;
        }

        infile_wait(chitm);

        if (chitm->cnct_cnt > 0) {
            decoder_send(chitm);
            decoder_receive(chitm);
            encoder_send(chitm);
            encoder_receive(chitm);
        }
        if (chitm->ifile.fmt_ctx == NULL) {
            break;
        }
    }
    av_packet_free(&chitm->pkt_in);
}

static int encoder_init_video_h264(ctx_channel_item *chitm)
{
    const AVCodec *encoder;
    AVStream *stream;
    AVCodecContext *enc_ctx,*dec_ctx;
    AVDictionary *opts = NULL;
    char errstr[128];
    int retcd;

    chitm->ofile.video.codec_ctx = nullptr;
    stream = avformat_new_stream(chitm->ofile.fmt_ctx, NULL);
    chitm->ofile.video.strm = stream;
    if (stream == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed allocating output video stream"
            , chitm->ch_nbr.c_str());
        return -1;
    }
    chitm->ofile.video.index = stream->index;

    encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (encoder == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not find video encoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    chitm->ofile.video.codec_ctx = avcodec_alloc_context3(encoder);
    if (chitm->ofile.video.codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not allocate video encoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    enc_ctx = chitm->ofile.video.codec_ctx;
    dec_ctx = chitm->ifile.video.codec_ctx;

    retcd = avcodec_parameters_from_context(stream->codecpar, dec_ctx);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not copy parms from video decoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    retcd = avcodec_parameters_to_context(enc_ctx, stream->codecpar);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not copy parms to video encoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx->width = dec_ctx->width;
    enc_ctx->height = dec_ctx->height;
    enc_ctx->time_base.num = 1;
    enc_ctx->time_base.den = 90000;
    enc_ctx->max_b_frames = 4;
    enc_ctx->framerate = dec_ctx->framerate;
    enc_ctx->bit_rate = 400000;
    enc_ctx->gop_size = 4;
    if (dec_ctx->pix_fmt == -1) {
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    } else {
        enc_ctx->pix_fmt = dec_ctx->pix_fmt;
    }

    if (chitm->ofile.fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_dict_set( &opts, "profile", "baseline", 0 );
    av_dict_set( &opts, "crf", "17", 0 );
    av_dict_set( &opts, "tune", "zerolatency", 0 );
    av_dict_set( &opts, "preset", "superfast", 0 );
    av_dict_set( &opts, "keyint", "4", 0 );
    av_dict_set( &opts, "scenecut", "0", 0 );

    retcd = avcodec_open2(enc_ctx, encoder, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not open video encoder: %s %dx%d %d %d"
            , chitm->ch_nbr.c_str(), errstr
            , enc_ctx->width, enc_ctx->height
            , enc_ctx->pix_fmt, enc_ctx->time_base.den);
        return -1;
    }
    av_dict_free(&opts);

    retcd = avcodec_parameters_from_context(stream->codecpar, enc_ctx);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not copy parms from decoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }
    stream->time_base.num = 1;
    stream->time_base.den = 90000;
    return 0;
}

static int encoder_init_video_mpeg(ctx_channel_item *chitm)
{
    const AVCodec *encoder;
    AVStream *stream;
    AVCodecContext *enc_ctx, *dec_ctx;
    AVDictionary *opts = nullptr;
    char errstr[128];
    int retcd;

    chitm->ofile.video.codec_ctx = nullptr;

    stream = avformat_new_stream(chitm->ofile.fmt_ctx, NULL);
    chitm->ofile.video.strm = stream;
    if (stream == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed allocating output video stream"
            , chitm->ch_nbr.c_str());
        return -1;
    }
    chitm->ofile.video.index = stream->index;

    encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
    if (encoder == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not find video encoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    chitm->ofile.fmt_ctx->video_codec_id = AV_CODEC_ID_MPEG2VIDEO;

    chitm->ofile.video.codec_ctx = avcodec_alloc_context3(encoder);
    if (chitm->ofile.video.codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not allocate video encoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    enc_ctx = chitm->ofile.video.codec_ctx;
    dec_ctx = chitm->ifile.video.codec_ctx;

    retcd = avcodec_parameters_from_context(stream->codecpar, dec_ctx);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not copy parms from video decoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    enc_ctx->codec_id = AV_CODEC_ID_MPEG2VIDEO;
    enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx->width = dec_ctx->width;
    enc_ctx->height = dec_ctx->height;
    enc_ctx->max_b_frames = 4;
    enc_ctx->gop_size = 4;
    enc_ctx->framerate = dec_ctx->framerate;
    enc_ctx->framerate.num = 30;
    enc_ctx->framerate.den = 1;
    enc_ctx->time_base.num = enc_ctx->framerate.den;
    enc_ctx->time_base.den = enc_ctx->framerate.num;
    enc_ctx->bit_rate = 6000000;
    enc_ctx->sw_pix_fmt = AV_PIX_FMT_YUV420P;
    enc_ctx->global_quality = 150;
    enc_ctx->flags |= AV_CODEC_FLAG_QSCALE;
    //enc_ctx->global_quality = FF_QP2LAMBDA * qscale;

    if (dec_ctx->pix_fmt == -1) {
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    } else {
        enc_ctx->pix_fmt = dec_ctx->pix_fmt;
    }
    if (chitm->ofile.fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    retcd = avcodec_open2(enc_ctx, encoder, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not open video encoder: %s %dx%d %d %d %d/%d"
            , chitm->ch_nbr.c_str(),errstr
            , enc_ctx->width, enc_ctx->height
            , enc_ctx->pix_fmt, enc_ctx->time_base.den
            ,enc_ctx->framerate.num,enc_ctx->framerate.den);
            abort();
        return -1;
    }
    av_dict_free(&opts);

    retcd = avcodec_parameters_from_context(stream->codecpar, enc_ctx);
    if (retcd < 0) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not copy parms from decoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }
    stream->time_base.num = 1;
    stream->time_base.den = 90000;
    return 0;

}

static int encoder_init_audio(ctx_channel_item *chitm)
{
    AVStream *stream;
    AVCodecContext *enc_ctx,*dec_ctx;
    int retcd;
    char errstr[128];
    AVDictionary *opts = NULL;
    const AVCodec *encoder;

    chitm->ofile.audio.codec_ctx = nullptr;

    encoder = avcodec_find_encoder(AV_CODEC_ID_AC3);

    stream = avformat_new_stream(chitm->ofile.fmt_ctx, encoder);
    chitm->ofile.audio.strm = stream;
    if (stream == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Failed allocating output stream"
            , chitm->ch_nbr.c_str());
        return -1;
    }
    chitm->ofile.audio.index = stream->index;

    chitm->ofile.audio.codec_ctx = avcodec_alloc_context3(encoder);
    if (chitm->ofile.audio.codec_ctx == nullptr) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not allocate audio encoder"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    enc_ctx = chitm->ofile.audio.codec_ctx;
    dec_ctx = chitm->ifile.audio.codec_ctx;

    chitm->ofile.fmt_ctx->audio_codec_id = AV_CODEC_ID_AC3;
    chitm->ofile.fmt_ctx->audio_codec = avcodec_find_encoder(AV_CODEC_ID_AC3);
    stream->codecpar->codec_id=AV_CODEC_ID_AC3;

    stream->codecpar->bit_rate = dec_ctx->bit_rate;
    stream->codecpar->frame_size = dec_ctx->frame_size;
    stream->codecpar->format = dec_ctx->sample_fmt;
    stream->codecpar->sample_rate = dec_ctx->sample_rate;
    stream->time_base.den  = dec_ctx->sample_rate;
    stream->time_base.num = 1;
    av_channel_layout_default(&stream->codecpar->ch_layout
        , dec_ctx->ch_layout.nb_channels);

    if (dec_ctx->bit_rate == 0) {
        enc_ctx->bit_rate = dec_ctx->sample_rate * 10;
    } else {
        enc_ctx->bit_rate = dec_ctx->bit_rate;
    }
    enc_ctx->frame_size = dec_ctx->frame_size;
    enc_ctx->sample_fmt = dec_ctx->sample_fmt;
    enc_ctx->sample_rate = dec_ctx->sample_rate;
    enc_ctx->time_base.num = 1;
    enc_ctx->time_base.den  = dec_ctx->sample_rate;
    av_channel_layout_default(&enc_ctx->ch_layout
        , dec_ctx->ch_layout.nb_channels);

    retcd = avcodec_open2(enc_ctx, encoder, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: Could not open audio encoder %s"
            , chitm->ch_nbr.c_str(), errstr);
        return -1;
    }
    av_dict_free(&opts);


    if (dec_ctx->codec_id == AV_CODEC_ID_AAC ) {
        /* Create the FIFO buffer based on the specified output sample format. */
        chitm->fifo = av_audio_fifo_alloc(
            chitm->ofile.audio.codec_ctx->sample_fmt
            , chitm->ofile.audio.codec_ctx->ch_layout.nb_channels, 1);
        if (chitm->fifo == nullptr) {
            LOG_MSG(NTC, NO_ERRNO
                , "Ch%s: Could not allocate FIFO"
                , chitm->ch_nbr.c_str());
            return -1;
        }
    }

    return 0;
}

int encoder_init(ctx_channel_item *chitm)
{
    if (chitm->ifile.fmt_ctx == NULL) {
        LOG_MSG(NTC, NO_ERRNO
            , "Ch%s: no input file provided"
            , chitm->ch_nbr.c_str());
        return -1;
    }

    chitm->ofile.fmt_ctx = avformat_alloc_context();
    chitm->ofile.fmt_ctx->oformat = av_guess_format("mpegts", NULL, NULL);

    if (chitm->pktarray_count == 0) {
        packetarray_resize(chitm);
    }

    if (chitm->ifile.video.index != -1) {
        if (chitm->ch_encode == "h264") {
            if (encoder_init_video_h264(chitm) != 0) {
                return -1;
            }
        } else {
            if (encoder_init_video_mpeg(chitm) != 0) {
                return -1;
            }
        }
    }
    if (chitm->ifile.audio.index != -1) {
        if (encoder_init_audio(chitm) != 0) {
            return -1;
        }
    }
    return 0;

}

void streams_close(ctx_channel_item *chitm)
{
    LOG_MSG(NTC, NO_ERRNO, "Ch%s: Closing"
        , chitm->ch_nbr.c_str());

    if (chitm->ifile.audio.codec_ctx !=  nullptr) {
        avcodec_free_context(&chitm->ifile.audio.codec_ctx);
        chitm->ifile.audio.codec_ctx =  nullptr;
    }
    if (chitm->ifile.video.codec_ctx !=  nullptr) {
        avcodec_free_context(&chitm->ifile.video.codec_ctx);
        chitm->ifile.video.codec_ctx =  nullptr;
    }
    if (chitm->ifile.fmt_ctx != nullptr) {
        avformat_close_input(&chitm->ifile.fmt_ctx);
        chitm->ifile.fmt_ctx = nullptr;
    }

    if (chitm->ofile.audio.codec_ctx !=  nullptr) {
        avcodec_free_context(&chitm->ofile.audio.codec_ctx);
        chitm->ofile.audio.codec_ctx =  nullptr;
    }
    if (chitm->ofile.video.codec_ctx !=  nullptr) {
        avcodec_free_context(&chitm->ofile.video.codec_ctx);
        chitm->ofile.video.codec_ctx =  nullptr;
    }
    if (chitm->ofile.fmt_ctx != nullptr) {
        avformat_free_context(chitm->ofile.fmt_ctx);
        chitm->ofile.fmt_ctx = nullptr;
    }

    if (chitm->fifo != nullptr) {
        av_audio_fifo_free(chitm->fifo);
        chitm->fifo= nullptr;
    }
}


