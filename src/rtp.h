/*********************************************************************
 * File Name    : rtp.h
 * Description  : 
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-26
 ********************************************************************/

#ifndef __RTP_H__
#define __RTP_H__


/* NALU payload type. */
enum {
    NALU_PT_FU_A = 28,
    NALU_PT_FU_B = 29,
};

/* RTP payload type. */
enum rtp_pt {
    RTP_PT_H264 = 96,
    RTP_PT_PCMU = 0,
    RTP_PT_PCMA = 8,
};

/* media type */
enum media_type {
    MEDIA_TYPE_VIDEO,
    MEDIA_TYPE_AUDIO,
};


struct rtsp_sess;
int handle_rtp_pkt(struct rtsp_sess *sessp, enum media_type media,
                   char *data, unsigned int sz);



#endif /* __RTP_H__ */

