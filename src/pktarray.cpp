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
#include "channel.hpp"
#include "infile.hpp"
#include "pktarray.hpp"
#include "webu.hpp"

void cls_pktarray::resize()
{
    ctx_packet_item pktitm;
    int indx;

    if (count <= 0) {
        LOG_MSG(ERR, NO_ERRNO,"Attempt to resize to zero");
        abort();
    }

    pktitm.idnbr = -1;
    pktitm.iskey = false;
    pktitm.iswritten = false;
    pktitm.packet = nullptr;
    pktitm.file_cnt = 0;
    pktitm.start_pts = 0;
    pktitm.timebase={0,0};

    pthread_mutex_lock(&mtx);
        for (indx=1; indx <= count; indx++) {
            array.push_back(pktitm);
        }
    pthread_mutex_unlock(&mtx);
}

int cls_pktarray::index_curr()
{
    int retval;
    pthread_mutex_lock(&mtx);
        retval = arrayindex;
    pthread_mutex_unlock(&mtx);
    return retval;
}

int cls_pktarray::index_next(int index)
{
    if (index == (count -1)) {
        return 0;
    } else {
        return ++index;
    }
}

int cls_pktarray::index_prev(int index)
{
    if (index == 0) {
        return (count -1);
    } else {
        return --index;
    }
}

void cls_pktarray::add(AVPacket *pkt)
{
    int indx_next, retcd;
    static int keycnt;
    char errstr[128];

    indx_next = index_next(arrayindex);

    pthread_mutex_lock(&mtx);

        pktnbr++;
        array[indx_next].idnbr = pktnbr;

        if (array[indx_next].packet != nullptr) {
            mypacket_free(array[indx_next].packet);
            array[indx_next].packet = nullptr;
        }
        array[indx_next].packet = mypacket_alloc(array[indx_next].packet);

        retcd = mycopy_packet(array[indx_next].packet, pkt);
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            LOG_MSG(NTC, NO_ERRNO
                , "Ch%s: Error copying packet: %s"
                , ch_nbr.c_str(), errstr);
            mypacket_free(array[indx_next].packet);
            array[indx_next].packet = NULL;
        }

        if (array[indx_next].packet->flags & AV_PKT_FLAG_KEY) {
            array[indx_next].iskey = true;
            if (array[indx_next].packet->stream_index == 0) {
                keycnt = 0;
            }
        } else {
            array[indx_next].iskey = false;
            if (array[indx_next].packet->stream_index == 0) {
                keycnt++;
            }
        }
        array[indx_next].iswritten = false;
        array[indx_next].file_cnt = chitm->file_cnt;
        if (pkt->stream_index == chitm->infile->ifile.video.index) {
            array[indx_next].timebase = chitm->infile->ifile.video.strm->time_base;
            array[indx_next].start_pts= chitm->infile->ifile.video.start_pts;
        } else {
            array[indx_next].timebase = chitm->infile->ifile.audio.strm->time_base;
            array[indx_next].start_pts= chitm->infile->ifile.audio.start_pts;
        }

        arrayindex = indx_next;

    pthread_mutex_unlock(&mtx);

/*
    LOG_MSG(ERR, NO_ERRNO," %d %d/%d %d/%d %d/%d %d/%d "
        ,ifile.video.codec_ctx->framerate
        ,ifile.video.strm->time_base.num
        ,ifile.video.strm->time_base.den
        ,ifile.video.codec_ctx->time_base.num
        ,ifile.video.codec_ctx->time_base.den
        ,ofile.video.strm->time_base.num
        ,ofile.video.strm->time_base.den
        ,ofile.video.codec_ctx->time_base.num
        ,ofile.video.codec_ctx->time_base.den
    );
*/

}

cls_pktarray::cls_pktarray(cls_channel *p_chitm)
{
    pthread_mutex_init(&mtx, NULL);
    chitm = p_chitm;
    pktnbr = 0;
    count = 600;        /* 600 is arbitrary */
    arrayindex = -1;
    start = 0;
    resize();
}

cls_pktarray::~cls_pktarray()
{
    int indx;

    pthread_mutex_destroy(&mtx);

    for (indx=0; indx < (int)array.size(); indx++) {
        if (array[indx].packet != nullptr) {
            mypacket_free(array[indx].packet);
            array[indx].packet = nullptr;
        }
    }

    array.clear();
    count = 0;

}

