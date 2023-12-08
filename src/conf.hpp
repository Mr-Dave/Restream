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


#ifndef _INCLUDE_CONF_HPP_
#define _INCLUDE_CONF_HPP_

    struct ctx_config {
        std::string     log_file;
        int             log_level;

        /* Webcontrol configuration parameters */
        int             webcontrol_port;
        int             webcontrol_port2;
        std::string     webcontrol_base_path;
        bool            webcontrol_ipv6;
        bool            webcontrol_localhost;
        int             webcontrol_parms;
        std::string     webcontrol_interface;
        std::string     webcontrol_auth_method;
        std::string     webcontrol_authentication;
        bool            webcontrol_tls;
        std::string     webcontrol_cert;
        std::string     webcontrol_key;
        std::string     webcontrol_headers;
        std::string     webcontrol_html;
        int             webcontrol_lock_minutes;
        int             webcontrol_lock_attempts;
        std::string     webcontrol_lock_script;

        std::list<std::string>  channels;

    };

    enum PARM_CAT{
        PARM_CAT_00
        ,PARM_CAT_01
        ,PARM_CAT_02
        ,PARM_CAT_MAX
    };
    enum PARM_TYP{
        PARM_TYP_STRING
        , PARM_TYP_INT
        , PARM_TYP_LIST
        , PARM_TYP_BOOL
        , PARM_TYP_ARRAY
    };

    /** Current parameters in the config file */
    struct ctx_parm {
        const std::string   parm_name;      /* name for this parameter                  */
        enum PARM_TYP       parm_type;      /* enum of parm_typ for bool,int or string. */
        enum PARM_CAT       parm_cat;       /* enum of parm_cat for grouping. */
        int                 webui_level;    /* Enum to display in webui: 0,1,2,3,99(always to never)*/
    };

    extern struct ctx_parm config_parms[];

    void conf_init(ctx_app *app);
    void conf_parms_log(ctx_app *app);
    void conf_parms_write(ctx_app *app);

    void conf_edit_set(ctx_config *conf, std::string parm_nm
        , std::string parm_val);
    void conf_edit_get(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_CAT parm_cat);
    void conf_edit_get(ctx_config *conf, std::string parm_nm
        , std::list<std::string> &parm_val, enum PARM_CAT parm_cat);
    void conf_edit_list(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_CAT parm_cat);
    void conf_edit_set_bool(bool &parm_dest, std::string &parm_in);

    std::string conf_type_desc(enum PARM_TYP ptype);
    std::string conf_cat_desc(enum PARM_CAT pcat, bool shrt);

#endif /* _INCLUDE_CONF_HPP_ */
