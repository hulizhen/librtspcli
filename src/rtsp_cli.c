/*********************************************************************
 * File Name    : rtsp_cli.c
 * Description  : Implementaion of a RTSP client.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-21
 ********************************************************************/

#include <stdio.h>
#include <pthread.h>
#include <sys/epoll.h>
#include "log.h"
#include "util.h"
#include "list.h"
#include "rtsp_cli.h"
#include "rtsp_method.h"
#include "send_queue.h"
#include "sd_handler.h"
#include "parser.h"


#define CONN_TIMEOUT    5
#define RECONN_INTERVAL 5


/**
 * Start RTSP interactive to play the media stream.
 */
static void start_playing(struct rtsp_sess *sessp)
{
    int i = 0;
    static int (*send_method[])(struct rtsp_sess *) = {
        [RTSP_METHOD_OPTIONS]       = send_method_options,
        [RTSP_METHOD_DESCRIBE]      = send_method_describe,
        [RTSP_METHOD_SETUP]         = send_method_setup,
        [RTSP_METHOD_PLAY]          = send_method_play,
        [RTSP_METHOD_PAUSE]         = send_method_pause,
        [RTSP_METHOD_GET_PARAMETER] = send_method_get_parameter,
        [RTSP_METHOD_SET_PARAMETER] = send_method_set_parameter,
        [RTSP_METHOD_TEARDOWN]      = send_method_teardown,
    };

    if (sessp->handling_state != HANDLING_STATE_INIT) {
        return;
    }

    /*
     * This place use `do while' loop,
     * just for convinience to stop doing the
     * continuing stuff, and exit the loop.
     */
    do {
        for (i = 0; i < RTSP_METHOD_NUM; i++) {
            if (sessp->supported_method[i].supported) {
                break;
            }
        }
        /* If all RTSP methods of this session arm un-supported,
         * it implies that we have to send the method `OPTIONS'. */
        if (i == RTSP_METHOD_NUM) {
            sessp->todo = RTSP_METHOD_OPTIONS;
            break;
        }

        if (sessp->rtsp_state != RTSP_STATE_READY) {
            if (!sessp->sdp_info) {
                sessp->todo = RTSP_METHOD_DESCRIBE;
                break;
            } else {
                sessp->todo = RTSP_METHOD_SETUP;
                break;
            }
        } else {
            sessp->todo = RTSP_METHOD_PLAY;
            break;
        }
    } while (0);

    /* Send the RTSP method request. */
    if (sessp->todo != RTSP_METHOD_NONE) {
        send_method[sessp->todo](sessp);
    }

    return;
}

/**
 * Main loop of RTSP session will excute this in each loop.
 */
static int single_step(struct rtsp_sess *sessp)
{
    int i = 0;
    int nfds = -1;
    unsigned long long now = 0;

    if (sessp->rtsp_state != RTSP_STATE_PLAYING) {
        start_playing(sessp);
    }

    /* keepalive RTSP session */
    now = time_now();
    if ((sessp->rtsp_state == RTSP_STATE_PLAYING) &&
        (now < sessp->last_keepalive || (now - sessp->last_keepalive >=
                                         KEEPALIVE_INTVL * MILLION))) {
        /* check whether the session is alive */
        if (sessp->keepalive_cnt >= KEEPALIVE_CNT) {
            sessp->keepalive_cnt = 0;
            printd(WARNING "The RTSP session isn't alive any longer!\n");
            return -1;
        }
        sessp->todo = RTSP_METHOD_OPTIONS;
        send_method_options(sessp);
        sessp->last_keepalive = now;
        sessp->keepalive_cnt++;
    }

    if (check_send_queue(sessp) < 0) {
        return -1;
    }

    /* Wait event notifications. */
    do {
        nfds = epoll_wait(sessp->ep_fd, sessp->ep_ev,
                          EPOLL_MAX_EVS, KEEPALIVE_INTVL * THOUSAND);
    } while (nfds < 0 && errno == EINTR);
    if (nfds < 0) {
        perrord(ERR "epoll_wait() for listen socket error");
        return -1;
    }

    /* time to teardown the session? */
    if (sessp->todo == RTSP_METHOD_TEARDOWN) {
        /* wait until all buffer in queue are sent out*/
        if (list_empty(&sessp->send_queue)) {
            sessp->enable = 0;
        }
    }

    /* Handle the sockets which there's any event occured. */
    for (i = 0; i < nfds; i++) {
        if ((sessp->ep_ev[i].events & EPOLLERR) ||
#ifdef EPOLLRDHUP
            (sessp->ep_ev[i].events & EPOLLRDHUP) ||
#endif
            (sessp->ep_ev[i].events & EPOLLHUP)) {
            printd(WARNING "epoll_wait() error occured on this fd[%d]\n",
                   sessp->ep_ev[i].events);
            continue;
        }

        if (do_sd_handler(sessp->ep_ev[i].data.fd, sessp->ep_ev[i].events, sessp) < 0) {
            printd(WARNING "Error occured when handling socket event!\n");
            return -1;
        }
    }

    return 0;
}

static void cleanup_before_reconn(struct rtsp_sess *sessp)
{
    int i = 0;

    sessp->rtsp_state = RTSP_STATE_INIT;
    sessp->handling_state = HANDLING_STATE_INIT;
    sessp->todo = RTSP_METHOD_NONE;
    sessp->cur_cseq = 0;
    sessp->sess_id = 0;

    close(sessp->ep_fd);
    sessp->ep_fd = -1;

    close(sessp->rtsp_sock.sd);
    sessp->rtsp_sock.sd = -1;

    for (i = 0; i < 2; i++) {
        close(sessp->rtp_rtcp[i].udp.rtp_sock.sd);
        sessp->rtp_rtcp[i].udp.rtp_sock.sd = -1;
        close(sessp->rtp_rtcp[i].udp.rtcp_sock.sd);
        sessp->rtp_rtcp[i].udp.rtcp_sock.sd = -1;

        sessp->rtp_rtcp[i].enable = 0;
    }

    for (i = 0; i < RTSP_METHOD_NUM; i++) {
        sessp->supported_method[i].supported = 0;
    }

    sessp->rtsp_state = RTSP_STATE_INIT;
    sessp->keepalive_cnt = 0;
    sessp->last_keepalive = 0;

    freez(sessp->sdp_info);
    return;
}

static void *rtsp_sess_thrd(void *arg)
{
    pthread_detach(pthread_self());
    entering_thread();

    struct rtsp_sess *sessp = (struct rtsp_sess *)arg;
    int ret = 0;
    int reconn = 0;

    do {
        reconn = 0;

        /* Create epoll file descriptor. */
        if ((sessp->ep_fd = epoll_create(MAX_FD_NUM)) < 0) {
            perrord(ERR "Create epoll file descriptor error");
            goto rtn;
        }

        /* Create socket for RTSP sessioin. */
        sessp->rtsp_sock.sd = socket(AF_INET, SOCK_STREAM, 0);
        if (sessp->rtsp_sock.sd < 0) {
            perrord(ERR "Create socket for RTSP session error");
            goto rtn;
        }

        /* Connect to RTSP server. */
        ret = connect_nonb(sessp->rtsp_sock.sd,
                           (struct sockaddr *)&sessp->srv_addr,
                           sizeof(sessp->srv_addr), CONN_TIMEOUT);
        if (ret < 0) {
            printd(INFO "Connect to RTSP server failed: %s, "
                   "reconnect after %d seconds ...\n", strerror(errno), RECONN_INTERVAL);
            cleanup_before_reconn(sessp);
            sleep(RECONN_INTERVAL);
            reconn = 1;
            continue;
        }

        if (set_block_mode(sessp->rtsp_sock.sd, 0) < 0) {
            goto rtn;
        }
        sessp->rtsp_sock.arg = sessp;
        sessp->rtsp_sock.handler = handle_rtsp_sd;
        sessp->rtsp_sock.ev = RTSP_SD_DFL_EV;
        if (monitor_sd_event(sessp->ep_fd, sessp->rtsp_sock.sd,
                             sessp->rtsp_sock.ev) < 0) {
            goto rtn;
        }

        /* The main loop of RTSP session. */
        while (sessp->enable) {
            if (reconn) {
                printd(INFO "Reconnect after %d seconds ...\n", RECONN_INTERVAL);
                cleanup_before_reconn(sessp);
                sleep(RECONN_INTERVAL);
                break;
            }
            if (single_step(sessp) < 0) {
                reconn = 1;
            }
        }
    } while (sessp->enable && reconn);

rtn:
    destroy_rtsp_sess(sessp);
    leaving_thread();
    return NULL;
}

struct rtsp_sess *create_rtsp_sess(char *uri, struct sockaddr_in *srv_addrp,
                                   struct chn_info *chnp, int intlvd)
{
    struct rtsp_sess *sessp = NULL;
    int ret = 0;
    int i = 0;

    sessp = mallocz(sizeof(*sessp));
    if (!sessp) {
        printd(EMERG "Allocate memory for struct rtsp_sess failed!\n");
        return NULL;
    }

    /* Initialize struct rtsp_sess. */
    sessp->rtsp_sock.sd = -1;
    sessp->ep_fd = -1;
    for (i = 0; i < 2; i++) {
        sessp->rtp_rtcp[i].udp.rtp_sock.sd = -1;
        sessp->rtp_rtcp[i].udp.rtcp_sock.sd = -1;
    }

    sessp->enable = 1;
    sessp->rtsp_state = RTSP_STATE_INIT;
    sessp->handling_state = HANDLING_STATE_INIT;
    sessp->todo = RTSP_METHOD_NONE;
    sessp->cur_cseq = 0;
    memcpy(&sessp->srv_addr, srv_addrp, sizeof(*srv_addrp));
    memcpy(&sessp->chn_info, chnp, sizeof(*chnp));
    sessp->intlvd_mode = intlvd;
    INIT_LIST_HEAD(&sessp->send_queue);

    strncpy(sessp->uri, uri, sizeof(sessp->uri) - 1);

    /* Allocate memory for epoll events. */
    sessp->ep_ev = calloc(EPOLL_MAX_EVS, sizeof(struct epoll_event));
    if (!sessp->ep_ev) {
        printd(EMERG "calloc() for epoll events failed!\n");
        return NULL;
    }

    /* Set all RTSP method un-supported by default. */
    for (i = 0; i < RTSP_METHOD_NUM; i++) {
        sessp->supported_method[i].method = i;
        sessp->supported_method[i].supported = 0;
    }

    /* Allocate memory for buffer storing incomplete data received last time. */
    sessp->last_data.buf = mallocz(RECV_BUF_SZ);
    if (!sessp->last_data.buf) {
        printd(EMERG "Allocate memory for receiving buffer failed!\n");
        freez(sessp);
        return NULL;
    }
    sessp->last_data.sz = 0;

    /* Allocate memory for buffer storing current frame. */
    sessp->frm_info.frm_buf = mallocz(MAX_FRM_SZ);
    if (!sessp->frm_info.frm_buf) {
        printd(EMERG "Allocate memory for storing frame failed!\n");
        freez(sessp->last_data.buf);
        freez(sessp);
        return NULL;
    }

    /* Create thread for each RTSP session. */
    if ((ret = pthread_create(&sessp->rtsp_sess_tid, NULL,
                              rtsp_sess_thrd, sessp)) != 0) {
        printd(EMERG "Create thread rtsp_sess_thrd error: %s\n", strerror(ret));
        freez(sessp->last_data.buf);
        freez(sessp->frm_info.frm_buf);
        freez(sessp);
        return NULL;
    }

    /* Add to RTSP session list. */
    pthread_mutex_lock(&rtsp_cli.list_mutex);
    list_add_tail(&sessp->entry, &rtsp_cli.rtsp_sess_list);
    pthread_mutex_unlock(&rtsp_cli.list_mutex);

    return sessp;
}

void destroy_rtsp_sess(struct rtsp_sess *sessp)
{
    int found = 0;
    struct rtsp_sess *tmp = NULL;

    list_for_each_entry(tmp, &rtsp_cli.rtsp_sess_list, entry) {
        if (tmp == sessp) {
            found = 1;
            break;
        }
    }

    if (!found) {
        return;
    }

    pthread_mutex_lock(&rtsp_cli.list_mutex);
    list_del(&sessp->entry);
    pthread_mutex_unlock(&rtsp_cli.list_mutex);

    close(sessp->rtsp_sock.sd);
    close(sessp->ep_fd);

    freez(sessp->sdp_info);
    freez(sessp->ep_ev);
    freez(sessp->frm_info.frm_buf);
    freez(sessp->last_data.buf);
    freez(sessp);
    return;
}

struct rtsp_req *alloc_rtsp_req(enum rtsp_method method, unsigned int cseq)
{
    struct rtsp_req *req = NULL;

    /* RTSP request */
    req = mallocz(sizeof(*req));
    if (!req) {
        printd(ERR "Allocate memory for struct rtsp_req failed!\n");
        return NULL;
    }

    /* RTSP request line. */
    req->req_line.method = method;
    memcpy(req->req_line.ver, RTSP_VER, sizeof(req->req_line.ver) - 1);

    /* RTSP request headers. */
    req->req_hdr.cseq = cseq;
    snprintf(req->req_hdr.usr_agent, sizeof(req->req_hdr.usr_agent), "ZMODO RTSP client");

    return req;
}

void free_rtsp_req(struct rtsp_req *req)
{
    freez(req);
}

static int run_rtsp_state_machine(struct rtsp_sess *sessp, struct rtsp_resp *resp)
{
    int i = 0;

    if (resp->resp_hdr.cseq != sessp->cur_cseq) {
        printd(WARNING "The CSeq of response not matches the request one.\n");
        return -1;
    }
    if (resp->resp_hdr.sess_id) {
        sessp->sess_id = resp->resp_hdr.sess_id;
    }

    switch (sessp->todo) {
    case RTSP_METHOD_OPTIONS:
        /* for keepalive message */
        sessp->keepalive_cnt = 0;

        for (i = 0; i < RTSP_METHOD_NUM; i++) {
            if (strstr(resp->resp_hdr.public, rtsp_method_tkn[i])) {
                sessp->supported_method[i].supported = 1;
            }
        }
        break;
    case RTSP_METHOD_DESCRIBE:
        sessp->sdp_info = resp->sdp_info;
        break;
    case RTSP_METHOD_SETUP:
        /* All media sessions were setup? */
        for (i = 0; i < 2; i++) {
            if (sessp->sdp_info->sdp_m[i].enable) {
                if (!sessp->rtp_rtcp[i].enable) {
                    break;
                }
            }
        }
        if (i == 2) {
            sessp->rtsp_state = RTSP_STATE_READY;
        }
        
        if (!sessp->intlvd_mode) {
            struct sockaddr_in *tmp_sap = NULL;
            for (i = 0; i < 2; i++) {
                if (resp->resp_hdr.transport.rtp_cli_port ==
                    sessp->rtp_rtcp[i].udp.rtp_port) {
                    tmp_sap = (struct sockaddr_in *)&sessp->rtp_rtcp[i].udp.rtp_sa;
                    tmp_sap->sin_port = htons(resp->resp_hdr.transport.rtp_srv_port);
                    tmp_sap = (struct sockaddr_in *)&sessp->rtp_rtcp[i].udp.rtcp_sa;
                    tmp_sap->sin_port = htons(resp->resp_hdr.transport.rtcp_srv_port);
                    break;
                }
            }
            if (i == 2) {
                printd("RTP/RTCP port of client in request and response is unmatched!\n");
                return -1;
            }
        }
        break;
    case RTSP_METHOD_PLAY:
        sessp->rtsp_state = RTSP_STATE_PLAYING;
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
        printd("Unsupported RTSP method[%d]!\n", sessp->todo);
        return -1;
    }

    return 0;
}

/**
 * Parse the RTSP response message, and run the RTSP state machine.
 */
int handle_rtsp_resp(struct rtsp_sess *sessp, const char *msg, unsigned int sz)
{
    struct rtsp_resp *resp = NULL;

    resp = mallocz(sizeof(*resp));
    if (!resp) {
        printd("Allocate memory for struct rtsp_resp failed!\n");
        return -1;
    }

    parse_rtsp_resp(sessp, resp, msg, sz);

    run_rtsp_state_machine(sessp, resp);

    sessp->handling_state = HANDLING_STATE_INIT;

    freez(resp);
    return 0;
}
