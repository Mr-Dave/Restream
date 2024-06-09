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

    int decoder_init(cls_channel *chitm);
    int encoder_init(cls_channel *chitm);
    int decoder_get_ts(cls_channel *chitm);
    void infile_read(cls_channel *chitm);
    void streams_close(cls_channel *chitm);
    int pktarray_get_index(cls_channel *chitm);
    int pktarray_indx_next(int index);
    int pktarray_indx_prev(int index);

#endif
