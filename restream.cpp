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
#include "guide.hpp"
#include "playlist.hpp"
#include "infile.hpp"
#include "reader.hpp"
#include "writer.hpp"

volatile int thread_count;
volatile int finish;


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
    ctx_restream *restrm = (ctx_restream *)ctx;

    snprintf(restrm->function_name,1024,"%s","restrm_interrupt");

    /* This needs to be worked */

    return FALSE;
    (void)(restrm->ofmt);

    return FALSE;
}


void output_checkpipe(ctx_restream *restrm){

    int pipefd;

    if (finish == TRUE) return;
    snprintf(restrm->function_name,1024,"%s","output_checkpipe");

    pipefd = open(restrm->out_filename, O_WRONLY | O_NONBLOCK);
    if (pipefd == -1 ) {
        if (errno == ENXIO) {
            restrm->pipe_state = PIPE_IS_CLOSED;
        } else if (errno == ENOENT) {
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

    if (finish == TRUE) return;
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
             * then we have a new connection. */
            retcd = writer_init_open(restrm);
            if (retcd < 0){
                fprintf(stderr,"%s: Failed to open the new connection\n"
                    ,restrm->guide_info->guide_displayname);
                restrm->pipe_state = PIPE_NEEDS_RESET;
                return;
            }
            fprintf(stderr,"%s: New connection\n"
                ,restrm->guide_info->guide_displayname);
            restrm->connect_start = av_gettime_relative();
            restrm->pipe_state = PIPE_IS_OPEN;
        }
    }
}

void *process_playlist(void *parms){

    ctx_restream *restrm = (ctx_restream *)parms;
    int retcd, finish_playlist;

    snprintf(restrm->function_name,1024,"%s","process_playlist");
    pthread_setname_np(pthread_self(), "ProcessPlaylist");

    thread_count++;

    fprintf(stderr, "%s: Process playlist %d\n"
        ,restrm->guide_info->guide_displayname,thread_count);

    restrm->watchdog_playlist = av_gettime_relative();

    reader_init(restrm);

    finish_playlist = 0;
    restrm->connect_start = 0;

    while (finish_playlist == FALSE) {
        retcd = playlist_loaddir(restrm);
        if (retcd == -1) finish_playlist = 1;

        if (restrm->playlist_index >= restrm->playlist_count) restrm->playlist_index = 0;

        while ((restrm->playlist_index < restrm->playlist_count) && (!finish_playlist)) {
            restrm->in_filename = restrm->playlist[restrm->playlist_index].movie_path;

            retcd = infile_init(restrm);

            if (retcd == 0){
                restrm->watchdog_playlist = av_gettime_relative();
                fprintf(stderr,"%s: Playing: %s \n"
                    ,restrm->guide_info->guide_displayname
                    ,restrm->in_filename);
                restrm->connect_start = av_gettime_relative();
                while (finish == FALSE) {
                    av_packet_free(&restrm->pkt);
                    restrm->pkt = av_packet_alloc();

                    restrm->pkt->data = NULL;
                    restrm->pkt->size = 0;

                    restrm->watchdog_playlist = av_gettime_relative();
                    retcd = av_read_frame(restrm->ifmt_ctx, restrm->pkt);
                    if (retcd < 0) break;

                    infile_wait(restrm);

                    output_pipestatus(restrm);
                    if (restrm->pipe_state == PIPE_IS_OPEN) writer_packet(restrm);

                    if (restrm->ifmt_ctx == NULL) break;

                    restrm->watchdog_playlist = av_gettime_relative();

                }
            }

            restrm->watchdog_playlist = av_gettime_relative();

            infile_close(restrm);

            restrm->playlist_index ++;
            restrm->watchdog_playlist = av_gettime_relative();
            if (finish == TRUE) finish_playlist = TRUE;
        }
    }

    infile_close(restrm);

    writer_close(restrm);

    reader_end(restrm);

    if (playlist_free(restrm) != 0){
        printf("%s: Process playlist exit abnormal %d\n"
            ,restrm->guide_info->guide_displayname,thread_count);
        thread_count--;
        restrm->finish_thread = TRUE;
        pthread_exit(NULL);
    } else {
        //printf("%s: Process playlist exit %d\n"
        //    ,restrm->guide_info->guide_displayname,thread_count);
        thread_count--;
        restrm->finish_thread = TRUE;
        pthread_exit(NULL);
    }

}

void *channel_process(void *parms){

    int retcd;
    struct channel_item *chn_item;
    ctx_restream *restrm;
    pthread_attr_t handler_attribute;

    pthread_setname_np(pthread_self(), "channel_process");
    thread_count++;
    //fprintf(stderr,"channel_process %d\n",thread_count);

    chn_item = (channel_item *)parms;

    restrm = (ctx_restream*)calloc(1, sizeof(ctx_restream));
    memset(restrm,'\0',sizeof(ctx_restream));
    restrm->guide_info = NULL;

    chn_item->channel_status= 1;
    restrm->finish_thread = FALSE;
    restrm->function_name = (char*)calloc(1,1024);
    restrm->playlist_dir = (char*)calloc((strlen(chn_item->channel_dir)+2),sizeof(char));
    restrm->out_filename = (char*)calloc((strlen(chn_item->channel_pipe)+2),sizeof(char));
    restrm->playlist_sort_method = (char*)calloc((strlen(chn_item->channel_order)+2),sizeof(char));
    restrm->rand_seed = chn_item->channel_seed;

    retcd = snprintf(restrm->playlist_dir,strlen(chn_item->channel_dir)+1,"%s",chn_item->channel_dir);
    if (retcd < 0){
      fprintf(stderr,"Error writing to restream playlist dir %d\n",thread_count);
    }

    retcd = snprintf(restrm->out_filename,strlen(chn_item->channel_pipe)+1,"%s",chn_item->channel_pipe);
    if (retcd < 0){
      fprintf(stderr,"Error writing to restream playlist dir %d\n",thread_count);
    }

    retcd = snprintf(restrm->playlist_sort_method,strlen(chn_item->channel_order)+1,"%s",chn_item->channel_order);
    if (retcd < 0){
      fprintf(stderr,"Error writing to restream playlist dir %d\n",thread_count);
    }

    guide_init(restrm);

    guide_names_guide(restrm);

    restrm->playlist_count = 0;
    restrm->pipe_state = PIPE_IS_CLOSED;
    restrm->reader_status = READER_STATUS_INACTIVE;
    restrm->playlist_index = 0;
    restrm->soft_restart = 1;

    restrm->ts_base.audio = 1;
    restrm->ts_base.video = 1;
    restrm->ts_out.audio = 1;
    restrm->ts_out.video = 1;

    restrm->pkt = av_packet_alloc();
    restrm->pkt->data = NULL;
    restrm->pkt->size = 0;

    restrm->watchdog_playlist = av_gettime_relative();

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);
    pthread_create(&restrm->process_playlist_thread, &handler_attribute, process_playlist, restrm);

    while (restrm->finish_thread == FALSE){
        if ((av_gettime_relative() - restrm->watchdog_playlist) > 50000000){

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

    av_packet_free(&restrm->pkt);

    thread_count--;

    free(restrm);

    chn_item->channel_status= 0;

    pthread_exit(NULL);

}

static void channels_free(struct channel_context *channels, int indx_max)
{
    int indx;

    for (indx=0; indx < indx_max; indx++){
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
}

int channels_init(const char *parm_file){

    int indx, retcd;
    struct channel_context *channels;
    int channels_running;

    FILE *fp;
    char line_char[4096];
    char p1[4096], p2[4096], p3[4096];
    int parm_index, parm_start, parm_end;
    int finish_channels;
    pthread_attr_t handler_attribute;
    struct timeval tv;
    unsigned int usec;

    gettimeofday(&tv,NULL);
    usec = (unsigned int)(tv.tv_usec / 10000) ;

    fp = fopen(parm_file, "r");
    if (fp == NULL){
        fprintf(stderr,"Unable to open parameter file\n");
        return -1;
    }

    indx = 0;
    channels =(channel_context*)malloc(sizeof(struct channel_context));
    channels->channel_info =(channel_item*)malloc(sizeof(struct channel_item));
    channels->channel_count = 0;
    memset(line_char, 0, 4096);

    while (fgets(line_char, sizeof(line_char), fp) != NULL) {
        memset(p1, 0, 4096);
        memset(p2, 0, 4096);
        memset(p3, 0, 4096);

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

            if (parm_end < (int)sizeof(line_char) ) {
                switch (parm_index) {
                case 1:
                    retcd = snprintf(p1, parm_end - parm_start, "%s", &line_char[parm_start + 1]);
                    break;
                case 2:
                    retcd = snprintf(p2, parm_end - parm_start, "%s", &line_char[parm_start + 1]);
                    break;
                case 3:
                    retcd = snprintf(p3, parm_end - parm_start, "%s", &line_char[parm_start + 1]);
                    break;
                }
            }
        }

        if (strlen(p1) > 0 && strlen(p2) > 0 && strlen(p3) > 0) {

            channels->channel_info =(channel_item*) realloc(channels->channel_info,sizeof(struct channel_item)*(indx+1));

            channels->channel_info[indx].channel_dir =(char*)calloc(strlen(p1) + 1, sizeof(char));
            sprintf(channels->channel_info[indx].channel_dir, "%s", p1);

            channels->channel_info[indx].channel_pipe =(char*) calloc(strlen(p2) + 1, sizeof(char));
            sprintf(channels->channel_info[indx].channel_pipe, "%s", p2);

            channels->channel_info[indx].channel_order =(char*) calloc(strlen(p3) + 1, sizeof(char));
            sprintf(channels->channel_info[indx].channel_order,"%s", p3);

            channels->channel_info[indx].channel_seed = rand_r(&usec);

            indx ++;
        }
    }
    channels->channel_count = indx;

    if (channels->channel_count == 0){
        fprintf(stderr,"Parameter file seems to be empty.\n");
        channels_free(channels, 0);
        return -1;
    }

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);

    for(indx=0; indx < channels->channel_count ; indx++){
        channels->channel_info[indx].channel_status = 1;
        pthread_create(&channels->channel_info[indx].process_channel_thread
            , &handler_attribute, channel_process, &channels->channel_info[indx]);
    }
    pthread_attr_destroy(&handler_attribute);

    finish_channels = FALSE;
    while (finish_channels == FALSE){
        channels_running = 0;
        for(indx=0; indx < channels->channel_count; indx++){
           channels_running = channels_running + channels->channel_info[indx].channel_status;
        }
        sleep(1);
        if (channels_running == 0) finish_channels = TRUE;
    }

    channels_free(channels, channels->channel_count);

    //printf("Exit channels_init normal \n");
    return 0;
}

void logger(void *var1, int ffav_errnbr, const char *fmt, va_list vlist){
    char buff[1024];

    vsnprintf(buff,sizeof(buff),fmt, vlist);
    if (ffav_errnbr < AV_LOG_FATAL){
        fprintf(stderr,"ffmpeg error %s \n",buff);
    }
}

int main(int argc, char **argv){

    std::string parameter_file;
    int   retcd, wait_cnt;

    if (argc < 2) {
        printf("No parameter file specified.  Using testing version. %s \n", argv[0]);
        parameter_file = "./testall.txt";
    } else {
        parameter_file  = argv[1];
    }

    finish = 0;

    signal_setup();

    av_log_set_callback(logger);

    retcd = 0;
    retcd = channels_init(parameter_file.c_str());

    wait_cnt = 0;
    while ((thread_count != 0) && (wait_cnt <=50000)){
        SLEEP(0,100000L);
        wait_cnt++;
    }

    if (wait_cnt < 50000){
        printf("Exit Normal \n");
    } else {
        printf("Exit.  Waited %d for threads %d \n",wait_cnt, thread_count);
    }

    return retcd;
}
