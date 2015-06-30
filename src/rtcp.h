/*********************************************************************
 * File Name    : rtcp.h
 * Description  : 
 * Author       : Hu Lizhen
 * Create Date  : 2012-12-26
 ********************************************************************/

#ifndef __RTCP_H__
#define __RTCP_H__


int handle_rtcp_pkt(struct rtsp_sess *sessp, enum media_type media,
                    char *data, unsigned int sz);



#endif /* __RTCP_H__ */

