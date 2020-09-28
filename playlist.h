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
        pthread_t    process_channel_thread;
    };

    int playlist_loaddir(ctx_restream *restrm);
    int playlist_free(ctx_restream *restrm);
    
#endif