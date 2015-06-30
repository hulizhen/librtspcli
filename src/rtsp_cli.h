/*********************************************************************
 * File Name    : rtsp_cli.h
 * Description  : Implementaion of a RTSP client.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-21
 ********************************************************************/

#ifndef __RTSP_CLI_H__
#define __RTSP_CLI_H__


#include <pthread.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include "list.h"
#include "util.h"
#include "librtspcli.h"
#include "rtsp_method.h"
#include "sd_handler.h"
#include "rtp.h"


#define RTSP_VER        "RTSP/1.0" /* RTSP version. */
#define RTSP_CLIENT     "Zmodo RTSP Client"
#define CRLF            "\r\n"

#define KEEPALIVE_INTVL     3   /* keepalive interval, second(s) */
#define KEEPALIVE_CNT       3   /* max un-responsed keepalive message number */

#define EPOLL_MAX_EVS   128       /* max epoll events */

/* Max URI size, example like: `rtsp://172.18.16.133:10554/av0_0'. */
#define MAX_URI_SZ      256

/* Max RTSP version size, example like: `RTSP/1.0'. */
#define MAX_VER_SZ      16

/* Max date size, example like: `Date: 25 Dec 2012 12:34:56 GMT'. */
#define MAX_DATE_SZ             64
#define MAX_REASON_SZ           64
#define MAX_PUBLIC_SZ           128
#define MAX_ACCEPT_SZ           128
#define MAX_CONTENT_TYPE_SZ     64
#define MAX_TRANSPORT_SZ        128
#define MAX_RANGE_SZ            64
#define MAX_USR_AGENT_SZ        64
#define MAX_ENC_NAME_SZ         32
#define MAX_TRACK_SZ            256
#define MAX_PROF_LVL_ID_SZ      128
#define MAX_SPROP_PARAM_SET_SZ  128
#define MAX_SDP_A_NAME_SZ       16
#define MAX_PROTO_SZ            16


#define RECV_BUF_SZ     (100 * 1024)

#define RTSP_SD_DFL_EV      (EPOLLIN | EPOLLET)
#define RTP_SD_DFL_EV       (EPOLLIN)
#define RTCP_SD_DFL_EV      (EPOLLIN)

/* RTSP state */
enum rtsp_state {
    RTSP_STATE_INIT,
    RTSP_STATE_READY,
    RTSP_STATE_PLAYING,
};

/* RTSP method handling state */
enum handling_state {
    HANDLING_STATE_INIT,
    HANDLING_STATE_DOING,
};

/* store information RTSP client */
struct rtsp_cli {
    struct list_head rtsp_sess_list;
    pthread_mutex_t list_mutex; /* mutex for session list */
    store_frm_t store_frm;      /* callback function to store frame */
};

/* RTP header. */
struct rtp_hdr {
#ifdef BIGENDIAN
	unsigned char v:2;          /* protocol version */
	unsigned char p:1;         	/* padding flag */
	unsigned char x:1;         	/* header extension flag */
	unsigned char cc:4;       	/* CSRC count */
	unsigned char m:1;         	/* marker bit */
	unsigned char pt:7;        	/* payload type */
#else
	unsigned char cc:4;
	unsigned char x:1;
	unsigned char p:1;
	unsigned char v:2;
	unsigned char pt:7;
	unsigned char m:1;
#endif
	unsigned short seq;			/* sequence number */
	unsigned int   ts;          /* timestamp */
	unsigned int   ssrc;        /* synchronization source */
#if 0
	unsigned int   csrc[2];		/* optional CSRC list */
#endif
};

struct rtp_rtcp {
    int enable;
    union {
        struct tcp {            /* used in interleaved mode */
            char rtp_chn;
            char rtcp_chn;
        } tcp;
        struct udp {
            struct sock rtp_sock;     /* client RTP socket descriptor */
            struct sock rtcp_sock;    /* client RTCP socket descriptor */
            unsigned short rtp_port;  /* client RTP port */
            unsigned short rtcp_port; /* client RTCP port */
            struct sockaddr rtp_sa;   /* server RTP socket address */
            struct sockaddr rtcp_sa;  /* server RTCP socket address */
        } udp;
    };
};

/* RTSP request */
struct rtsp_req {
    struct req_line {
        enum rtsp_method method;
        char uri[MAX_URI_SZ];
        char ver[MAX_VER_SZ];
    } req_line;
    struct req_hdr {
        unsigned int cseq;
        unsigned long long sess_id;
        char usr_agent[MAX_USR_AGENT_SZ];
        char date[MAX_DATE_SZ];
        char accept[MAX_ACCEPT_SZ];
        char transport[MAX_TRANSPORT_SZ];
        char range[MAX_RANGE_SZ];
    } req_hdr;
};

/* RTSP response */
struct rtsp_resp {
    struct resp_line {
        char ver[MAX_VER_SZ];
        enum status_code code;
        char reason[MAX_REASON_SZ];
    } resp_line;
    struct resp_hdr {
        unsigned int cseq;
        unsigned long long sess_id;
        char date[MAX_DATE_SZ];
        char public[MAX_PUBLIC_SZ];
        char content_type[MAX_CONTENT_TYPE_SZ];
        unsigned int content_length;
        struct resp_transport {
            int intlvd_mode;
            char rtp_chn;
            char rtcp_chn;
            unsigned short rtp_cli_port;
            unsigned short rtcp_cli_port;
            unsigned short rtp_srv_port;
            unsigned short rtcp_srv_port;
        } transport;
    } resp_hdr;
    struct sdp_info *sdp_info;      /* After parsed the method `DESCRIBE',
                                     * the sdp_info will be hung to struct rtsp_sess. */
};

/* Interleaved header for transport(RTP over RTSP). */
struct intlvd {
    unsigned char dollar;
    unsigned char chn;
    unsigned short sz;
};

enum intlvd_chn {
    INTLVD_CHN_RTP_V,
    INTLVD_CHN_RTCP_V,
    INTLVD_CHN_RTP_A,
    INTLVD_CHN_RTCP_A,
};

/* used for receiving data from rtsp_sd */
struct last_data {
    char *buf;                      /* buffer used for storing un-completed data received last time */
    unsigned int sz;                /* size of data received last time */
};

/* SDP attribute description */
struct sdp_a {
    struct list_head entry;
    char name[MAX_SDP_A_NAME_SZ];
    union {
        struct rtpmap {
            enum rtp_pt pt;                 /* media payload type */
            char enc_name[MAX_ENC_NAME_SZ]; /* encoding name */
            unsigned int clk_rate;          /* clock rate */
        } rtpmap;
        struct control {
            char track[MAX_TRACK_SZ];
        } control;
        struct range {
            unsigned int start;
            unsigned int end;
        } range;
        struct fmtp {
            enum rtp_pt pt;     /* media payload type */
            char packetization_mode;
            char profile_level_id[MAX_PROF_LVL_ID_SZ];
            char sprop_parameter_sets[MAX_SPROP_PARAM_SET_SZ];
        } fmtp;
    };
};

/* SDP media description */
struct sdp_m {
    int enable;
    int port;
    char proto[MAX_PROTO_SZ];
    enum rtp_pt pt;
    struct list_head sdp_a_list;
    
};

/*
 * Actually, the struct definition is incomplete.
 * Just define what we need.
 */
struct sdp_info {
    struct sdp_m sdp_m[2];      /* 0: video; 1: audio */
};

/* Each RTSP session has this struct to store its information. */
struct rtsp_sess {
    struct list_head entry;         /* entry of RTSP session list */
    char uri[MAX_URI_SZ];           /* RTSP uri */
    int enable;                     /* state of the session */
    struct sockaddr_in srv_addr;    /* socket address RTSP server */
    pthread_t rtsp_sess_tid;        /* thread ID of RTSP session thread */
    unsigned long long sess_id;     /* RTSP session ID */
    unsigned int cur_cseq;          /* CSeq used in current RTSP interactive */
    enum rtsp_method todo;          /* current handling RTSP method */
    enum handling_state handling_state; /* RTSP method to do this time */
    enum rtsp_state rtsp_state;     /* used in RTSP state machine */
    int intlvd_mode;                /* interleaved mode */
    int ep_fd;                      /* epoll file descriptor */
    struct epoll_event *ep_ev;      /* epoll event */

    unsigned long long last_keepalive; /* last time of sending keepalive message */
    unsigned keepalive_cnt;     /* current un-responsed keepalive message */

    struct supported_method {       /* RTSP method supported by RTSP server */
        enum rtsp_method method;
        int supported;
    } supported_method[RTSP_METHOD_NUM];

    struct chn_info chn_info;       /* information of remote channel */
    struct frm_info frm_info;       /* information of frame, pass on to the storing frame callback */

    struct last_data last_data;
    struct list_head send_queue;    /* a list keeps send buffers to be sent out */
    struct sock rtsp_sock;          /* used in RTSP interactive & interleaved mode */
    struct sdp_info *sdp_info;      /* session description information */
    struct rtp_rtcp rtp_rtcp[2];    /* struct store RTP & RTCP information */
};

/* Global variable shared in the library. */
extern struct rtsp_cli rtsp_cli;

struct rtsp_sess *create_rtsp_sess(char *uri, struct sockaddr_in *srv_addrp,
                                   struct chn_info *chnp, int intlvd);
void destroy_rtsp_sess(struct rtsp_sess *sessp);

struct rtsp_req *alloc_rtsp_req(enum rtsp_method method, unsigned int cseq);
void free_rtsp_req(struct rtsp_req *req);

int handle_rtsp_resp(struct rtsp_sess *sessp, const char *msg, unsigned int sz);


#endif /* __RTSP_CLI_H__ */
