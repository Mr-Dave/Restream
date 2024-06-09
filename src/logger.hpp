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

#ifndef _INCLUDE_LOGGER_HPP_
#define _INCLUDE_LOGGER_HPP_

    #define LOGMODE_NONE            0   /* No logging             */
    #define LOGMODE_FILE            1   /* Log messages to file   */
    #define LOGMODE_SYSLOG          2   /* Log messages to syslog */

    #define NO_ERRNO                0   /* Do not display system error message */
    #define SHOW_ERRNO              1   /* Display the system error message */

    #define LOG_ALL                 9
    #define EMG                     1
    #define ALR                     2
    #define CRT                     3
    #define ERR                     4
    #define WRN                     5
    #define NTC                     6
    #define INF                     7
    #define DBG                     8
    #define ALL                     9
    #define LEVEL_DEFAULT           ALL

    #define LOG_MSG(x, z, format, args...) app->log->write_msg(x, z, true, format, __FUNCTION__, ##args)
    #define SHT_MSG(x, z, format, args...) app->log->write_msg(x, z, false, format, ##args)

    class cls_log {
        public:
            cls_log();
            ~cls_log();
            int             log_level;
            void set_log_file(std::string pname);
            void write_msg(int loglvl, int flgerr, bool flgfnc, const char *fmt, ...);
        private:
            pthread_mutex_t     mtx;
            int                 log_mode;
            FILE                *log_file_ptr;
            std::string         log_file_name;
            char                msg_prefix[512];
            char                msg_flood[1024];
            char                msg_full[1024];
            int                 flood_cnt;
            void set_mode(int mode);
            void write_flood(int loglvl);
            void write_norm(int loglvl, int prefixlen);
            void add_errmsg(int flgerr, int err_save);

    };

#endif /* _INCLUDE_LOGGER_HPP_ */
