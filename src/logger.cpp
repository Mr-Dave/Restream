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

const char *log_level_str[] = {NULL, "EMG", "ALR", "CRT", "ERR", "WRN", "NTC", "INF", "DBG", "ALL"};

void cls_log::write_flood(int loglvl)
{
    char flood_repeats[1024];

    if (app->log->flood_cnt <= 1) {
        return;
    }

    snprintf(flood_repeats, sizeof(flood_repeats)
        , "%s Above message repeats %d times\n"
        , app->log->msg_prefix, app->log->flood_cnt-1);

    if (app->log->log_mode == LOGMODE_FILE) {
        fputs(flood_repeats, app->log->log_file_ptr);
        fflush(app->log->log_file_ptr);

    } else {    /* The syslog level values are one less*/
        syslog(loglvl, "%s", flood_repeats);
        fputs(flood_repeats, stderr);
        fflush(stderr);
    }
}

void cls_log::write_norm(int loglvl, int prefixlen)
{
    app->log->flood_cnt = 1;

    snprintf(app->log->msg_flood
        , sizeof(app->log->msg_flood), "%s"
        , &app->log->msg_full[16]);

    snprintf(app->log->msg_prefix, prefixlen, "%s", app->log->msg_full);

    if (app->log->log_mode == LOGMODE_FILE) {
        strcpy(app->log->msg_full +
            strlen(app->log->msg_full),"\n");
        fputs(app->log->msg_full, app->log->log_file_ptr);
        fflush(app->log->log_file_ptr);

    } else {
        syslog(loglvl-1, "%s", app->log->msg_full);
        strcpy(app->log->msg_full +
            strlen(app->log->msg_full),"\n");
        fputs(app->log->msg_full, stderr);
        fflush(stderr);

    }
}

void cls_log::add_errmsg(int flgerr, int err_save)
{
    int errsz, msgsz;
    char err_buf[90];

    if (flgerr == NO_ERRNO) {
        return;
    }

    memset(err_buf, 0, sizeof(err_buf));
    strerror_r(err_save, err_buf, sizeof(err_buf));
    errsz = strlen(err_buf);
    msgsz = strlen(app->log->msg_full);

    if ((msgsz+errsz+2) >= (int)sizeof(app->log->msg_full)) {
        msgsz = msgsz-errsz-2;
        memset(app->log->msg_full+msgsz, 0, sizeof(app->log->msg_full) - msgsz);
    }
    strcpy(app->log->msg_full+msgsz,": ");
    memcpy(app->log->msg_full+msgsz + 2, err_buf, errsz);

}

void cls_log::set_mode(int mode_new)
{
    if ((log_mode != LOGMODE_SYSLOG) && (mode_new == LOGMODE_SYSLOG)) {
        openlog("restream", LOG_PID, LOG_USER);
    }
    if ((log_mode == LOGMODE_SYSLOG) && (mode_new != LOGMODE_SYSLOG)) {
        closelog();
    }
    log_mode = mode_new;
}

void cls_log::set_log_file(std::string pname)
{
    if ((pname == "") || (pname == "syslog")) {
        if (log_file_ptr != nullptr) {
            myfclose(log_file_ptr);
            log_file_ptr = nullptr;
        }
        if (log_file_name == "") {
            set_mode(LOGMODE_SYSLOG);
            log_file_name == "syslog";
            LOG_MSG(NTC, NO_ERRNO, "Logging to syslog");
        }

    } else if ((pname != log_file_name) || (log_file_ptr == nullptr)) {
        if (log_file_ptr != nullptr) {
            myfclose(log_file_ptr);
            log_file_ptr = nullptr;
        }
        log_file_ptr = myfopen(pname.c_str(), "ae");
        if (log_file_ptr != nullptr) {
            log_file_name = pname;
            set_mode(LOGMODE_SYSLOG);
            LOG_MSG(NTC, NO_ERRNO, "Logging to file (%s)"
                , app->conf->log_file.c_str());
            set_mode(LOGMODE_FILE);
        } else {
            log_file_name = "syslog";
            set_mode(LOGMODE_SYSLOG);
            LOG_MSG(EMG, SHOW_ERRNO, "Cannot create log file %s"
                , app->conf->log_file.c_str());
        }
    }
}

cls_log::cls_log()
{
    log_mode = LOGMODE_NONE;
    log_level = LEVEL_DEFAULT;
    log_file_ptr  = nullptr;
    log_file_name = "";
    flood_cnt = 0;
    set_mode(LOGMODE_SYSLOG);
    pthread_mutex_init(&mtx, NULL);
    memset(msg_prefix,0,sizeof(msg_prefix));
    memset(msg_flood,0,sizeof(msg_flood));
    memset(msg_full,0,sizeof(msg_full));

}

cls_log::~cls_log()
{
    if (log_file_ptr != nullptr) {
        LOG_MSG(NTC, NO_ERRNO, "Closing log_file (%s)."
            , app->conf->log_file.c_str());
        myfclose(log_file_ptr);
        log_file_ptr = nullptr;
    }
    pthread_mutex_destroy(&mtx);
}

void cls_log::write_msg(int loglvl, int flgerr, bool flgfnc, const char *fmt, ...)
{
    int err_save, n, prefixlen;
    char usrfmt[1024];
    char msg_time[16];
    char threadname[32];
    va_list ap;
    time_t now;

    if (loglvl > app->log->log_level) {
        return;
    }

    pthread_mutex_lock(&mtx);

    err_save = errno;
    memset(app->log->msg_full, 0, sizeof(app->log->msg_full));
    memset(usrfmt, 0, sizeof(usrfmt));

    mythreadname_get(threadname);

    now = time(NULL);
    strftime(msg_time, sizeof(msg_time)
        , "%b %d %H:%M:%S", localtime(&now));

    n = snprintf(app->log->msg_full
        , sizeof(app->log->msg_full)
        , "%s [%s][%s] ", msg_time
        , log_level_str[loglvl], threadname );
    prefixlen = n;

    if (flgfnc) {
        va_start(ap, fmt);
            prefixlen += snprintf(usrfmt, sizeof(usrfmt)
                , "%s: ", va_arg(ap, char *));
        va_end(ap);
        snprintf(usrfmt, sizeof (usrfmt),"%s: %s", "%s", fmt);
    } else {
        snprintf(usrfmt, sizeof (usrfmt),"%s",fmt);
    }

    va_start(ap, fmt);
        n += vsnprintf(
            app->log->msg_full + n
            , sizeof(app->log->msg_full)-n-1
            , usrfmt, ap);
    va_end(ap);

    app->log->add_errmsg(flgerr, err_save);

    /*
      Compare the part of the message that is
      after the message time (time is 16 bytes)
    */
    if ((app->log->flood_cnt <= 5000) &&
        mystreq(app->log->msg_flood, &app->log->msg_full[16])) {
        app->log->flood_cnt++;
        pthread_mutex_unlock(&mtx);
        return;
    }

    app->log->write_flood(loglvl);

    app->log->write_norm(loglvl, prefixlen);

    pthread_mutex_unlock(&mtx);

}
