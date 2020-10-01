/*
 * This program used as the starting point the trancoding example as
 * provided by the FFmpeg project.  As such, the copyright notices and
 * notifications associated with that starting example are included below.
 *
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * API example for demuxing, decoding, filtering, encoding and muxing
 * @example transcoding.c
 */

    #include "restream.h"
    #include "guide.h"
    #include "playlist.h"
    #include "infile.h"
    #include "reader.h"
    #include "writer.h"

void signal_handler(int signo){

    switch(signo) {
    case SIGALRM:
        fprintf(stderr, "Caught alarm signal.\n");
        break;
    case SIGINT:
        fprintf(stderr, "Caught interrupt signal.\n");
        finish = 1;
        break;
    case SIGABRT:
        fprintf(stderr, "Caught abort signal.\n");
        break;
    case SIGHUP:
        fprintf(stderr, "Caught hup signal.\n");
        break;
    case SIGQUIT:
        fprintf(stderr, "Caught quit signal.\n");
        break;
    case SIGIO:
        fprintf(stderr, "Caught IO signal.\n");
        break;
    case SIGTERM:
        fprintf(stderr, "Caught term signal.\n");
        break;
    case SIGPIPE:
        //fprintf(stderr, "Caught pipe signal.\n");
        break;
    case SIGVTALRM:
        fprintf(stderr, "Caught alarm signal.\n");
        break;

    }
}

void signal_setup(){

    if (signal(SIGPIPE, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch pipe signal.\n");
    if (signal(SIGALRM, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch alarm signal.\n");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch term signal.\n");
    if (signal(SIGQUIT, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch quit signal.\n");
    if (signal(SIGHUP, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch hup signal.\n");
    if (signal(SIGABRT, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch abort signal.\n");
    if (signal(SIGVTALRM, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch VTalarm\n");
    if (signal(SIGINT, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch VTalarm\n");

}

int restrm_interrupt(void *ctx){
    ctx_restream *restrm = ctx;

    snprintf(restrm->function_name,1024,"%s","restrm_interrupt");

    /* This needs to be worked */

    return FALSE;
    (void)(restrm->ofmt);

  /*
        if ((rtsp_data->interruptcurrenttime.tv_sec - rtsp_data->interruptstarttime.tv_sec ) > rtsp_data->interruptduration){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera reading (%s) timed out")
                , rtsp_data->cameratype, rtsp_data->camera_name);
            rtsp_data->interrupted = TRUE;
            return TRUE;
        } else{
            return FALSE;
        }
  */

  /* should not be possible to get here */
    return FALSE;
}


void output_checkpipe(ctx_restream *restrm){

    int pipefd;

    snprintf(restrm->function_name,1024,"%s","output_checkpipe");

    pipefd = open(restrm->out_filename, O_WRONLY | O_NONBLOCK);
    if (pipefd == -1 ){

        if (errno == ENXIO){
            restrm->pipe_state = PIPE_IS_CLOSED;
        } else if (errno == ENOENT){
            fprintf(stderr, "%s: Error: No such pipe %s\n", restrm->out_filename
                ,restrm->guide_info->guide_displayname);
            restrm->pipe_state = PIPE_IS_CLOSED;
        } else {
            fprintf(stderr, "%s: Error occurred when checking status of named pipe %d \n"
                ,restrm->guide_info->guide_displayname, errno);
            restrm->pipe_state = PIPE_IS_CLOSED;
        }
    } else {
        restrm->pipe_state = PIPE_IS_OPEN;
        close(pipefd);
    }

    return;

}

void output_pipestatus(ctx_restream *restrm){

    int retcd;
    int64_t tmnow;

    if (finish) return;

    /* Frequently when the pipe gets opened, it gets a close request
     * in a very short timeframe thereafter and the associated application
     * reading from the pipe fails.  To avert this, we set a delay which
     * requires that the pipe remain open for at least 2 seconds before
     * allowing it to go back to a closed status
     */
    tmnow  = av_gettime_relative();
    if (((tmnow - restrm->connect_start)<2000000) &&
        (restrm->pipe_state == PIPE_IS_OPEN)){
        return;
    }

    snprintf(restrm->function_name,1024,"%s","output_pipestatus");

    restrm->watchdog_playlist = av_gettime_relative();

    if (restrm->pipe_state == PIPE_NEEDS_RESET ){

        writer_close(restrm);

        reader_start(restrm);

        retcd = writer_init(restrm);
        if (retcd < 0){
            fprintf(stderr,"%s: Failed to open the new connection\n"
                ,restrm->guide_info->guide_displayname);
        }

        reader_close(restrm);

        fprintf(stderr,"%s: Connection closed\n",restrm->guide_info->guide_displayname);
        restrm->pipe_state = PIPE_IS_CLOSED;

    } else if (restrm->pipe_state == PIPE_IS_CLOSED) {
        output_checkpipe(restrm);
        if (restrm->pipe_state == PIPE_IS_OPEN) {
            /* If was closed and now is being indicated as open
             * then we have a new connection.
             */
            retcd = writer_init_open(restrm);
            if (retcd < 0){
                fprintf(stderr,"%s: Failed to open the new connection\n"
                    ,restrm->guide_info->guide_displayname);
                restrm->pipe_state = PIPE_NEEDS_RESET;
            }
            fprintf(stderr,"%s: New Connection\n",restrm->guide_info->guide_displayname);
            restrm->connect_start = av_gettime_relative();
        }
    }
}

void *process_playlist(void *parms){

    ctx_restream *restrm = parms;
    int retcd, finish_playlist;

    snprintf(restrm->function_name,1024,"%s","process_playlist");
    pthread_setname_np(pthread_self(), "ProcessPlaylist");

    thread_count++;

    fprintf(stderr, "%s: Process playlist %d\n"
        ,restrm->guide_info->guide_displayname,thread_count);

    restrm->watchdog_playlist = av_gettime_relative();

    reader_init(restrm);

    finish_playlist = 0;
    while (!finish_playlist){
        retcd = playlist_loaddir(restrm);
        if (retcd == -1) finish_playlist = 1;

        if (restrm->playlist_index >= restrm->playlist_count) restrm->playlist_index = 0;

        while ((restrm->playlist_index < restrm->playlist_count) && (!finish_playlist)) {
            restrm->in_filename = restrm->playlist[restrm->playlist_index].movie_path;

            retcd = infile_init(restrm);
            if (retcd == 0){
                reader_start(restrm);
                retcd = writer_init(restrm);
                reader_close(restrm);
            } else {
                infile_close(restrm);
            }

            if (retcd == 0){
                restrm->watchdog_playlist = av_gettime_relative();
                fprintf(stderr,"%s: Playing: %s \n"
                    ,restrm->guide_info->guide_displayname
                    ,restrm->in_filename);
                restrm->connect_start = av_gettime_relative();
                while (!finish) {

                    av_packet_unref(&restrm->pkt);
                    av_init_packet(&restrm->pkt);
                    restrm->pkt.data = NULL;
                    restrm->pkt.size = 0;

                    restrm->watchdog_playlist = av_gettime_relative();
                    retcd = av_read_frame(restrm->ifmt_ctx, &restrm->pkt);
                    if (retcd < 0) break;

                    infile_wait(restrm);

                    output_pipestatus(restrm);
                    if (restrm->pipe_state == PIPE_IS_OPEN) writer_packet(restrm);

                    restrm->watchdog_playlist = av_gettime_relative();

                }
                if (finish) finish_playlist = 1;
            }

            restrm->watchdog_playlist = av_gettime_relative();

            writer_close(restrm);

            infile_close(restrm);

            restrm->playlist_index ++;
            restrm->watchdog_playlist = av_gettime_relative();
            if (finish) finish_playlist = 1;
        }
    }

    reader_end(restrm);

    if (playlist_free(restrm) != 0){
        printf("%s: Process playlist exit abnormal %d\n"
            ,restrm->guide_info->guide_displayname,thread_count);
        thread_count--;
        restrm->finish = 1;
        pthread_exit(NULL);
    } else {
        //printf("%s: Process playlist exit %d\n"
        //    ,restrm->guide_info->guide_displayname,thread_count);
        thread_count--;
        restrm->finish = 1;
        pthread_exit(NULL);
    }

}

void *channel_process(void *parms){

    struct channel_item *chn_item;
    ctx_restream *restrm;
    pthread_attr_t handler_attribute;

    pthread_setname_np(pthread_self(), "channel_process");
    thread_count++;
    //fprintf(stderr,"channel_process %d\n",thread_count);

    chn_item = parms;

    restrm = malloc(sizeof(ctx_restream));
    memset(restrm,'\0',sizeof(ctx_restream));
    restrm->guide_info = NULL;

    chn_item->channel_status= 1;
    restrm->finish = 0;
    restrm->function_name = malloc(1024);
    restrm->playlist_dir = malloc((strlen(chn_item->channel_dir)+2)*sizeof(char));
    restrm->out_filename = malloc((strlen(chn_item->channel_pipe)+2)*sizeof(char));
    restrm->playlist_sort_method = malloc((strlen(chn_item->channel_order)+2)*sizeof(char));

    strcpy(restrm->playlist_dir,chn_item->channel_dir);
    strcpy(restrm->out_filename,chn_item->channel_pipe);
    strcpy(restrm->playlist_sort_method,chn_item->channel_order);

    guide_init(restrm);

    guide_names_guide(restrm);

    restrm->playlist_count = 0;
    restrm->pipe_state = PIPE_IS_CLOSED;
    restrm->reader_status = READER_STATUS_INACTIVE;
    restrm->playlist_index = 0;
    restrm->soft_restart = 1;

    av_init_packet(&restrm->pkt);
    restrm->pkt.data = NULL;
    restrm->pkt.size = 0;

    restrm->watchdog_playlist = av_gettime_relative();

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);
    pthread_create(&restrm->process_playlist_thread, &handler_attribute, process_playlist, restrm);

    while (!restrm->finish){
        if ((av_gettime_relative() - restrm->watchdog_playlist) > 5000000){

            if (restrm->soft_restart == 1){
                fprintf(stderr,"%s: Watchdog soft: %s\n"
                    ,restrm->guide_info->guide_displayname
                    ,restrm->function_name);
                restrm->pipe_state = PIPE_NEEDS_RESET;

                reader_flush(restrm);

                fprintf(stderr,"%s: Flushed Watchdog soft: %s\n"
                    ,restrm->guide_info->guide_displayname
                    ,restrm->function_name);

                restrm->watchdog_playlist = av_gettime_relative();
                restrm->soft_restart = 0;
            } else {

                pthread_cancel(restrm->process_playlist_thread);
                thread_count--;
                fprintf(stderr,"%s:  >Watchdog hard< %d %s\n"
                    ,restrm->guide_info->guide_displayname
                    ,thread_count
                    ,restrm->guide_info->guide_displayname);
                restrm->watchdog_playlist = av_gettime_relative();
                restrm->playlist_index ++;
                pthread_create(&restrm->process_playlist_thread
                    , &handler_attribute, process_playlist, restrm);
                restrm->soft_restart = 1;
            }
        }
        sleep(1);
    }

    pthread_attr_destroy(&handler_attribute);

    //fprintf(stderr,"%s: Channel process exit %d\n"
    //    ,restrm->guide_info->guide_displayname,thread_count);

    if (restrm->guide_info != NULL){
        free(restrm->guide_info->movie1_filename);
        free(restrm->guide_info->movie1_displayname);
        free(restrm->guide_info->movie2_filename);
        free(restrm->guide_info->movie2_displayname);
        free(restrm->guide_info->guide_filename);
        free(restrm->guide_info->guide_displayname);
        free(restrm->guide_info);
    }
    free(restrm->playlist_dir);
    free(restrm->out_filename);
    free(restrm->playlist_sort_method);
    free(restrm->function_name);

    av_packet_unref(&restrm->pkt);

    thread_count--;

    free(restrm);

    chn_item->channel_status= 0;

    pthread_exit(NULL);

}

int channels_init(char *parm_file){

    int indx;
    struct channel_context *channels;
    int channels_running;

    FILE *fp;
    char line_char[4096];
    int parm_index, parm_start, parm_end;
    int finish_channels;
    pthread_attr_t handler_attribute;

    channels = malloc(sizeof(struct channel_context));

    fp = fopen(parm_file, "r");
    if (fp == NULL){
        fprintf(stderr,"Unable to open parameter file\n");
        return -1;
    }

    channels->channel_count = 0;
    while (fgets(line_char, sizeof(line_char), fp) != NULL) {
        channels->channel_count++;
    }
    fclose(fp);

    if (channels->channel_count == 0){
        fprintf(stderr,"Parameter file seems to be empty.\n");
        return -1;
    }

    channels->channel_info = malloc(sizeof(struct channel_item) * channels->channel_count);

    for (indx=0; indx < channels->channel_count; indx++){
        channels->channel_info[indx].channel_dir = NULL;
        channels->channel_info[indx].channel_pipe = NULL;
        channels->channel_info[indx].channel_order = NULL;
    }

    fp = fopen(parm_file, "r");
    if (fp == NULL){
        fprintf(stderr,"This is weird, unable to open parameter file on second try.\n");
        return -1;
    }

    indx = 0;
    while (fgets(line_char, sizeof(line_char), fp) != NULL) {
        parm_end = -1;
        for(parm_index=1; parm_index <= 3; parm_index++){
            parm_start = parm_end + 1;
            while (parm_start < sizeof(line_char)) {
                if (line_char[parm_start] == '\"' ) break;
                    parm_start ++;
            }
            parm_end = parm_start + 1;
            while (parm_end < sizeof(line_char)) {
                if (line_char[parm_end] == '\"') break;
                    parm_end ++;
            }

            if (parm_end >= (int)sizeof(line_char) ) {
                if (parm_index >= 3) free(channels->channel_info[2].channel_pipe);
                if (parm_index >= 2) free(channels->channel_info[1].channel_dir);
            } else {
                switch (parm_index){
                case 1:
                    channels->channel_info[indx].channel_dir = malloc((parm_end - parm_start));
                    memset(channels->channel_info[indx].channel_dir,'\0',(parm_end - parm_start));
                    strncpy(channels->channel_info[indx].channel_dir, line_char + parm_start + 1 , parm_end - parm_start -1 );
                    break;
                case 2:
                    channels->channel_info[indx].channel_pipe = malloc((parm_end - parm_start));
                    memset(channels->channel_info[indx].channel_pipe,'\0',(parm_end - parm_start));
                    strncpy(channels->channel_info[indx].channel_pipe, line_char + parm_start+1 , parm_end - parm_start -1 );
                    break;
                case 3:
                    channels->channel_info[indx].channel_order = malloc((parm_end - parm_start));
                    memset(channels->channel_info[indx].channel_order,'\0',(parm_end - parm_start));
                    strncpy(channels->channel_info[indx].channel_order, line_char + parm_start + 1 , parm_end - parm_start - 1 );
                    break;
                }
            }
        }
        indx ++;
    }

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);

    for(indx=0; indx < channels->channel_count ; indx++){
        channels->channel_info[indx].channel_status = 1;
        pthread_create(&channels->channel_info[indx].process_channel_thread
            , &handler_attribute, channel_process, &channels->channel_info[indx]);
    }
    pthread_attr_destroy(&handler_attribute);

    finish_channels = 0;
    while (!finish_channels){
        channels_running = 0;
        for(indx=0; indx < channels->channel_count; indx++){
           channels_running = channels_running + channels->channel_info[indx].channel_status;
        }
        sleep(1);
        if (channels_running == 0) finish_channels = 1;
    }

    for (indx=0; indx < channels->channel_count; indx++){
        if (channels->channel_info[indx].channel_dir != NULL){
            free(channels->channel_info[indx].channel_dir);
        }
        if(channels->channel_info[indx].channel_pipe != NULL){
            free(channels->channel_info[indx].channel_pipe);
        }
        if (channels->channel_info[indx].channel_order != NULL){
            free(channels->channel_info[indx].channel_order);
        }
    }
    free(channels->channel_info);
    free(channels);

    //printf("Exit channels_init normal \n");
    return 0;
}

void ffavlogger(void *var1, int ffav_errnbr, const char *fmt, va_list vlist){
    char buff[1024];

    vsnprintf(buff,sizeof(buff),fmt, vlist);
    if (ffav_errnbr < AV_LOG_FATAL){
        fprintf(stderr,"ffmpeg error %s \n",buff);
    }

}

int main(int argc, char **argv){

    char *parameter_file;
    int   retcd, wait_cnt;

    if (argc < 2) {
        printf("No parameter file specified.  Using testing version. %s \n", argv[0]);
        parameter_file = "./testall.txt";
    } else {
        parameter_file  = argv[1];
    }

    finish = 0;

    signal_setup();

    av_register_all();

    av_log_set_callback((void *)ffavlogger);

    retcd = 0;
    retcd = channels_init(parameter_file);

    wait_cnt = 0;
    while ((thread_count != 0) && (wait_cnt <=500)){
        SLEEP(0,10000000L);
        wait_cnt++;
    }

    if (wait_cnt < 500){
        printf("Exit Normal \n");
    } else {
        printf("Exit.  Waited %d for threads %d \n",wait_cnt, thread_count);
    }

    return retcd;
}
