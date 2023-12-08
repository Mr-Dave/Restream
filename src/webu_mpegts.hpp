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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
*/

#ifndef _INCLUDE_WEBU_MPEGTS_HPP_
#define _INCLUDE_WEBU_MPEGTS_HPP_

    mhdrslt webu_stream_main(ctx_webui *webui);
    mhdrslt webu_mpegts_main(ctx_webui *webui);
    void webu_mpegts_free_context(ctx_webui *webui);

#endif /* _INCLUDE_WEBU_MPEGTS_HPP_ */
