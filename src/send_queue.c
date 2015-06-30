/*********************************************************************
 * File Name    : send_queue.c
 * Description  : Implementation of send buffer queue.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-25
 ********************************************************************/

#include <sys/epoll.h>
#include "log.h"
#include "util.h"
#include "list.h"
#include "send_queue.h"
#include "rtsp_cli.h"


/**
 * Check whether there's any data to be sent in send queue.
 */
int check_send_queue(struct rtsp_sess *sessp)
{
    struct send_buf *sendp = NULL;
    struct sock *sockp = NULL;

    if (!list_empty(&sessp->send_queue)) {
        sendp = list_first_entry(&sessp->send_queue, struct send_buf, entry);

        if (sessp->intlvd_mode || sendp->type == DATA_TYPE_RTSP_REQ) {
            sockp = &sessp->rtsp_sock;
        } else {
            switch (sendp->type) {
            case DATA_TYPE_RTCP_V_PKT:
                sockp = &sessp->rtp_rtcp[MEDIA_TYPE_VIDEO].udp.rtcp_sock;
                break;
            case DATA_TYPE_RTCP_A_PKT:
                sockp = &sessp->rtp_rtcp[MEDIA_TYPE_AUDIO].udp.rtcp_sock;
                break;
            default:
                printd("Wrong data type[%d]\n", sendp->type);
                break;
            }
        }

        sockp->ev |= EPOLLOUT;
        if (update_sd_event(sessp->ep_fd, sockp->sd, sockp->ev) < 0) {
            return -1;
        }
    }
    return 0;
}

static ssize_t send_buf_data(struct rtsp_sess *sessp, struct send_buf *sendp)
{
    ssize_t ns = -1;             /* Bytes sent.  */
    struct sock *sockp = NULL;
    struct sockaddr *sap = NULL;

    if (sessp->intlvd_mode || sendp->type == DATA_TYPE_RTSP_REQ) {
        sockp = &sessp->rtsp_sock;
        ns = send(sockp->sd, sendp->buf, sendp->sz, 0);
        if (ns < 0 || ns != sendp->sz) {
            printd(WARNING "ns[%d] != sendp->sz[%d]\n", ns, sendp->sz);
            if (ns < 0) {
                perrord(ERR "send buffer data error");
            }
        }
    } else {
        switch (sendp->type) {
        case DATA_TYPE_RTCP_V_PKT:
            sockp = &sessp->rtp_rtcp[MEDIA_TYPE_VIDEO].udp.rtcp_sock;
            sap = &sessp->rtp_rtcp[MEDIA_TYPE_VIDEO].udp.rtcp_sa;
            break;
        case DATA_TYPE_RTCP_A_PKT:
            sockp = &sessp->rtp_rtcp[MEDIA_TYPE_AUDIO].udp.rtcp_sock;
            sap = &sessp->rtp_rtcp[MEDIA_TYPE_AUDIO].udp.rtcp_sa;
            break;
        default:
            return -1;
        }
        ns = sendto(sockp->sd, sendp->buf, sendp->sz, 0, sap, sizeof(*sap));
    }

    if (sockp) {
        sockp->ev &= ~EPOLLOUT;
        if (update_sd_event(sessp->ep_fd, sockp->sd, sockp->ev) < 0) {
            return -1;
        }
    }
    return ns;
}

int consume_send_buf(struct rtsp_sess *sessp, enum data_type type)
{
    struct send_buf *sendp = NULL;
    int ret = 0;

    if (!list_empty(&sessp->send_queue)) {
        sendp = list_first_entry(&sessp->send_queue, struct send_buf, entry);
        if (sendp->type & type) {
            ret = send_buf_data(sessp, sendp);
            list_del(&sendp->entry);
            free_send_buf(sendp);
            if (ret < 0) {
                return -1;
            }
        }
    }

    return 0;
}

struct send_buf *alloc_send_buf(enum data_type type, unsigned int sz)
{
    struct send_buf *sendp = NULL;

    sendp = mallocz(sizeof(*sendp));
    if (!sendp) {
        printd("Allocate memory for sendp failed!\n");
        return NULL;
    }

    sendp->sz = sz;
    sendp->type = type;
    sendp->buf = mallocz(sz);
    if (!sendp->buf) {
        printd("Allocate memory for sendp->buf failed!\n");
        freez(sendp);
        return NULL;
    }
    return sendp;
}

void free_send_buf(struct send_buf *sendp)
{
    freez(sendp->buf);
    freez(sendp);
    return;
}
