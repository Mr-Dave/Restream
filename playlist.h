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

#ifndef _INCLUDE_PLAYLIST_H_
    #define _INCLUDE_PLAYLIST_H_

    struct playlist_item {
        char   *movie_path;
        int    path_length;
        int    movie_seq;
    };

    struct channel_item {
        char         *channel_dir;
        char         *channel_pipe;
        char         *channel_order;
        int          channel_status;
        unsigned int channel_seed;
        pthread_t    process_channel_thread;
    };

    int playlist_loaddir(ctx_restream *restrm);
    int playlist_free(ctx_restream *restrm);

#endif