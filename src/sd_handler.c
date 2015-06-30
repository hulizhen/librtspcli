/*********************************************************************
 * File Name    : sd_handler.c
 * Description  : Handlers of socket descriptors and received data.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-24
 ********************************************************************/

#include <sys/epoll.h>
#include "log.h"
#include "util.h"
#include "rtsp_cli.h"
#include "sd_handler.h"
#include "send_queue.h"
#include "rtp.h"
#include "rtcp.h"


/**
 * Handle the received data in interleaved mode.
 */
static int handle_intlvd_data(struct rtsp_sess *sessp, char *data, unsigned sz)
{
    struct intlvd *intlvdp = NULL;

    if (data[0] == '$') {       /* RTP/RTCP packet */
        if (sz > sizeof(*intlvdp)) {
            intlvdp = (struct intlvd *)data;
            if (ntohs(intlvdp->sz) == (sz - sizeof(*intlvdp))) {
                switch (intlvdp->chn) {
                case INTLVD_CHN_RTP_V:
                    handle_rtp_pkt(sessp, MEDIA_TYPE_VIDEO,
                                   data + sizeof(*intlvdp),
                                   sz - sizeof(*intlvdp));
                    break;
                case INTLVD_CHN_RTP_A:
                    handle_rtp_pkt(sessp, MEDIA_TYPE_AUDIO,
                                   data + sizeof(*intlvdp),
                                   sz - sizeof(*intlvdp));
                    break;
                case INTLVD_CHN_RTCP_V:
                    handle_rtcp_pkt(sessp, MEDIA_TYPE_VIDEO,
                                    data + sizeof(*intlvdp),
                                    sz - sizeof(*intlvdp));
                    break;
                case INTLVD_CHN_RTCP_A:
                    handle_rtcp_pkt(sessp, MEDIA_TYPE_AUDIO,
                                    data + sizeof(*intlvdp),
                                    sz - sizeof(*intlvdp));
                    break;
                }
            } else {
#if 1
                printd("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx-----bbb--------> ntohs(intlvdp->sz) = %d, (sz - sizeof(*intlvdp)) = %d\n",
                       ntohs(intlvdp->sz), (sz - sizeof(*intlvdp)));
#include <assert.h>
                assert(ntohs(intlvdp->sz) == (sz - sizeof(*intlvdp)));
#endif
            }
        }
    } else {                    /* RTSP response message */
        handle_rtsp_resp(sessp, data, sz);
    }

    return 0;
}

/**
 * @brief: Check whether it's a complete RTSP message.
 *
 * Return 1 if complete, 0 if incomplete.
 */
static int rtsp_msg_complete(char *data, unsigned sz)
{
    int complete = 0;

    if (sz < 4) {
        return complete;
    }
    
    if (data[sz - 2] == '\r' && data[sz - 1] == '\n') {
        complete = 1;
    } else {
        complete = 0;
    }
    return complete;
}

/**
 * Consider about the interleaved mode, we have to
 * filter out the RTP & RTCP packet.
 */
static int recv_from_rtsp_sd(struct rtsp_sess *sessp)
{
    ssize_t nr = 0;             /* bytes recv()ed. */
    char recv_buf[RECV_BUF_SZ] = {0};
    int rtsp_sd = sessp->rtsp_sock.sd;
    struct last_data *lastp = &sessp->last_data;
    struct intlvd *intlvdp = NULL;
    char *ptr = NULL;
    char *new = NULL;           /* New message block(RTSP/RTP/RTCP message) start */
    unsigned left = 0;

    nr = recv(rtsp_sd, recv_buf, RECV_BUF_SZ, 0);
    if (nr <= 0) {
        if (nr < 0) {
            perrord(ERR "recv() from rtsp_sd error");
        }
        return -1;
    }

    ptr = recv_buf;
    left = nr;
    new = ptr;

    /* handle the data in last buf */
    if (lastp->sz >= sizeof(*intlvdp) && lastp->buf[0] == '$') {
        intlvdp = (struct intlvd *)lastp->buf;
        if (left >= ntohs(intlvdp->sz) + sizeof(*intlvdp) - lastp->sz) {
            memcpy(lastp->buf + lastp->sz, ptr, ntohs(intlvdp->sz) + sizeof(*intlvdp) - lastp->sz);
            printd("hulz--------->test: lastp->buf[0] = %c, lastp->sz = %d, handle_sz = %d\n", lastp->buf[0], lastp->sz, ntohs(intlvdp->sz) + sizeof(*intlvdp));
            handle_intlvd_data(sessp, lastp->buf, ntohs(intlvdp->sz) + sizeof(*intlvdp));
            ptr += ntohs(intlvdp->sz) + sizeof(*intlvdp) - lastp->sz;
            left -= ntohs(intlvdp->sz) + sizeof(*intlvdp) - lastp->sz;
            lastp->sz = 0;
            new = ptr;
        } else {
            memcpy(lastp->buf + lastp->sz, ptr, left);
            lastp->sz += left;
            return 0;
        }
    }

    while (left > 0) {
        if (ptr[0] == '$') {
            if (ptr != new) {
                memcpy(lastp->buf + lastp->sz, new, ptr - new);
                lastp->sz += ptr - new;
                lastp->buf[lastp->sz] = 0;
                printd("hulz--------->test: lastp->buf[0] = %c, lastp->sz = %d, handle_sz = %d\n", lastp->buf[0], lastp->sz, lastp->sz);
                handle_intlvd_data(sessp, lastp->buf, lastp->sz);
                lastp->sz = 0;
                new = ptr;
            }

            /* handle the new RTP/RTCP packet */
            intlvdp = (struct intlvd *)new;
            if (left >= sizeof(*intlvdp) &&
                (left >= sizeof(*intlvdp) + ntohs(intlvdp->sz))) {
                printd("hulz--------->test: new[0] = %c, handle_sz = %d\n", new[0], sizeof(*intlvdp) + ntohs(intlvdp->sz));
                handle_intlvd_data(sessp, new, sizeof(*intlvdp) + ntohs(intlvdp->sz));
                ptr += sizeof(*intlvdp) + ntohs(intlvdp->sz);
                left -= sizeof(*intlvdp) + ntohs(intlvdp->sz);
                new = ptr;
            } else {
                memcpy(lastp->buf, new, left);
                lastp->sz = left;
                left = 0;
            }
        } else {
            ptr++;
            left--;

            if (!left) {
                memcpy(lastp->buf + lastp->sz, new, ptr - new);
                lastp->sz += ptr - new;
                if (rtsp_msg_complete(lastp->buf, lastp->sz)) {
                    lastp->buf[lastp->sz] = 0;
                    printd("hulz--------->test: lastp->buf[0] = %c, lastp->sz = %d, handle_sz = %d\n", lastp->buf[0], lastp->sz, lastp->sz);
                    handle_intlvd_data(sessp, lastp->buf, lastp->sz);
                    lastp->sz = 0;
                } else {
                    lastp->sz += ptr - new;
                }
            }
        }
    }

    return 0;
}

int handle_rtsp_sd(int sd, int ev, void *arg)
{
    struct rtsp_sess *sessp = (struct rtsp_sess *)arg;
    enum data_type type = 0;

    if (ev & EPOLLIN) {
        if (recv_from_rtsp_sd(sessp) < 0) {
            return -1;
        }
    }

    if (ev & EPOLLOUT) {
        type = sessp->intlvd_mode   ?
            (DATA_TYPE_RTSP_REQ     |
             DATA_TYPE_RTCP_V_PKT   |
             DATA_TYPE_RTCP_A_PKT)  :
            DATA_TYPE_RTSP_REQ;
        if (consume_send_buf(sessp, type) < 0) {
            return -1;
        }
    }

    return 0;
}

int handle_rtp_sd(int sd, int ev, void *arg)
{
    struct rtsp_sess *sessp = (struct rtsp_sess *)arg;
    ssize_t nr = 0;
    int i = 0;
    char recv_buf[RECV_BUF_SZ] = {0};
    enum media_type media = 0;

    for (i = 0; i < 2; i++) {
        if (sd == sessp->rtp_rtcp[i].udp.rtp_sock.sd) {
            media = i;
            break;
        }
    }
    if (i == 2) {
        printd(ERR "Unmatched socket descriptor!\n");
        return -1;
    }

    if (ev & EPOLLIN) {
        nr = recvfrom(sd, recv_buf, sizeof(recv_buf), 0, NULL, 0);
        if (nr <= 0) {
            if (nr < 0) {
                perrord(ERR "recvfrom() rtp_sd error");
            }
            return -1;
        }
        handle_rtp_pkt(sessp, media, recv_buf, nr);
    }
    return 0;
}

int handle_rtcp_sd(int sd, int ev, void *arg)
{
    return 0;
}

int do_sd_handler(int sd, int ev, struct rtsp_sess *sessp)
{
    int i = 0;
    struct sock *sockp = NULL;

    if (sd == sessp->rtsp_sock.sd) {
        sockp = &sessp->rtsp_sock;
    } else {
        for (i = 0; i < 2; i++) {
            if (sd == sessp->rtp_rtcp[i].udp.rtp_sock.sd) {
                sockp = &sessp->rtp_rtcp[i].udp.rtp_sock;
            } else if (sd == sessp->rtp_rtcp[i].udp.rtcp_sock.sd) {
                sockp = &sessp->rtp_rtcp[i].udp.rtcp_sock;
            }
        }
    }

    if (sockp) {
        if (sockp->handler(sd, ev, sessp) < 0) {
            return -1;
        }
    }
    return 0;
}
