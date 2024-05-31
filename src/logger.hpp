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

    /* Logging mode */
    #define LOGMODE_NONE            0   /* No logging             */
    #define LOGMODE_FILE            1   /* Log messages to file   */
    #define LOGMODE_SYSLOG          2   /* Log messages to syslog */

    #define NO_ERRNO                0   /* Flag to avoid how message associated to errno */
    #define SHOW_ERRNO              1   /* Flag to show message associated to errno */

    /* Log levels */
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

    #define LOG_MSG(x, z, format, args...) log_msg(x, z, true, format, __FUNCTION__, ##args)

    void log_msg(int loglvl, int flgerr, bool flgfnc, const char *fmt, ...);

    void log_init();
    void log_deinit();
    void log_set_level(int new_level);

#endif /* _INCLUDE_LOGGER_HPP_ */
