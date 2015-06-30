/*********************************************************************
 * File Name    : rtp.c
 * Description  :
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-26
 ********************************************************************/

#include "rtsp_cli.h"
#include "rtp.h"
#include "log.h"


int handle_rtp_pkt(struct rtsp_sess *sessp, enum media_type media,
                   char *data, unsigned int sz)
{
    struct rtp_hdr *hdrp = NULL;
    char *pl = NULL;            /* RTP plyload */
    char nalu_pt = 0;           /* payload type */
    char start_code[4] = {0, 0, 0, 1}; /* start code of NALU */
    struct frm_info *frmp = &sessp->frm_info;
    char nalu_hdr = 0;
    enum frm_type frm_type;
    char *frm_buf = frmp->frm_buf + sessp->chn_info.frm_hdr_sz;

    hdrp = (struct rtp_hdr *)data;
    pl = data + sizeof(*hdrp);

    switch (hdrp->pt) {
    case RTP_PT_H264:
        nalu_pt = pl[0] & 0x1F; /* first byte in payload & 0x1F */
        switch (nalu_pt) {
        case 1 ... 23:          /* single NALU Packet. */
            nalu_hdr = pl[0];
            frm_type = ((nalu_hdr & 0x1F) == 0x01) ? FRM_TYPE_PF : FRM_TYPE_IF;
            printd("start code--------[cseq = %d]------>sz = %d\n", ntohs(hdrp->seq), sz);
            memcpy(frm_buf + frmp->frm_sz, &start_code, sizeof(start_code));
            frmp->frm_sz += sizeof(start_code);
            memcpy(frm_buf + frmp->frm_sz, pl, sz - sizeof(*hdrp));
            frmp->frm_sz += sz - sizeof(*hdrp);
            break;
        case NALU_PT_FU_A:
            nalu_hdr = (pl[0] & 0xE0) | (pl[1] & 0x1F);
            frm_type = ((nalu_hdr & 0x1F) == 0x01) ? FRM_TYPE_PF : FRM_TYPE_IF;
            if (pl[1] & 0x80) { /* first segment in NALU */
                printd("start code--------[cseq = %d]------>sz = %d\n", ntohs(hdrp->seq), sz);
                memcpy(frm_buf + frmp->frm_sz, &start_code, sizeof(start_code));
                frmp->frm_sz += sizeof(start_code);
                frm_buf[frmp->frm_sz] = nalu_hdr;
                frmp->frm_sz += 1;
            }
            memcpy(frm_buf + frmp->frm_sz, pl + 2, sz - sizeof(*hdrp) - 2);
            frmp->frm_sz += sz - sizeof(*hdrp) - 2;
            break;
        default:
            printd("Unsupported or undefined NALU payload type[%d]\n", nalu_pt);
            break;
        }
        break;
    case RTP_PT_PCMA:
    case RTP_PT_PCMU:
        frm_type = FRM_TYPE_AF;
        break;
    default:
        printd("Unsupported or undefined RTP payload type[%d]\n", nalu_pt);
        break;
    }

    if (hdrp->m) {              /* last nalu of a frame */
        frmp->frm_type = frm_type;
        rtsp_cli.store_frm(&sessp->chn_info, &sessp->frm_info);
        frmp->frm_sz = 0;
    }

    return 0;
}
