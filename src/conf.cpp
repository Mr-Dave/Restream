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

#include <dirent.h>
#include <string>

#include "restream.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "guide.hpp"
#include "infile.hpp"
#include "webu.hpp"

enum PARM_ACT{
    PARM_ACT_DFLT
    , PARM_ACT_SET
    , PARM_ACT_GET
    , PARM_ACT_LIST
};

/*Configuration parameters */
ctx_parm config_parms[] = {
    {"log_file",                  PARM_TYP_STRING, PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"log_level",                 PARM_TYP_LIST,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },

    {"webcontrol_port",           PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_port2",          PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_base_path",      PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_ipv6",           PARM_TYP_BOOL,   PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_localhost",      PARM_TYP_BOOL,   PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_parms",          PARM_TYP_LIST,   PARM_CAT_01, WEBUI_LEVEL_NEVER},
    {"webcontrol_interface",      PARM_TYP_LIST,   PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_auth_method",    PARM_TYP_LIST,   PARM_CAT_01, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_authentication", PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_tls",            PARM_TYP_BOOL,   PARM_CAT_01, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_cert",           PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_key",            PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_headers",        PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_html",           PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_lock_minutes",   PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_lock_attempts",  PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_lock_script",    PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_RESTRICTED },

    {"channel",                   PARM_TYP_ARRAY, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },

    { "", (enum PARM_TYP)0, (enum PARM_CAT)0, (enum WEBUI_LEVEL)0 }
};

void conf_edit_set_bool(bool &parm_dest, std::string &parm_in)
{
    if ((parm_in == "1") || (parm_in == "yes") || (parm_in == "on") || (parm_in == "true") ) {
        parm_dest = true;
    } else {
        parm_dest = false;
    }
}

static void conf_edit_get_bool(std::string &parm_dest, bool &parm_in)
{
    if (parm_in == true) {
        parm_dest = "on";
    } else {
        parm_dest = "off";
    }
}

static void conf_edit_log_file(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    char    lognm[4096];
    tm      *logtm;
    time_t  logt;

    if (pact == PARM_ACT_DFLT) {
        conf->log_file = "";
    } else if (pact == PARM_ACT_SET) {
        time(&logt);
        logtm = localtime(&logt);
        strftime(lognm, 4096, parm.c_str(), logtm);
        conf->log_file = lognm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->log_file;
    }
    return;
}

static void conf_edit_log_level(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->log_level = 6;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 9)) {
            LOG_MSG(NTC,  NO_ERRNO, "Invalid log_level %d",parm_in);
        } else {
            conf->log_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->log_level);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\"";
        parm = parm + "]";
    }

    return;
}

static void conf_edit_webcontrol_port(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_port = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            LOG_MSG(NTC, NO_ERRNO, "Invalid webcontrol_port %d",parm_in);
        } else {
            conf->webcontrol_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->webcontrol_port);
    }
    return;
}

static void conf_edit_webcontrol_base_path(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_base_path = "";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "/") {
            LOG_MSG(NTC, NO_ERRNO
                , "Invalid webcontrol_base_path: Use blank instead of single / ");
            conf->webcontrol_base_path = "";
        } else if (parm.length() >= 1) {
            if (parm.substr(0, 1) != "/") {
                LOG_MSG(NTC, NO_ERRNO
                    , "Invalid webcontrol_base_path:  Must start with a / ");
                conf->webcontrol_base_path = "/" + parm;
            } else {
                conf->webcontrol_base_path = parm;
            }
        } else {
            conf->webcontrol_base_path = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_base_path;
    }
    return;
}

static void conf_edit_webcontrol_ipv6(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_ipv6 = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->webcontrol_ipv6, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->webcontrol_ipv6);
    }
    return;
}

static void conf_edit_webcontrol_localhost(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_localhost = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->webcontrol_localhost, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->webcontrol_localhost);
    }
    return;
}

static void conf_edit_webcontrol_parms(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_parms = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 3)) {
            LOG_MSG(NTC, NO_ERRNO, "Invalid webcontrol_parms %d",parm_in);
        } else {
            conf->webcontrol_parms = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->webcontrol_parms);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"0\",\"1\",\"2\",\"3\"";
        parm = parm + "]";
    }
    return;
}

static void conf_edit_webcontrol_interface(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_interface = "default";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "default") || (parm == "user"))  {
            conf->webcontrol_interface = parm;
        } else if (parm == "") {
            conf->webcontrol_interface = "default";
        } else {
            LOG_MSG(NTC, NO_ERRNO, "Invalid webcontrol_interface %s", parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_interface;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"default\",\"user\"";
        parm = parm + "]";
    }

    return;
}

static void conf_edit_webcontrol_auth_method(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_auth_method = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "basic") || (parm == "digest"))  {
            conf->webcontrol_auth_method = parm;
        } else if (parm == "") {
            conf->webcontrol_auth_method = "none";
        } else {
            LOG_MSG(NTC, NO_ERRNO, "Invalid webcontrol_auth_method %s", parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_auth_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"none\",\"basic\",\"digest\"";
        parm = parm + "]";
    }
    return;
}

static void conf_edit_webcontrol_authentication(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_authentication = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_authentication = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_authentication;
    }
    return;
}

static void conf_edit_webcontrol_tls(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_tls = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->webcontrol_tls, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->webcontrol_tls);
    }
    return;
}

static void conf_edit_webcontrol_cert(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_cert = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_cert = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_cert;
    }
    return;
}

static void conf_edit_webcontrol_key(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_key = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_key = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_key;
    }
    return;
}

static void conf_edit_webcontrol_headers(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_headers = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_headers = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_headers;
    }
    return;
}

static void conf_edit_webcontrol_html(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_html = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_html = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_html;
    }
    return;
}

static void conf_edit_webcontrol_lock_minutes(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_lock_minutes = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 0) {
            LOG_MSG(NTC, NO_ERRNO, "Invalid webcontrol_lock_minutes %d",parm_in);
        } else {
            conf->webcontrol_lock_minutes = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->webcontrol_lock_minutes);
    }
    return;
}

static void conf_edit_webcontrol_lock_attempts(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_lock_attempts = 3;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 0) {
            LOG_MSG(NTC, NO_ERRNO, "Invalid webcontrol_lock_attempts %d",parm_in);
        } else {
            conf->webcontrol_lock_attempts = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->webcontrol_lock_attempts);
    }
    return;
}

static void conf_edit_webcontrol_lock_script(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_lock_script = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_lock_script = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_lock_script;
    }
    return;
}

static void conf_edit_channels(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->channels.clear();
        if (parm != "") {
            conf->channels.push_back(parm);
        }
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            return;
        }
        conf->channels.push_back(parm);   /* Add to the end of list*/
    } else if (pact == PARM_ACT_GET) {
        if (conf->channels.empty()) {
            parm = "";
        } else {
            parm = conf->channels.back();     /* Give last item*/
        }
    }
    return;
}

static void conf_edit_channels(ctx_config *conf, std::list<std::string> &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->channels.clear();
    } else if (pact == PARM_ACT_SET) {
        conf->channels = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->channels;
    }
    return;
}

static void conf_edit_cat00(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "log_file") {            conf_edit_log_file(conf, parm_val, pact);
    } else if (parm_nm == "log_level") {    conf_edit_log_level(conf, parm_val, pact);
    }
}
static void conf_edit_cat01(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "webcontrol_port") {                    conf_edit_webcontrol_port(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_base_path") {        conf_edit_webcontrol_base_path(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_ipv6") {             conf_edit_webcontrol_ipv6(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_localhost") {        conf_edit_webcontrol_localhost(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_parms") {            conf_edit_webcontrol_parms(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_interface") {        conf_edit_webcontrol_interface(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_auth_method") {      conf_edit_webcontrol_auth_method(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_authentication") {   conf_edit_webcontrol_authentication(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_tls") {              conf_edit_webcontrol_tls(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_cert") {             conf_edit_webcontrol_cert(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_key") {              conf_edit_webcontrol_key(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_headers") {          conf_edit_webcontrol_headers(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_html") {             conf_edit_webcontrol_html(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_lock_minutes") {     conf_edit_webcontrol_lock_minutes(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_lock_attempts") {    conf_edit_webcontrol_lock_attempts(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_lock_script") {      conf_edit_webcontrol_lock_script(conf, parm_val, pact);
    }
}

static void conf_edit_cat02(ctx_config *conf, std::string parm_nm
        ,std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "channel") {  conf_edit_channels(conf, parm_val, pact);
    }
}

static void conf_edit_cat02(ctx_config *conf, std::string parm_nm
        ,std::list<std::string> &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "channel") {  conf_edit_channels(conf, parm_val, pact);
    }
}

static void conf_edit_cat(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat)
{
    if (pcat == PARM_CAT_00) {          conf_edit_cat00(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_01) {   conf_edit_cat01(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_02) {   conf_edit_cat02(conf, parm_nm, parm_val, pact);
    }
}

static void conf_edit_cat(ctx_config *conf, std::string parm_nm
        ,std::list<std::string> &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat)
{
    if (pcat == PARM_CAT_02) {
        conf_edit_cat02(conf, parm_nm, parm_val, pact);
    }
}

static void conf_edit_dflt(ctx_config *conf)
{
    int indx;
    std::string dflt = "";

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        conf_edit_cat(conf, config_parms[indx].parm_name, dflt
            , PARM_ACT_DFLT, config_parms[indx].parm_cat);
        indx++;
    }
}

void conf_edit_get(ctx_config *conf, std::string parm_nm
    , std::string &parm_val, enum PARM_CAT parm_cat)
{
    conf_edit_cat(conf, parm_nm, parm_val, PARM_ACT_GET, parm_cat);
}

void conf_edit_get(ctx_config *conf, std::string parm_nm
    , std::list<std::string> &parm_val, enum PARM_CAT parm_cat)
{
    conf_edit_cat(conf, parm_nm, parm_val, PARM_ACT_GET, parm_cat);
}

/* Assign the parameter value */
void conf_edit_set(ctx_config *conf, std::string parm_nm
        , std::string parm_val)
{
    int indx;
    enum PARM_CAT pcat;

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (parm_nm ==  config_parms[indx].parm_name) {
            pcat = config_parms[indx].parm_cat;
            conf_edit_cat(conf, parm_nm, parm_val, PARM_ACT_SET, pcat);
            return;
        }
        indx++;
    }

    LOG_MSG(ALR, NO_ERRNO, "Unknown config option \"%s\"", parm_nm.c_str());
}

/* Get list of valid values for items only permitting a set*/
void conf_edit_list(ctx_config *conf, std::string parm_nm, std::string &parm_val
        , enum PARM_CAT parm_cat)
{
    conf_edit_cat(conf, parm_nm, parm_val, PARM_ACT_LIST, parm_cat);
}

std::string conf_type_desc(enum PARM_TYP ptype)
{
    if (ptype == PARM_TYP_BOOL) {           return "bool";
    } else if (ptype == PARM_TYP_INT) {     return "int";
    } else if (ptype == PARM_TYP_LIST) {    return "list";
    } else if (ptype == PARM_TYP_STRING) {  return "string";
    } else if (ptype == PARM_TYP_ARRAY) {   return "array";
    } else {                                return "error";
    }
}

/* Return a string describing the parameter category */
std::string conf_cat_desc(enum PARM_CAT pcat, bool shrt) {

    if (shrt) {
        if (pcat == PARM_CAT_00)        { return "app";
        } else if (pcat == PARM_CAT_01) { return "webcontrol";
        } else if (pcat == PARM_CAT_02) { return "channels";
        } else { return "unk";
        }
    } else {
        if (pcat == PARM_CAT_00)        { return "Application";
        } else if (pcat == PARM_CAT_01) { return "Webcontrol";
        } else if (pcat == PARM_CAT_02) { return "Channels";
        } else { return "Other";
        }
    }
}

/** Prints usage and options allowed from Command-line. */
static void usage(void)
{
    printf("Restream version %s, Copyright 2023\n","0.1.1");
    printf("\nusage:\trestream [options]\n");
    printf("\n\n");
    printf("Possible options:\n\n");
    printf("-c config\t\tFull path and filename of config file.\n");
    printf("-d level\t\tLog level (1-9) (EMG, ALR, CRT, ERR, WRN, NTC, INF, DBG, ALL). default: 6 / NTC.\n");
    printf("-l log file \t\tFull path and filename of log file.\n");
    printf("-h\t\t\tShow this screen.\n");
    printf("\n");
}

/** Process Command-line options specified */
static void conf_cmdline(ctx_app *app)
{
    int c;

    while ((c = getopt(app->argc, app->argv, "cdlh:")) != -1)
        switch (c) {
        case 'c':
            app->conf_file.assign(optarg);
            break;
        case 'd':
            app->conf->log_level= atoi(optarg);
            break;
        case 'l':
            app->conf->log_file.assign(optarg);
            break;
        case 'h':
        case '?':
        default:
             usage();
             exit(1);
        }

    optind = 1;
}

/** Process each line from the config file. */
void conf_process(ctx_app *app)
{
    size_t stpos;
    std::string line, parm_nm, parm_vl;
    std::ifstream ifs;

    ifs.open(app->conf_file);
        if (ifs.is_open() == false) {
            LOG_MSG(ERR, NO_ERRNO
                , "config file not found: %s"
                , app->conf_file.c_str());
            return;
        }

        LOG_MSG(NTC, NO_ERRNO
            , "Processing config file %s"
            , app->conf_file.c_str());

        while (std::getline(ifs, line)) {
            mytrim(line);
            stpos = line.find(" ");
            if (line.find('\t') != std::string::npos) {
                if (line.find('\t') < stpos) {
                    stpos =line.find('\t');
                }
            }
            if (stpos > line.find("=")) {
                stpos = line.find("=");
            }
            if ((stpos != std::string::npos) &&
                (stpos != line.length()-1) &&
                (stpos != 0) &&
                (line.substr(0, 1) != ";") &&
                (line.substr(0, 1) != "#")) {
                parm_nm = line.substr(0, stpos);
                parm_vl = line.substr(stpos+1, line.length()-stpos);
                myunquote(parm_nm);
                myunquote(parm_vl);
                conf_edit_set(app->conf, parm_nm, parm_vl);
            } else if ((line != "") &&
                (line.substr(0, 1) != ";") &&
                (line.substr(0, 1) != "#") &&
                (stpos != std::string::npos) ) {
                LOG_MSG(ERR, NO_ERRNO
                , "Unable to parse line: %s", line.c_str());
            }
        }
    ifs.close();

}

/**  Write the configuration(s) to the log */
void conf_parms_log(ctx_app *app)
{
    int i;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;

    log_msg(INF, NO_ERRNO,false,"Logging parameters from config file: %s"
        , app->conf_file.c_str());

    i = 0;
    while (config_parms[i].parm_name != "") {
        parm_nm=config_parms[i].parm_name;
        parm_ct=config_parms[i].parm_cat;
        parm_typ=config_parms[i].parm_type;
        if (parm_typ == PARM_TYP_ARRAY) {
            conf_edit_get(app->conf, parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                log_msg(INF, NO_ERRNO, false, "%-25s %s", parm_nm.c_str(), it->c_str());
            }
        } else {
            conf_edit_get(app->conf, parm_nm, parm_vl, parm_ct);
            log_msg(INF, NO_ERRNO,false, "%-25s %s", parm_nm.c_str(), parm_vl.c_str());
        }

        i++;
    }
}

void conf_parms_write_parms(FILE *conffile, std::string parm_nm
    , std::string parm_vl, enum PARM_CAT parm_ct, bool reset)
{
    static enum PARM_CAT prev_ct;

    if (reset) {
        prev_ct = PARM_CAT_00;
        return;
    }

    if (parm_ct != prev_ct) {
        fprintf(conffile,"\n%s",";*************************************************\n");
        fprintf(conffile,"%s%s\n", ";*****   ", conf_cat_desc(parm_ct,false).c_str());
        fprintf(conffile,"%s",";*************************************************\n");
        prev_ct = parm_ct;
    }

    if (parm_vl.compare(0, 1, " ") == 0) {
        fprintf(conffile, "%s \"%s\"\n", parm_nm.c_str(), parm_vl.c_str());
    } else {
        fprintf(conffile, "%s %s\n", parm_nm.c_str(), parm_vl.c_str());
    }
}

/**  Write the configuration(s) to file */
void conf_parms_write(ctx_app *app)
{
    int i;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;
    char timestamp[32];
    FILE *conffile;

    time_t now = time(0);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

    conffile = myfopen(app->conf_file.c_str(), "we");
    if (conffile == NULL) {
        LOG_MSG(NTC,  NO_ERRNO
            , "Failed to write configuration to %s"
            , app->conf_file.c_str());
        return;
    }

    fprintf(conffile, "; %s\n", app->conf_file.c_str());
    fprintf(conffile, "; at %s\n", timestamp);
    fprintf(conffile, "\n\n");

    conf_parms_write_parms(conffile, "", "", PARM_CAT_00, true);

    i=0;
    while (config_parms[i].parm_name != "") {
        parm_nm=config_parms[i].parm_name;
        parm_ct=config_parms[i].parm_cat;
        parm_typ=config_parms[i].parm_type;
        if (parm_typ == PARM_TYP_ARRAY) {
            conf_edit_get(app->conf, parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                conf_parms_write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
            }
        } else {
            conf_edit_get(app->conf, parm_nm, parm_vl, parm_ct);
            conf_parms_write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
        }
        i++;
    }

    fprintf(conffile, "\n");
    myfclose(conffile);

    LOG_MSG(NTC,  NO_ERRNO
        , "Configuration written to %s"
        , app->conf_file.c_str());
}

void conf_init(ctx_app *app)
{
    std::string filename;
    char path[PATH_MAX];
    struct stat statbuf;

    conf_edit_dflt(app->conf);

    conf_cmdline(app);

    filename = "";
    if (app->conf_file != "") {
        filename = app->conf_file;
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename="";
        }
    }

    if (filename == "") {
        if (getcwd(path, sizeof(path)) == NULL) {
            LOG_MSG(ERR,  SHOW_ERRNO, "Error getcwd");
            exit(-1);
        }
        filename = path + std::string("/restream.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        filename = std::string(getenv("HOME")) + std::string("/.restream/restream.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        filename = std::string( sysconfdir ) + std::string("/restream.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        LOG_MSG(ALR,  SHOW_ERRNO
            ,"Could not open configuration file");
        exit(-1);
    }

    app->conf_file = filename;

    conf_process(app);

}

