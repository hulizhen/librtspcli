/*********************************************************************
 * File Name    : rtsp_method.c
 * Description  : Implementation of RTSP method.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-22
 ********************************************************************/

#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "rtsp_cli.h"
#include "log.h"
#include "util.h"
#include "rtsp_method.h"
#include "send_queue.h"


static unsigned int sess_cseq(struct rtsp_sess *sessp)
{
    return ++sessp->cur_cseq;
}

/**
 * The function will produce a RTSP request message according
 * to struct rtsp_req, And add it to send buffer queue.
 */
static int produce_rtsp_req(struct rtsp_sess *sessp, struct rtsp_req *req)
{
    /* Use this MACRO to move forward while filling the response message. */
#define MOVE_FORWARD() do {                                         \
        left -= strlen(line);                                       \
        if (left < 0)  {                                            \
            printd("MOVE_FORWARD failed when parsing request.\n");  \
            return -1;                                              \
        }                                                           \
        line += strlen(line);                                       \
    } while (0)

    struct send_buf *sendp = NULL;
    char *line = NULL;
    unsigned int left = 0;

    /* Copy to send buffer and add it to send buffer queue. */
    sendp = alloc_send_buf(DATA_TYPE_RTSP_REQ, RTSP_MSG_SZ);
    if (!sendp) {
        return -1;
    }
    line = sendp->buf;
    left = sendp->sz;

    /* Start make the RTSP request message. */
    MOVE_FORWARD();
    snprintf(line, left, "%s %s %s"CRLF,
             rtsp_method_tkn[req->req_line.method],
             req->req_line.uri, req->req_line.ver);

    /* Common headers. */
    MOVE_FORWARD();
    snprintf(line, left, "CSeq: %d"CRLF, req->req_hdr.cseq);
    MOVE_FORWARD();
    snprintf(line, left, "Date: %s"CRLF, make_date_hdr());
    MOVE_FORWARD();
    snprintf(line, left, "User-Agent: %s"CRLF, RTSP_CLIENT);
    if (sessp->sess_id) {
        MOVE_FORWARD();
        snprintf(line, left, "Session: %llu"CRLF, sessp->sess_id);
    }

    switch (req->req_line.method) {
    case RTSP_METHOD_OPTIONS:
        break;
    case RTSP_METHOD_DESCRIBE:
        MOVE_FORWARD();
        snprintf(line, left, "Accept: %s"CRLF, req->req_hdr.accept);
        break;
    case RTSP_METHOD_SETUP:
        MOVE_FORWARD();
        snprintf(line, left, "Transport: %s"CRLF, req->req_hdr.transport);
        break;
    case RTSP_METHOD_PLAY:
        MOVE_FORWARD();
        snprintf(line, left, "Range: %s"CRLF, req->req_hdr.range);
        break;
    case RTSP_METHOD_PAUSE:
        break;
    case RTSP_METHOD_GET_PARAMETER:
        break;
    case RTSP_METHOD_SET_PARAMETER:
        break;
    case RTSP_METHOD_TEARDOWN:
        break;
    default:
        printd("Undefined RTSP method[%d]\n", req->req_line.method);
        free_send_buf(sendp);
        return -1;
    }

    /* End of request message. */
    MOVE_FORWARD();
    snprintf(line, left, CRLF);

    /* Get the actual message size which will be sent. */
    MOVE_FORWARD();
    sendp->sz -= left;

    print_rtsp_msg(sendp->buf, sendp->sz);

    list_add_tail(&sendp->entry, &sessp->send_queue);

    return 0;

#undef MOVE_FORWARD
}

void make_rtsp_uri(struct rtsp_sess *sessp, char *uri, unsigned int sz, enum media_type media)
{
    char track[MAX_TRACK_SZ] = {0};
    struct sdp_a *ap = NULL;

    if (sessp->todo == RTSP_METHOD_SETUP) {
        if (sessp->sdp_info) {
            list_for_each_entry(ap, &sessp->sdp_info->sdp_m[media].sdp_a_list, entry) {
                if (!strncmp(ap->name, "control", strlen("control"))) {
                    memcpy(track, ap->control.track, sizeof(track));
                    break;
                }
            }
            if (track[0] == '\0') {
                snprintf(track, sizeof(track), "track%d", media);
            }
            if (strstr(track, "rtsp://")) {
                snprintf(uri, sz, "%s", track);
            } else {
                snprintf(uri, sz, "%s/%s", sessp->uri, track);
            }
        }
    } else {
        strncpy(uri, sessp->uri, MAX_URI_SZ);
    }

    return;
}

int send_method_options(struct rtsp_sess *sessp)
{
    struct rtsp_req *req = NULL;
    int ret = 0;

	sessp->handling_state = HANDLING_STATE_DOING;

    /* RTSP request */
    req = alloc_rtsp_req(RTSP_METHOD_OPTIONS, sess_cseq(sessp));
    if (!req) {
        return -1;
    }

    /* Fill RTSP request. */
    make_rtsp_uri(sessp, req->req_line.uri, sizeof(req->req_line.uri), 0);

    if (produce_rtsp_req(sessp, req) < 0) {
        ret = -1;
    }
    free_rtsp_req(req);

    return ret;
}

int send_method_describe(struct rtsp_sess *sessp)
{
    struct rtsp_req *req = NULL;
    int ret = 0;

    if (!sessp->supported_method[RTSP_METHOD_DESCRIBE].supported) {
        return 0;
    }
	sessp->handling_state = HANDLING_STATE_DOING;

    /* RTSP request */
    req = alloc_rtsp_req(RTSP_METHOD_DESCRIBE, sess_cseq(sessp));
    if (!req) {
        return -1;
    }

    /* Fill RTSP request. */
    make_rtsp_uri(sessp, req->req_line.uri, sizeof(req->req_line.uri), 0);
    snprintf(req->req_hdr.accept, sizeof(req->req_hdr.accept),
             "application/sdp");

    if (produce_rtsp_req(sessp, req) < 0) {
        ret = -1;
    }
    free_rtsp_req(req);

    return ret;
}

static int make_transport_hdr(struct rtsp_sess *sessp,
                              struct rtsp_req *req, enum media_type media)
{
    if (sessp->intlvd_mode) {
        snprintf(req->req_hdr.transport, sizeof(req->req_hdr.transport),
                 "RTP/AVP/TCP;unicast;interleaved=%d-%d",
                 media * 2, media * 2 + 1); /* RTP & RTCP interleaved channel ID */
    } else {
        snprintf(req->req_hdr.transport, sizeof(req->req_hdr.transport),
                 "RTP/AVP/UDP;unicast;client_port=%d-%d",
                 sessp->rtp_rtcp[media].udp.rtp_port,
                 sessp->rtp_rtcp[media].udp.rtcp_port);
    }
    return 0;
}

static int setup_transport(struct rtsp_sess *sessp, enum media_type media)
{
    int first_sd = -1;
    int second_sd = -1;
    unsigned short first_port = 0;
    unsigned short second_port = 0;
    struct sockaddr_in sa;  /* use for creating RTP & RTCP socket */
    socklen_t salen = sizeof(sa);
    struct rtp_rtcp *rtp_rtcp = &sessp->rtp_rtcp[media];
    struct sockaddr_in *tmp_sap = NULL;
    struct sockaddr_in rtsp_sa;

    rtp_rtcp->udp.rtp_sock.sd = -1;
    rtp_rtcp->udp.rtcp_sock.sd = -1;

    if (!sessp->intlvd_mode) {
        /* Create first socket. */
        bzero(&sa, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        if ((first_sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("Create first UDP socket for RTP error");
            goto err;
        }
        if (bind(first_sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            perror("Bind first UDP sockaddr error");
            goto err;
        }
        if (getsockname(first_sd, &sa, &salen) < 0) {
            perror("Getsockname first UDP socket error");
            goto err;
        }
        first_port = ntohs(sa.sin_port);
        second_port = (first_port % 2 == 0) ? (first_port + 1) : (first_port - 1);

        /* Create second socket. */
        sa.sin_port = htons(second_port);
        if ((second_sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("Create second UDP socket for RTP error");
            goto err;
        }
        if (bind(second_sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            if (errno == EADDRINUSE) { /* address already in use */
                sa.sin_port = 0;       /* dynamicly allocate */
                if (bind(second_sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
                    perror("Bind second UDP sockaddr error");
                    goto err;
                }
            } else {
                perror("Bind second UDP sockaddr error");
                goto err;
            }
        }
        if (getsockname(second_sd, &sa, &salen) < 0 ) {
            perror("Getsockname second UDP socket error");
            goto err;
        }
        second_port = ntohs(sa.sin_port);
        rtp_rtcp->udp.rtp_sock.sd = (first_port % 2 == 0) ? first_sd : second_sd;
        rtp_rtcp->udp.rtcp_sock.sd = (first_port % 2 == 0) ? second_sd : first_sd;
        rtp_rtcp->udp.rtp_port = (first_port % 2 == 0) ? first_port : second_port;
        rtp_rtcp->udp.rtcp_port = (first_port % 2 == 0) ? second_port : first_port;

        /* set the RTP/RTCP socket */
        /* RTP */
        if (set_block_mode(rtp_rtcp->udp.rtp_sock.sd, 0) < 0) {
            goto err;
        }
        rtp_rtcp->udp.rtp_sock.arg = sessp;
        rtp_rtcp->udp.rtp_sock.handler = handle_rtp_sd;
        rtp_rtcp->udp.rtp_sock.ev = RTP_SD_DFL_EV;
        if (monitor_sd_event(sessp->ep_fd, rtp_rtcp->udp.rtp_sock.sd,
                             rtp_rtcp->udp.rtp_sock.ev) < 0) {
            goto err;
        }

        /* RTCP */
        if (set_block_mode(rtp_rtcp->udp.rtcp_sock.sd, 0) < 0) {
            goto err;
        }
        rtp_rtcp->udp.rtcp_sock.arg = sessp;
        rtp_rtcp->udp.rtcp_sock.handler = handle_rtcp_sd;
        rtp_rtcp->udp.rtcp_sock.ev = RTCP_SD_DFL_EV;
        if (monitor_sd_event(sessp->ep_fd, rtp_rtcp->udp.rtcp_sock.sd,
                             rtp_rtcp->udp.rtcp_sock.ev) < 0) {
            goto err;
        }

        /* get UDP peer socket address */
        if (getpeername(sessp->rtsp_sock.sd, &rtsp_sa, &salen) < 0) {
            perror("getpeername() cli_sock");
            goto err;
        }

        tmp_sap = (struct sockaddr_in *)&sessp->rtp_rtcp[media].udp.rtp_sa;
        tmp_sap->sin_family = rtsp_sa.sin_family;
        tmp_sap->sin_addr = rtsp_sa.sin_addr;

        tmp_sap = (struct sockaddr_in *)&sessp->rtp_rtcp[media].udp.rtcp_sa;
        tmp_sap->sin_family = rtsp_sa.sin_family;
        tmp_sap->sin_addr = rtsp_sa.sin_addr;
    }
    return 0;

err:
    if (!sessp->intlvd_mode) {
        close(rtp_rtcp->udp.rtp_sock.sd);
        rtp_rtcp->udp.rtp_sock.sd = -1;
        close(rtp_rtcp->udp.rtcp_sock.sd);
        rtp_rtcp->udp.rtcp_sock.sd = -1;
    }
    return -1;
}

int send_method_setup(struct rtsp_sess *sessp)
{
    struct rtsp_req *req = NULL;
    int ret = 0;
    int i = 0;

    if (!sessp->supported_method[RTSP_METHOD_SETUP].supported) {
        return 0;
    }
	sessp->handling_state = HANDLING_STATE_DOING;

    /* RTSP request */
    req = alloc_rtsp_req(RTSP_METHOD_SETUP, sess_cseq(sessp));
    if (!req) {
        return -1;
    }

    /* Fill RTSP request. */
    if (sessp->sdp_info) {
        for (i = 0; i < 2; i++) {
            if (sessp->sdp_info->sdp_m[i].enable) {
                if (!sessp->rtp_rtcp[i].enable) {
                    make_rtsp_uri(sessp, req->req_line.uri,
                                  sizeof(req->req_line.uri), i);
                    setup_transport(sessp, i);
                    make_transport_hdr(sessp, req, i);
                    req->req_hdr.sess_id = sessp->sess_id;
                    sessp->rtp_rtcp[i].enable = 1;
                    break;
                }
            }
        }
    } else {
        return 0;
    }

    if (produce_rtsp_req(sessp, req) < 0) {
        ret = -1;
    }
    free_rtsp_req(req);

    return ret;
}

int send_method_play(struct rtsp_sess *sessp)
{
    struct rtsp_req *req = NULL;
    int ret = 0;

    if (!sessp->supported_method[RTSP_METHOD_PLAY].supported) {
        return 0;
    }
	sessp->handling_state = HANDLING_STATE_DOING;

    /* RTSP request */
    req = alloc_rtsp_req(RTSP_METHOD_PLAY, sess_cseq(sessp));
    if (!req) {
        return -1;
    }
    req->req_hdr.sess_id = sessp->sess_id;

    /* Fill RTSP request. */
    make_rtsp_uri(sessp, req->req_line.uri, sizeof(req->req_line.uri), 0);
    snprintf(req->req_hdr.range, sizeof(req->req_hdr.range), "npt=0.000-");

    if (produce_rtsp_req(sessp, req) < 0) {
        ret = -1;
    }
    free_rtsp_req(req);

    return ret;
}

int send_method_pause(struct rtsp_sess *sessp)
{
    if (!sessp->supported_method[RTSP_METHOD_PAUSE].supported) {
        return 0;
    }
	sessp->handling_state = HANDLING_STATE_DOING;
    return 0;
}

int send_method_get_parameter(struct rtsp_sess *sessp)
{
    if (!sessp->supported_method[RTSP_METHOD_GET_PARAMETER].supported) {
        return 0;
    }
	sessp->handling_state = HANDLING_STATE_DOING;
    return 0;
}

int send_method_set_parameter(struct rtsp_sess *sessp)
{
    if (!sessp->supported_method[RTSP_METHOD_SET_PARAMETER].supported) {
        return 0;
    }
	sessp->handling_state = HANDLING_STATE_DOING;
    return 0;
}

int send_method_teardown(struct rtsp_sess *sessp)
{
    struct rtsp_req *req = NULL;
    int ret = 0;

	sessp->handling_state = HANDLING_STATE_DOING;

    /* RTSP request */
    req = alloc_rtsp_req(RTSP_METHOD_TEARDOWN, sess_cseq(sessp));
    if (!req) {
        return -1;
    }

    /* Fill RTSP request. */
    make_rtsp_uri(sessp, req->req_line.uri, sizeof(req->req_line.uri), 0);

    if (produce_rtsp_req(sessp, req) < 0) {
        ret = -1;
    }
    free_rtsp_req(req);

    return ret;
}
