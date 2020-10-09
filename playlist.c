

    #include "restream.h"
    #include "guide.h"
    #include "playlist.h"
    #include "infile.h"
    #include "reader.h"
    #include "writer.h"

int playlist_free(ctx_restream *restrm){

    int  indx;
    snprintf(restrm->function_name,1024,"%s","playlist_free");

    if (restrm != NULL) {
        if(restrm->playlist_count > 0 ){
            indx = 0;
            while (indx < restrm->playlist_count) {
                if (restrm->playlist[indx].movie_path != NULL){
                    free(restrm->playlist[indx].movie_path);
                }
                indx ++;
            }
        }
        if (restrm->playlist != NULL )  free(restrm->playlist);
    }
    //fprintf(stderr,"Playlist freed \n");

    return 0;

}

void playlist_sort_alpha(ctx_restream *restrm){

    char *temp;
    int indx1, indx2;
    snprintf(restrm->function_name,1024,"%s","playlist_sort_alpha");

    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    //fprintf(stderr,"%s: alpha \n",restrm->guide_info->guide_displayname);

    for(indx1=0; indx1 < restrm->playlist_count - 1 ; indx1++){
        for(indx2 =indx1 + 1; indx2 < restrm->playlist_count ; indx2++)
        {
            if(strcasecmp(restrm->playlist[indx1].movie_path,restrm->playlist[indx2].movie_path) > 0)
            {
                temp = restrm->playlist[indx1].movie_path;
                restrm->playlist[indx1].movie_path = restrm->playlist[indx2].movie_path;
                restrm->playlist[indx2].movie_path = temp;
            }
        }
    }
}

void playlist_sort_random(ctx_restream *restrm){

    char *temp;
    int indx1, indx2, tmpseq, tmplen;

    snprintf(restrm->function_name,1024,"%s","playlist_sort_random");

    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    /*
    fprintf(stderr,"%s: seed: %u\n"
            ,restrm->guide_info->guide_displayname
            ,usec );
    srand(usec);
    */

    for(indx1=0; indx1 <= restrm->playlist_count-1 ; indx1++){
        restrm->playlist[indx1].movie_seq = rand_r(&restrm->rand_seed);
        /*
        fprintf(stderr," %d %s: \n"
            ,restrm->playlist[indx1].movie_seq
            ,restrm->playlist[indx1].movie_path);
        */
    }

    for(indx1=0; indx1 < restrm->playlist_count-1 ; indx1++){
        for(indx2 =0; indx2 < restrm->playlist_count-1-indx1; indx2++) {
            if(restrm->playlist[indx2].movie_seq > restrm->playlist[indx2+1].movie_seq) {

                temp = restrm->playlist[indx2].movie_path;
                tmpseq = restrm->playlist[indx2].movie_seq;
                tmplen = restrm->playlist[indx2].path_length;

                restrm->playlist[indx2].movie_path = restrm->playlist[indx2+1].movie_path;
                restrm->playlist[indx2].movie_seq = restrm->playlist[indx2+1].movie_seq;
                restrm->playlist[indx2].path_length = restrm->playlist[indx2+1].path_length;

                restrm->playlist[indx2+1].movie_path = temp;
                restrm->playlist[indx2+1].movie_seq = tmpseq;
                restrm->playlist[indx2+1].path_length = tmplen;
            }
        }
    }

    /*
    for(indx1=0; indx1 <= restrm->playlist_count-1 ; indx1++){
        fprintf(stderr," %d %s: \n"
            ,restrm->playlist[indx1].movie_seq
            , restrm->playlist[indx1].movie_path);
    }
    */
}

int playlist_loaddir(ctx_restream *restrm){

    DIR           *d;
    struct dirent *dir;
    size_t         basepath_len, totlen;
    int            indx, retcd;

    snprintf(restrm->function_name,1024,"%s","playlist_loaddir");

    restrm->watchdog_playlist = av_gettime_relative() + 5000000;

    playlist_free(restrm);

    restrm->playlist_count = 0;

    d = opendir(restrm->playlist_dir);
    if (d) {
        basepath_len = strlen(restrm->playlist_dir)+1;
        while ((dir=readdir(d)) != NULL){
            if ((strstr(dir->d_name,".mkv") != NULL) || (strstr(dir->d_name,".mp4") != NULL)) {
                restrm->playlist_count++;
            }
        }
        if (restrm->playlist_count != 0){
            restrm->playlist = malloc(sizeof(struct playlist_item) * restrm->playlist_count);
            rewinddir(d);
            indx = 0;
            while ((dir=readdir(d)) != NULL){
                if ((strstr(dir->d_name,".mkv") != NULL) || (strstr(dir->d_name,".mp4") != NULL)) {
                    totlen =basepath_len + strlen(dir->d_name);
                    restrm->playlist[indx].movie_path = calloc(totlen + 2,sizeof(char));
                    retcd = snprintf(restrm->playlist[indx].movie_path, totlen+1
                        ,"%s%s",restrm->playlist_dir,dir->d_name);
                    if (retcd < 0){
                        fprintf(stderr,"Error on playlist %s\n",restrm->playlist_dir);
                        return -1;
                    }
                    indx ++;
                }
            }
            if (strcasecmp(restrm->playlist_sort_method,"a") == 0) {
                playlist_sort_alpha(restrm);
            } else {
                playlist_sort_random(restrm);
            }
            for(indx=0; indx < restrm->playlist_count; indx++){
                restrm->playlist[indx].path_length = basepath_len + strlen(restrm->playlist[indx].movie_path) ;
                //fprintf(stderr,"%d: %s \n",restrm->playlist[indx].path_length, restrm->playlist[indx].movie_path);
            }
        }
        closedir(d);
    }

    if (restrm->playlist_count == 0){
        fprintf(stderr,"Playlist count is zero\n");
        return -1;
    }
    return 0;

}
