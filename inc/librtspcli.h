/*********************************************************************
 * File Name    : librtspcli.h
 * Description  : Interface to user layer.
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-21
 ********************************************************************/

#ifndef __LIBRTSPCLI_H__
#define __LIBRTSPCLI_H__


#if defined (__cplusplus) || defined (_cplusplus)
extern "C" {
#endif

#define MAX_FRM_SZ      (1024 * 1024) /* max frame size */
#define MAX_CHN_NUM     8             /* max channel number */
#define DFL_RTSP_PORT   10554

#define MAX_FD_NUM      1024

/* frame type */
enum frm_type {
    FRM_TYPE_IF = 1,
    FRM_TYPE_BF,
    FRM_TYPE_PF,
    FRM_TYPE_AF,
};

/* channel type */
enum chn_type {
    CHN_TYPE_MAIN,
    CHN_TYPE_MINOR,
};


/*
 * channel information:
 * @chn_no:     remote channel number.
 * @chn_type:   main channel or minor channel.
 * @frm_hdr_sz: used to reserve some space for storing
 *              frame header in user level.
 * @usr_data:   store user data, may be used when nessary.
 */
struct chn_info {
    int local_chn;              /* local channel number */
    unsigned frm_hdr_sz;
    void *usr_data;
};

/* frame information used for getting frame */
struct frm_info {
    char *frm_buf;              /* frame buffer: store pure av data */
    unsigned frm_sz;        /* frame size */
    enum frm_type frm_type;     /* frame type */
};

/**
 * @breif: Start thread and allocate resource for opening remote channel.
 *
 * @uri:        RTSP URI 
 * 
 * @intlvd:     choose the way to transfer the RTP packet.
 *              1: interleaved mode(TCP); 0: non-interleaved mode(UDP).
 *
 * @chnp:       channel information
 *              
 * Return one user ID when we start thread for opening
 * remote channel successfully, return zero when failed.
 */
unsigned long open_chn(char *uri, struct chn_info *chnp, int intlvd);

/**
 * @breif: stop thread and free resource for opening remote channel.
 *
 * @usr_id: the value returned by open_chn().
 */
void close_chn(unsigned long usr_id);

/**
 * @breif: tests whether the specified channel is playing
 *         video/audio stream.
 *         
 * @usr_id: the value returned by open_chn().
 */
int chn_playing(unsigned long usr_id);

/**
 * @breif: we will call this callback function when prepare one
 *         completed frame(just pure av data without frame header).
 */
typedef int (*store_frm_t)(struct chn_info *chnp, struct frm_info *frmp);

int init_rtsp_cli(store_frm_t store_frm);
void deinit_rtsp_cli(void);


#if defined (__cplusplus) || defined (_cplusplus)
}
#endif


#endif /* __LIBRTSPCLI_H__ */
