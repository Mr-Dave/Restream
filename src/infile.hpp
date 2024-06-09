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

#ifndef _INCLUDE_INFILE_HPP_
#define _INCLUDE_INFILE_HPP_

    class cls_infile {
        public:
            cls_infile(cls_channel *p_chitm);
            ~cls_infile();

            void start(std::string fnm);
            void read();
            void stop();

            ctx_file_info   ifile;
            ctx_file_info   ofile;
            pthread_mutex_t mtx;

        private:
            bool            is_started;
            cls_channel     *chitm;
            std::string     ch_nbr;

            AVPacket        *pkt_in;
            AVFrame         *frame;
            AVAudioFifo     *fifo;
            int64_t         audio_last_pts;
            int64_t         audio_last_dts;

            void defaults();

            int  decoder_init_video();
            int  decoder_init_audio();
            int  decoder_init(std::string fnm);
            int  decoder_get_ts();
            void decoder_send();
            void decoder_receive();

            int  encoder_buffer_audio();
            void encoder_send();
            void encoder_receive();
            int  encoder_init_video_h264();
            int  encoder_init_video_mpeg();
            int  encoder_init_audio();
            int  encoder_init();

            void infile_wait();

    };

#endif
