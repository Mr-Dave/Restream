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

#ifndef _INCLUDE_WRITER_HPP_
#define _INCLUDE_WRITER_HPP_

    int writer_packet_flush(ctx_restream *restrm);
    int writer_init_video(ctx_restream *restrm, int indx);
    int writer_init_audio(ctx_restream *restrm, int indx);
    int writer_init_open(ctx_restream *restrm);
    int writer_init(ctx_restream *restrm);
    void writer_close_encoder(ctx_restream *restrm);
    void writer_close(ctx_restream *restrm);
    void writer_rescale_frame(ctx_restream *restrm);
    void writer_rescale_enc_pkt(ctx_restream *restrm);
    int writer_packet_sendpkt(ctx_restream *restrm);
    int writer_packet_encode(ctx_restream *restrm);
    void writer_packet(ctx_restream *restrm);

#endif
