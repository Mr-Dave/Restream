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

#ifndef _INCLUDE_PKTARRAY_HPP_
#define _INCLUDE_PKTARRAY_HPP_
    class cls_pktarray{
        public:
            cls_pktarray(cls_channel *p_chitm);
            ~cls_pktarray();
            std::vector<ctx_packet_item> array;
            int     count;
            int     start;
            int64_t pktnbr;
            pthread_mutex_t    mtx;
            void    resize();
            void    add(AVPacket *pkt);
            int     index_curr();
            int     index_next(int index);
            int     index_prev(int index);
        private:
            std::string     ch_nbr;
            cls_channel     *chitm;
            int             arrayindex;
    };

#endif
