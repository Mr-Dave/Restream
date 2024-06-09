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

#ifndef _INCLUDE_WEBU_HPP_
#define _INCLUDE_WEBU_HPP_

    /* Some defines of lengths for our buffers */
    #define WEBUI_LEN_PARM 512          /* Parameters specified */
    #define WEBUI_LEN_URLI 512          /* Maximum URL permitted */
    #define WEBUI_LEN_RESP 2048         /* Initial response size */
    #define WEBUI_MHD_OPTS 10           /* Maximum number of options permitted for MHD */

    #define WEBUI_POST_BFRSZ  512

    enum WEBUI_METHOD {
        WEBUI_METHOD_GET    = 0,
        WEBUI_METHOD_POST   = 1
    };

    enum WEBUI_CNCT {
        WEBUI_CNCT_CONTROL,
        WEBUI_CNCT_TS_FULL,
        WEBUI_CNCT_UNKNOWN
    };

    enum WEBUI_RESP {
        WEBUI_RESP_HTML     = 0,
        WEBUI_RESP_JSON     = 1,
        WEBUI_RESP_TEXT     = 2
    };

    struct ctx_key {
        char                        *key_nm;        /* Name of the key item */
        char                        *key_val;       /* Value of the key item */
        size_t                      key_sz;         /* The size of the value */
    };

    struct ctx_webui {
        std::string                 url;            /* The URL sent from the client */
        std::string                 uri_chid;       /* Parsed channel number from the url eg /camid/cmd1/cmd2/cmd3 */
        std::string                 uri_cmd1;       /* Parsed command1 from the url eg /camid/cmd1/cmd2/cmd3 */
        std::string                 uri_cmd2;       /* Parsed command2 from the url eg /camid/cmd1/cmd2/cmd3 */
        std::string                 uri_cmd3;       /* Parsed command3 from the url eg /camid/cmd1/cmd2/cmd3 */

        std::string                 clientip;       /* IP of the connecting client */
        std::string                 hostfull;       /* Full http name for host with port number */

        char                        *auth_opaque;   /* Opaque string for digest authentication*/
        char                        *auth_realm;    /* Realm string for digest authentication*/
        char                        *auth_user;     /* Parsed user from config authentication string*/
        char                        *auth_pass;     /* Parsed password from config authentication string*/
        int                         authenticated;  /* Boolean for whether authentication has been passed */

        std::string                 resp_page;      /* The response that will be sent */
        unsigned char               *resp_image;    /* Response image to provide to user */
        int                         resp_type;      /* indicator for the type of response to provide. */
        size_t                      resp_size;      /* The allocated size of the response */
        size_t                      resp_used;      /* The amount of the response page used */
        size_t                      aviobuf_sz;     /* The size of the mpegts avio buffer */

        ctx_file_info               wfile;
        int64_t                     file_cnt;
        int                         start_cnt;
        cls_channel                 *chitm;
        int                         channel_indx;   /* Index number of the channel */
        int                         channel_id;     /* channel id number requested */

        int64_t                     msec_cnt;
        AVPacket                    *pkt;
        int                         pkt_index;
        int64_t                     pkt_idnbr;
        int64_t                     pkt_start_pts;
        AVRational                  pkt_timebase;
        int64_t                     pkt_file_cnt;
        bool                        pkt_key;

        enum WEBUI_CNCT             cnct_type;      /* Type of connection we are processing */
        enum WEBUI_METHOD           cnct_method;    /* Connection method.  Get or Post */

        uint64_t                    stream_pos;     /* Stream position of sent image */
        int                         stream_fps;     /* Stream rate per second */
        struct timespec             time_last;      /* Keep track of processing time for stream thread*/
        int                         mhd_first;      /* Boolean for whether it is the first connection*/
        struct MHD_Connection       *connection;    /* The MHD connection value from the client */
    };

    void webu_init();
    void webu_deinit();

#endif /* _INCLUDE_WEBU_HPP_ */
