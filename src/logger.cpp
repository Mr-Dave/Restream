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

#include <stdarg.h>
#include <syslog.h>

#include "restream.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "guide.hpp"
#include "infile.hpp"
#include "webu.hpp"

static int log_mode = LOGMODE_SYSLOG;
static FILE *logfile  = NULL;
static int log_level = LEVEL_DEFAULT;

static const char *log_level_str[] = {NULL, "EMG", "ALR", "CRT", "ERR", "WRN", "NTC", "INF", "DBG", "ALL"};

/** Sets mode of logging, could be using syslog or files. */
static void log_set_mode(int mode)
{
    int prev_mode = log_mode;

    log_mode = mode;

    if (mode == LOGMODE_SYSLOG && prev_mode != LOGMODE_SYSLOG) {
        openlog("restream", LOG_PID, LOG_USER);
    }

    if (mode != LOGMODE_SYSLOG && prev_mode == LOGMODE_SYSLOG) {
        closelog();
    }
}

/** Sets logfile to be used instead of syslog. */
static void log_set_logfile(const char *logfile_name)
{
    log_set_mode(LOGMODE_SYSLOG);

    logfile = myfopen(logfile_name, "ae");

    /* If logfile was opened correctly */
    if (logfile) {
        log_set_mode(LOGMODE_FILE);
    }
}

/** Return string with human readable time */
static char *log_time(void)
{
    static char buffer[16];
    time_t now = 0;

    now = time(0);
    strftime(buffer, 16, "%b %d %H:%M:%S", localtime(&now));
    return buffer;
}

/* Print log message*/
void log_msg(int loglvl, int flgerr, bool flgfnc, const char *fmt, ...)
{

    int err_save, n, prefixlen;
    size_t buf_len;

    char buf[1024]= {0};
    char usrfmt[1024]= {0};
    char err_buf[100]= {0};
    char flood_repeats[1024];
    char threadname[32];

    static int flood_cnt = 0;
    static char flood_msg[1024];
    static char prefix_msg[512];

    va_list ap;

    if (loglvl > log_level) {
        return;
    }
    err_save = errno;

    mythreadname_get(threadname);
    /* Add time/level/threadname */
    n = snprintf(buf, sizeof(buf), "%s [%s][%s] "
        , log_time(), log_level_str[loglvl], threadname );
    prefixlen = n;

    /* Add format specifier for function name*/
    if (flgfnc) {
        va_start(ap, fmt);
            prefixlen += snprintf(usrfmt, sizeof(usrfmt),"%s: ", va_arg(ap, char *));
        va_end(ap);
        snprintf(usrfmt, sizeof (usrfmt),"%s: %s", "%s", fmt);
    } else {
        snprintf(usrfmt, sizeof (usrfmt),"%s",fmt);
    }

    /* Add user message*/
    va_start(ap, fmt);
        n += vsnprintf(buf + n, sizeof(buf) - n, usrfmt, ap);
    va_end(ap);
    buf[1023] = '\0';

    /* If error flag is set, add on the library error message. */
    if (flgerr) {
        buf_len = strlen(buf);
        // just knock off 10 characters if we're that close...
        if (buf_len + 10 > 1024) {
            buf[1024 - 10] = '\0';
            buf_len = 1024 - 10;
        }

        strncat(buf, ": ", 1024 - buf_len);
        n += 2;
        strncat(buf
            , strerror_r(err_save, err_buf, sizeof(err_buf))
            , 1024 - strlen(buf));
    }

    if ((mystreq(&buf[16], flood_msg)) && (flood_cnt <= 5000)) {
        flood_cnt++;
        return;
    }

    if (flood_cnt > 1) {
        snprintf(flood_repeats, 1024
            , "%s Above message repeats %d times"
            , prefix_msg, flood_cnt-1);
        switch (log_mode) {
        case LOGMODE_FILE:
            strncat(flood_repeats, "\n", 1024 - strlen(flood_repeats));
            fputs(flood_repeats, logfile);
            fflush(logfile);
            break;
        case LOGMODE_SYSLOG:
            /* The syslog level values are one less*/
            syslog(loglvl-1, "%s", flood_repeats);
            strncat(flood_repeats, "\n", 1024 - strlen(flood_repeats));
            fputs(flood_repeats, stderr);
            fflush(stderr);
            break;
        }
    }

    flood_cnt = 1;
    snprintf(flood_msg, 1024, "%s", &buf[16]);
    snprintf(prefix_msg, prefixlen, "%s", buf);
    switch (log_mode) {
    case LOGMODE_FILE:
        strncat(buf, "\n", 1024 - strlen(buf));
        fputs(buf, logfile);
        fflush(logfile);
        break;
    case LOGMODE_SYSLOG:
        syslog(loglvl-1, "%s", buf);
        strncat(buf, "\n", 1024 - strlen(buf));
        fputs(buf, stderr);
        fflush(stderr);
        break;
    }

}

void log_init(cls_app *app)
{
    if ((app->conf->log_level > ALL) ||
        (app->conf->log_level == 0)) {
        app->conf->log_level = LEVEL_DEFAULT;
        LOG_MSG(NTC, NO_ERRNO
            ,"Using default log level (%s) (%d)"
            ,log_level_str[app->conf->log_level]
            ,app->conf->log_level);
    }

    if (app->conf->log_file != "") {
        if (app->conf->log_file != "syslog") {
            log_set_mode(LOGMODE_FILE);
            log_set_logfile(app->conf->log_file.c_str());
            if (logfile) {
                log_set_mode(LOGMODE_SYSLOG);
                LOG_MSG(NTC, NO_ERRNO
                    , "Logging to file (%s)"
                    , app->conf->log_file.c_str());
                log_set_mode(LOGMODE_FILE);
            } else {
                LOG_MSG(EMG, SHOW_ERRNO
                    , "Exit.  Cannot create log file %s"
                    , app->conf->log_file.c_str());
                exit(0);
            }
        } else {
            LOG_MSG(NTC, NO_ERRNO, "Logging to syslog");
        }
    } else {
        LOG_MSG(NTC, NO_ERRNO, "Logging to syslog");
    }

    log_level = app->conf->log_level;
}

void log_deinit(cls_app *app)
{
    if (logfile != NULL) {
        LOG_MSG(NTC, NO_ERRNO
            , "Closing logfile (%s)."
            , app->conf->log_file.c_str());
        myfclose(logfile);
        log_set_mode(LOGMODE_NONE);
        logfile = NULL;
    }
}
