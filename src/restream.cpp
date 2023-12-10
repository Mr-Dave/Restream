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
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "guide.hpp"
#include "infile.hpp"
#include "webu.hpp"

bool finish;

void signal_handler(int signo)
{
    switch(signo) {
    case SIGALRM:
        fprintf(stderr, "Caught alarm signal.\n");
        break;
    case SIGINT:
        fprintf(stderr, "Caught interrupt signal.\n");
        finish = true;
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

void signal_setup()
{
    if (signal(SIGPIPE,   signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch pipe signal.\n");
    if (signal(SIGALRM,   signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch alarm signal.\n");
    if (signal(SIGTERM,   signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch term signal.\n");
    if (signal(SIGQUIT,   signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch quit signal.\n");
    if (signal(SIGHUP,    signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch hup signal.\n");
    if (signal(SIGABRT,   signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch abort signal.\n");
    if (signal(SIGVTALRM, signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch VTalarm\n");
    if (signal(SIGINT,    signal_handler) == SIG_ERR)  fprintf(stderr, "Can not catch VTalarm\n");
}

bool playlist_cmp(const ctx_playlist_item& a, const ctx_playlist_item& b)
{
/*
    if (a.fullnm < b.fullnm) {
        LOG_MSG(NTC, NO_ERRNO
            ,"less %s %s", a.fullnm, b.fullnm);
    } else {
        LOG_MSG(NTC, NO_ERRNO
            ,"greater %s %s", a.fullnm, b.fullnm);
    }
*/
    return a.fullnm < b.fullnm;
}

void playlist_loaddir(ctx_channel_item *chitm)
{
    DIR           *d;
    struct dirent *dir;
    ctx_playlist_item playitm;

    if (finish == true) {
        return;
    }

    chitm->playlist_count = 0;
    chitm->playlist.clear();
    d = opendir(chitm->ch_dir.c_str());
    if (d) {
        while ((dir=readdir(d)) != NULL){
            if ((strstr(dir->d_name,".mkv") != NULL) || (strstr(dir->d_name,".mp4") != NULL)) {
                chitm->playlist_count++;
                playitm.fullnm = chitm->ch_dir;
                playitm.fullnm += dir->d_name;
                playitm.filenm = dir->d_name;
                playitm.displaynm = playitm.filenm.substr(
                    0, playitm.filenm.find_last_of("."));
                chitm->playlist.push_back(playitm);
                }
            }
    }
    closedir(d);

    if (chitm->ch_sort == "alpha") {
        std::sort(chitm->playlist.begin(), chitm->playlist.end(), playlist_cmp);
    } else {
        std::random_shuffle(chitm->playlist.begin(), chitm->playlist.end());
    }
}

void channel_process_setup(ctx_channel_item *chitm)
{
    std::list<ctx_params_item>::iterator    it;

    chitm->ch_finish = false;
    chitm->ch_dir = "";
    chitm->ch_nbr = "";
    chitm->ch_sort = "";
    chitm->ch_running = false;
    chitm->ch_tvhguide = true;
    chitm->frame = nullptr;
    chitm->pktnbr = 0;
    chitm->pktarray_count = 0;
    chitm->pktarray_index = -1;
    chitm->pktarray_lastwritten = -1;
    chitm->pkt = nullptr;
    chitm->cnct_cnt = 0;
    chitm->file_cnt = 0;
    chitm->fifo = nullptr;

    pthread_mutex_init(&chitm->mtx_ifmt, NULL);
    pthread_mutex_init(&chitm->mtx_pktarray, NULL);

    util_parms_parse(
        chitm->ch_params
        , "ch"+std::to_string(chitm->ch_index)
        , chitm->ch_conf);

    for (it  = chitm->ch_params.params_array.begin();
         it != chitm->ch_params.params_array.end(); it++) {
        if (it->param_name == "dir") {
            chitm->ch_dir = it->param_value;
        }
        if (it->param_name == "ch") {
            chitm->ch_nbr = it->param_value;
        }
        if (it->param_name == "sort") {
            chitm->ch_sort = it->param_value;
        }
        if (it->param_name == "tvhguide") {
            conf_edit_set_bool(chitm->ch_tvhguide, it->param_value);
        }
    }

}

void channel_process_defaults(ctx_channel_item *chitm)
{
    chitm->ifile.audio.index = -1;
    chitm->ifile.audio.last_pts = -1;
    chitm->ifile.audio.start_pts = -1;
    chitm->ifile.audio.codec_ctx = nullptr;
    chitm->ifile.audio.strm = nullptr;
    chitm->ifile.audio.base_pdts = 0;
    chitm->ifile.video = chitm->ifile.audio;
    chitm->ifile.fmt_ctx = nullptr;
    chitm->ifile.time_start = -1;
    chitm->ofile = chitm->ifile;
}

void channel_process(ctx_app *app, int chindx)
{
    ctx_channel_item *chitm = &app->channels[chindx];
    int indx;

    chitm->ch_running = true;

    channel_process_setup(chitm);

    pthread_mutex_lock(&chitm->mtx_ifmt);

    while (chitm->ch_finish == false) {
        playlist_loaddir(chitm);
        for (indx=0; indx < chitm->playlist_count; indx++) {
            LOG_MSG(NTC, NO_ERRNO
                , "Playing: %s"
                , chitm->playlist[indx].filenm.c_str());
            chitm->playlist_index = indx;
            if (chitm->ch_tvhguide == true) {
                guide_process(chitm);
            }
            channel_process_defaults(chitm);
            decoder_init(chitm);
            encoder_init(chitm);
            decoder_get_ts(chitm);

            pthread_mutex_unlock(&chitm->mtx_ifmt);
            infile_read(chitm);
            pthread_mutex_lock(&chitm->mtx_ifmt);
            streams_close(chitm);
            if (chitm->ch_finish == true) {
                break;
            }
        }
    }

    for (indx=0; indx < (int)chitm->pktarray.size(); indx++) {
        if (chitm->pktarray[indx].packet != nullptr) {
            mypacket_free(chitm->pktarray[indx].packet);
            chitm->pktarray[indx].packet = nullptr;
        }
    }

    chitm->pktarray.clear();
    chitm->pktarray_count = 0;

    pthread_mutex_destroy(&chitm->mtx_ifmt);
    pthread_mutex_destroy(&chitm->mtx_pktarray);

    chitm->ch_running = false;
}

void channels_init(ctx_app *app)
{
    int indx;
    ctx_channel_item chitm;
    std::list<std::string>::iterator    it;
    std::thread ch_thread;

    app->ch_count = 0;
    for (it  = app->conf->channels.begin();
         it != app->conf->channels.end(); it++) {
        app->ch_count++;
        chitm.ch_conf = it->c_str();
        chitm.app = app;
        app->channels.push_back(chitm);
    }

    for (indx=0; indx < app->ch_count; indx++) {
        app->channels[indx].ch_index = indx;
        ch_thread = std::thread(channel_process, app, indx);
        ch_thread.detach();
    }
    if (app->ch_count == 0) {
        LOG_MSG(NTC, NO_ERRNO,"Configuration file lacks channel parameters");
    }

}

void channels_wait(ctx_app *app)
{
    int chcnt, indx, chk;

    chcnt = 1;
    chk = 0;
    while (chcnt != 0){
        sleep(1);
        if (finish) {
            chk++;
            for (indx=0; indx < app->ch_count; indx++) {
                app->channels[indx].ch_finish = true;
            }
            sleep(1);
            chcnt = 0;
            for (indx=0; indx < app->ch_count; indx++) {
                if (app->channels[indx].ch_running == true) {
                    chcnt++;
                }
            }
            if (chk >5) {
                LOG_MSG(NTC, NO_ERRNO,"Excessive wait for shutdown");
                chcnt =0;
            }
        }
    }

}

void logger(void *var1, int errnbr, const char *fmt, va_list vlist)
{
    char buff[1024];
    (void)var1;
    vsnprintf(buff,sizeof(buff),fmt, vlist);
    if (errnbr < AV_LOG_FATAL) {
        LOG_MSG(INF, NO_ERRNO,"ffmpeg message: %s", buff);
    }
}

int main(int argc, char **argv){

    ctx_app *app;

    finish = false;

    app = new ctx_app;
    app->argc = argc;
    app->argv = argv;
    app->conf = new ctx_config;

    mythreadname_set(nullptr,1,"main");

    signal_setup();
    conf_init(app);
    log_init(app);
    conf_parms_log(app);

    av_log_set_callback(logger);

    channels_init(app);

    webu_init(app);

    channels_wait(app);

    webu_deinit(app);

    delete app->conf;
    delete app;

    return 0;
}
