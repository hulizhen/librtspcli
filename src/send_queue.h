/*********************************************************************
 * File Name    : send_queue.h
 * Description  : Implementation of send buffer queue.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-25
 ********************************************************************/

#ifndef __SEND_QUEUE_H__
#define __SEND_QUEUE_H__


enum data_type {
    DATA_TYPE_RTSP_REQ      = 0x01, /* RTSP request */
    DATA_TYPE_RTCP_V_PKT    = 0x02, /* RTP packet of video */
    DATA_TYPE_RTCP_A_PKT    = 0x04, /* RTP packet of audio */
};

/*
 * Each RTSP session contains one buffer queue.
 * And each buffer queue links the nodes below.
 */
struct send_buf {
    struct list_head entry;          /* entry of send queue */
    enum data_type type;             /* RTSP, RTCP, RTP(I/P/A frame) */
    unsigned int sz;                 /* buffer size */
    char *buf;                       /* buffer for storing raw data */
};


struct send_buf *alloc_send_buf(enum data_type type, unsigned int sz);
void free_send_buf(struct send_buf *sendp);

struct rtsp_sess;
int check_send_queue(struct rtsp_sess *sessp);
int consume_send_buf(struct rtsp_sess *sessp, enum data_type type);


#endif /* __SEND_QUEUE_H__ */

