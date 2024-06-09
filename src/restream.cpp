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
#include "channel.hpp"
#include "infile.hpp"
#include "pktarray.hpp"
#include "webu.hpp"

cls_app *app;

static void signal_handler(int signo)
{
    switch(signo) {
    case SIGALRM:
        fprintf(stderr, "Caught alarm signal.\n");
        break;
    case SIGINT:
        fprintf(stderr, "Caught interrupt signal.\n");
        app->finish = true;
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

void cls_app::signal_setup()
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

void cls_app::channels_start()
{
    int indx;
    cls_channel *chitm;
    std::list<std::string>::iterator    it;
    std::thread ch_thread;

    app->ch_count = 0;
    for (it  = app->conf->channels.begin();
         it != app->conf->channels.end(); it++) {
        chitm = new cls_channel(app->ch_count, it->c_str());
        app->channels.push_back(chitm);
        app->ch_count++;
    }

    for (indx=0; indx < app->ch_count; indx++) {
        ch_thread = std::thread(&cls_channel::process, app->channels[indx]);
        ch_thread.detach();
    }
    if (app->ch_count == 0) {
        LOG_MSG(NTC, NO_ERRNO,"Configuration file lacks channel parameters");
    }
}

void cls_app::channels_wait()
{
    int p_count, indx, chk;

    p_count = app->ch_count;
    chk = 0;
    while (p_count != 0){
        sleep(1);
        if (app->finish) {
            LOG_MSG(NTC, NO_ERRNO,"Closing web interface connections");
            app->webcontrol_finish = true;
            chk = 0;
            p_count = 1;
            while ((chk < 5) && (p_count > 0)) {
                p_count = 0;
                for (indx=0; indx < app->ch_count; indx++) {
                    p_count += app->channels[indx]->cnct_cnt;
                }
                if (p_count > 0) {
                    sleep(1);
                }
                chk++;
            }
            if (chk >= 5) {
                LOG_MSG(NTC, NO_ERRNO,"Excessive wait for webcontrol shutdown");
            } else {
                LOG_MSG(NTC, NO_ERRNO,"Web connections closed. Waited %d", chk);
            }

            LOG_MSG(NTC, NO_ERRNO,"Closing channels");
            for (indx=0; indx < app->ch_count; indx++) {
                app->channels[indx]->ch_finish = true;
            }

            chk = 0;
            p_count = 1;
            while ((chk < 5) && (p_count > 0)) {
                p_count = 0;
                for (indx=0; indx < app->ch_count; indx++) {
                    if (app->channels[indx]->ch_running == true) {
                        p_count++;
                    }
                }
                if (p_count > 0) {
                    sleep(1);
                }
                chk++;
            }
            if (chk >= 5) {
                LOG_MSG(NTC, NO_ERRNO,"Excessive wait for shutdown");
                p_count =0;
            } else {
                LOG_MSG(NTC, NO_ERRNO,"Channels closed. Waited %d", chk);
            }
        }
    }

}

void logger(void *var1, int errnbr, const char *fmt, va_list vlist)
{
    char buff[1024];
    (void)var1;

    vsnprintf(buff, sizeof(buff), fmt, vlist);

    buff[strlen(buff)-1] = 0;

    if (strstr(buff, "forced frame type") != nullptr) {
        return;
    }

    if (errnbr < AV_LOG_VERBOSE) {
        LOG_MSG(INF, NO_ERRNO,"ffmpeg message: %s",buff );
    }

    return;

    if (errnbr < AV_LOG_FATAL) {
        LOG_MSG(INF, NO_ERRNO,"ffmpeg message: %s", buff);
    }
    if (errnbr < AV_LOG_TRACE) {
        LOG_MSG(INF, NO_ERRNO,"ffmpeg message: %s", buff);
    }

}

int main(int argc, char **argv)
{
    mythreadname_set(nullptr,1,"main");

    app = new cls_app(argc, argv);
    app->log = new cls_log();
    app->conf = new cls_config();
    app->conf->parms_log();

    app->channels_start();

    webu_init();

    app->channels_wait();

    webu_deinit();

    delete app;

    return 0;
}

cls_app::cls_app(int p_argc, char **p_argv)
{
    argc = p_argc;
    argv = p_argv;

    finish = false;

    signal_setup();

    av_log_set_callback(logger);

}

cls_app::~cls_app()
{
    int indx;

    for (indx=0; indx < app->ch_count; indx++) {
        delete app->channels[indx];
    }

    delete conf;
    delete log;
}